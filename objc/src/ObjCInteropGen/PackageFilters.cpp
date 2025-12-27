// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "PackageConfig.h"

#include <iostream>
#include <regex>

#include "FatalException.h"
#include "Logging.h"

namespace objcgen {

[[nodiscard]] static auto create_regex(const Package& package, const std::string& pattern, std::string_view mode_name)
{
    try {
        return std::regex(pattern);
    } catch (const std::regex_error& e) {
        fatal("`packages` entry `", package.cangjie_name(), "` ", mode_name, " filter (`", pattern,
            "`) has thrown an error: ", describe_regex_error(e.code()));
    }
}

class RegexFilter final : public PackageFilter {
    const std::string pattern_;
    const std::regex regex_;
    const std::string_view mode_name_; // "include"/"exclude"/"filter"/"filter-not", only for better diagnostics

public:
    explicit RegexFilter(const Package& package, std::string pattern, const std::string_view mode_name)
        : PackageFilter(package),
          pattern_(std::move(pattern)),
          regex_(create_regex(package, pattern_, mode_name)),
          mode_name_(mode_name)
    {
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        try {
            const auto match = std::regex_match(entity_name.begin(), entity_name.end(), regex_);

            if (verbosity >= LogLevel::TRACE) {
                std::cerr << "`packages` entry `" << package_name() << "` " << mode_name_ << " filter (`" << pattern_
                          << "`) does" << (match ? "" : " NOT") << " match entity `" << entity_name << '`' << std::endl;
            }

            return match;
        } catch (std::regex_error& e) {
            fatal("`packages` entry `", package_name(), "` ", mode_name_, " filter (`", pattern_,
                "`) has thrown an error: ", describe_regex_error(e.code()));
        }
    }
};

class NotFilter final : public PackageFilter {
    const PackageFilter& filter_;

public:
    NotFilter(const Package& package, const PackageFilter& filter) noexcept : PackageFilter(package), filter_(filter)
    {
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        return !filter_.apply(entity_name);
    }
};

class SetOperationFilter : public PackageFilter {
public:
    [[nodiscard]] bool empty() const noexcept
    {
        return arguments_.empty();
    }

    void add_argument(const PackageFilter& arg)
    {
        arguments_.push_back(&arg);
    }

protected:
    std::vector<const PackageFilter*> arguments_;

    SetOperationFilter(const Package& package) noexcept : PackageFilter(package)
    {
    }
};

/** aka "ANY", "OR" */
class UnionFilter final : public SetOperationFilter {
public:
    static constexpr auto mode_name = "union";

    UnionFilter(const Package& package) noexcept : SetOperationFilter(package)
    {
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        for (const auto* argument : arguments_) {
            if (argument->apply(entity_name)) {
                return true;
            }
        }
        return false;
    }
};

/** aka "ALL", "AND" */
class IntersectionFilter final : public SetOperationFilter {
public:
    static constexpr auto mode_name = "intersect";

    IntersectionFilter(const Package& package) noexcept : SetOperationFilter(package)
    {
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        for (const auto* argument : arguments_) {
            if (!argument->apply(entity_name)) {
                return false;
            }
        }
        return true;
    }
};

[[nodiscard]] static PackageFilter& create_regex_filter(
    const Package& package, const toml::node& node, std::string_view mode_name)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry `" << package.cangjie_name() << "` " << mode_name << " filter" << std::endl;
    }

    if (auto* node_string = node.as_string()) {
        if (const auto node_value = node_string->value_exact<std::string>()) {
            return *new RegexFilter(package, *node_value, mode_name);
        }

        fatal("`packages` entry `", package.cangjie_name(), "` ", mode_name, " filter has no string value");
    }

    if (auto* node_array = node.as_array()) {
        auto& result = *new UnionFilter(package);

        std::size_t i = 0;
        for (auto&& item_any : *node_array) {
            if (auto* item_string = item_any.as_string()) {
                if (auto item_value = item_string->value_exact<std::string>()) {
                    result.add_argument(*new RegexFilter(package, *item_value, mode_name));
                } else {
                    fatal("`packages` entry `", package.cangjie_name(), "` ", mode_name, " filter #", i,
                        " has no string value");
                }
            } else {
                fatal("`packages` entry `", package.cangjie_name(), "` ", mode_name, " filter #", i,
                    " must be a TOML string");
            }
            i++;
        }

        if (result.empty()) {
            fatal("`packages` entry `", package.cangjie_name(), "` ", mode_name, " filter array has no items");
        }

        return result;
    }

    fatal("`packages` entry `", package.cangjie_name(), "` ", mode_name,
        " filter must be a TOML string or an array of TOML strings");
}

