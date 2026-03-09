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

class RegexFilter final : public PackageFilter {
    std::string pattern_;
    std::regex regex_;
    std::string_view mode_name_; // "include"/"exclude"/"filter"/"filter-not", only for better diagnostics

public:
    explicit RegexFilter(const Package* package, const std::string& pattern, const std::string_view mode_name)
        : PackageFilter(package), pattern_(pattern), mode_name_(mode_name)
    {
        try {
            regex_ = std::regex(pattern);
        } catch (std::regex_error& e) {
            fatal("`packages` entry `", package_name(), "` ", mode_name_, " filter (`", pattern,
                "`) has thrown an error: ", describe_regex_error(e.code()));
        }
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        try {
            const auto match = std::regex_match(entity_name.begin(), entity_name.end(), regex_);

            if (verbosity >= LogLevel::TRACE) {
                std::cerr << "`packages` entry `" << package_name() << "` " << mode_name_ << " filter (`" << pattern_
                          << "`) does" << (match ? "" : " NOT") << " match entity `" << entity_name << "`" << std::endl;
            }

            return match;
        } catch (std::regex_error& e) {
            fatal("`packages` entry `", package_name(), "` ", mode_name_, " filter (`", pattern_,
                "`) has thrown an error: ", describe_regex_error(e.code()));
        }
    }
};

class NotFilter final : public PackageFilter {
    const PackageFilter* filter_;

public:
    NotFilter(const Package* package, const PackageFilter* filter) : PackageFilter(package), filter_(filter)
    {
        assert(filter);
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        return !filter_->apply(entity_name);
    }
};

enum class SetOperation : std::uint8_t {
    Union,        // aka "ANY", "OR"
    Intersection, // aka "ALL", "AND"
};

class SetOperationFilter final : public PackageFilter {
    const SetOperation op_;
    std::vector<const PackageFilter*> arguments_;

public:
    SetOperationFilter(const Package* package, const SetOperation op) : PackageFilter(package), op_(op)
    {
    }

    [[nodiscard]] std::size_t size() const
    {
        return arguments_.size();
    }

    void add_argument(const PackageFilter* arg)
    {
        assert(arg);
        arguments_.push_back(arg);
    }

    [[nodiscard]] bool apply(const std::string_view entity_name) const override
    {
        const std::size_t size = arguments_.size();

        for (std::size_t i = 0; i < size; ++i) {
            auto* arg = arguments_[i];
            const auto match = arg->apply(entity_name);

            switch (op_) {
                case SetOperation::Union:
                    if (match)
                        return true;
                    break;
                case SetOperation::Intersection:
                    if (!match)
                        return false;
                    break;
                default:
                    assert(false);
                    break;
            }
        }

        switch (op_) {
            case SetOperation::Union:
                return false;
            case SetOperation::Intersection:
                return true;
            default:
                assert(false);
                return false;
        }
    }
};

static PackageFilter* create_regex_filter(
    const Package* package, const toml::Value& node, const std::string_view mode_name)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry `" << package->cangjie_name() << "` " << mode_name << " filter" << std::endl;
    }

    if (node.is<std::string>()) {
        return new RegexFilter(package, node.as<std::string>(), mode_name);
    }

    if (!node.is<toml::Array>()) {
        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name,
            " filter must be a TOML string or an array of TOML strings");
    }
    auto* result = new SetOperationFilter(package, SetOperation::Union);

    std::size_t i = 0;
    for (auto&& item_any : node.as<toml::Array>()) {
        if (!item_any.is<std::string>()) {
            fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter #", i,
                " must be a TOML string");
        }
        const auto& item_string = item_any.as<std::string>();
        result->add_argument(new RegexFilter(package, item_string, mode_name));
        i++;
    }

    if (result->size() == 0) {
        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter array has no items");
    }

    return result;
}

static PackageFilter* create_set_filter(const Package* package, const toml::Value& node, const SetOperation op)
{
    const std::string_view mode_name = op == SetOperation::Union ? "union" : "intersect";

    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry `" << package->cangjie_name() << "` " << mode_name << " filter" << std::endl;
    }

    auto* result = new SetOperationFilter(package, op);

    if (!node.is<toml::Array>()) {
        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter must be an array of filters");
    }
    std::size_t i = 0;
    for (auto&& item_any : node.as<toml::Array>()) {
        if (!item_any.is<toml::Table>()) {
            fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter #", i,
                " must be a TOML table");
        }
        result->add_argument(create_filter(package, item_any.as<toml::Table>()));
        i++;
    }

    if (result->size() == 0) {
        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter array has no items");
    }

    return result;
}

PackageFilter* create_filter(const Package* package, const toml::Table& table)
{
    auto e = table.end();
    auto include_it = table.find("include");
    auto exclude_it = table.find("exclude");
    auto filter_it = table.find("filter");
    auto filter_not_it = table.find("filter-not");
    auto set_union_it = table.find("union");
    auto set_intersect_it = table.find("intersect");
    auto set_not_it = table.find("not");

    std::size_t non_null = 0;
    if (include_it != e) {
        non_null++;
    }
    if (exclude_it != e) {
        non_null++;
    }
    if (set_union_it != e) {
        non_null++;
    }
    if (set_intersect_it != e) {
        non_null++;
    }
    if (set_not_it != e) {
        non_null++;
    }

    if (non_null == 0) {
        fatal("`packages` entry `", package->cangjie_name(),
            "` filter has no specified operations (like include, union, etc)");
    }

    if (non_null != 1) {
        fatal("`packages` entry `", package->cangjie_name(), "` filter has ", non_null,
            " operations, but only 1 is allowed simultaneously");
    }

    auto* result = [package, e, include_it, exclude_it, set_union_it, set_intersect_it,
                       set_not_it]() -> PackageFilter* {
        if (include_it != e) {
            return create_regex_filter(package, include_it->second, "include");
        }

        if (exclude_it != e) {
            return new NotFilter(package, create_regex_filter(package, exclude_it->second, "exclude"));
        }

        if (set_union_it != e) {
            return create_set_filter(package, set_union_it->second, SetOperation::Union);
        }

        if (set_intersect_it != e) {
            return create_set_filter(package, set_intersect_it->second, SetOperation::Intersection);
        }

        if (set_not_it == e) {
            fatal("`packages` entry `", package->cangjie_name(), "` filter is unknown");
        }
        const auto& set_not_any = set_not_it->second;
        if (!set_not_any.is<toml::Table>()) {
            fatal("`packages` entry `", package->cangjie_name(), "` not filter must be a TOML table");
        }
        return new NotFilter(package, create_filter(package, set_not_any.as<toml::Table>()));
    }();

    if (filter_it != e || filter_not_it != e) {
        auto* intersect = new SetOperationFilter(package, SetOperation::Intersection);
        intersect->add_argument(result);

        if (filter_it != e) {
            const auto* regex_filter = create_regex_filter(package, filter_it->second, "filter");
            intersect->add_argument(regex_filter);
        }

        if (filter_not_it != e) {
            const auto* regex_filter = create_regex_filter(package, filter_not_it->second, "filter-not");
            intersect->add_argument(new NotFilter(package, regex_filter));
        }

        return intersect;
    }

    return result;
}

} // namespace objcgen