template <class SetOperationFilter>
[[nodiscard]] static SetOperationFilter& create_set_filter(const Package& package, const toml::node& node)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry `" << package.cangjie_name() << "` " << SetOperationFilter::mode_name
                  << " filter" << std::endl;
    }

    auto& result = *new SetOperationFilter(package);

    if (auto* node_array = node.as_array()) {
        std::size_t i = 0;
        for (auto&& item_any : *node_array) {
            if (auto* item_table = item_any.as_table()) {
                result.add_argument(create_filter(package, *item_table));
            } else {
                fatal("`packages` entry `", package.cangjie_name(), "` ", SetOperationFilter::mode_name, " filter #", i,
                    " must be a TOML table");
            }
            i++;
        }
    } else {
        fatal("`packages` entry `", package.cangjie_name(), "` ", SetOperationFilter::mode_name,
            " filter must be an array of filters");
    }

    if (result.empty()) {
        fatal("`packages` entry `", package.cangjie_name(), "` ", SetOperationFilter::mode_name,
            " filter array has no items");
    }

    return result;
}

PackageFilter& create_filter(const Package& package, const toml::table& table)
{
    auto* include = table.get("include");
    size_t non_null = include ? 1 : 0;
    auto* exclude = table.get("exclude");
    if (exclude) {
        ++non_null;
    }
    auto* set_union = table.get("union");
    if (set_union) {
        ++non_null;
    }
    auto* set_intersect = table.get("intersect");
    if (set_intersect) {
        ++non_null;
    }
    auto* set_not = table.get("not");
    if (set_not) {
        ++non_null;
    }

    auto* filter = table.get("filter");
    auto* filter_not = table.get("filter-not");

    switch (non_null) {
        case 0:
            fatal("`packages` entry `", package.cangjie_name(),
                "` filter has no specified operations (like include, union, etc)");
        case 1: {
            PackageFilter* result;
            if (include) {
                result = &create_regex_filter(package, *include, "include");
            } else if (exclude) {
                result = new NotFilter(package, create_regex_filter(package, *exclude, "exclude"));
            } else if (set_union) {
                result = &create_set_filter<UnionFilter>(package, *set_union);
            } else if (set_intersect) {
                result = &create_set_filter<IntersectionFilter>(package, *set_intersect);
            } else {
                assert(set_not);
                auto* set_not_table = set_not->as_table();
                if (!set_not_table) {
                    fatal("`packages` entry `", package.cangjie_name(), "` not filter must be a TOML table");
                }
                result = new NotFilter(package, create_filter(package, *set_not_table));
            }

            if (!filter && !filter_not) {
                return *result;
            }

            auto& intersect = *new IntersectionFilter(package);
            intersect.add_argument(*result);

            if (filter) {
                intersect.add_argument(create_regex_filter(package, *filter, "filter"));
            }

            if (filter_not) {
                intersect.add_argument(
                    *new NotFilter(package, create_regex_filter(package, *filter_not, "filter-not")));
            }

            return intersect;
        }
        default:
            fatal("`packages` entry `", package.cangjie_name(), "` filter has ", non_null,
                " operations, but only 1 is allowed simultaneously");
    }
}

} // namespace objcgen
