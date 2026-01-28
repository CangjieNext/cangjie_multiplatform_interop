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
    const Package* package, const toml::node& node, const std::string_view mode_name)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry `" << package->cangjie_name() << "` " << mode_name << " filter" << std::endl;
    }

    if (auto* node_string = node.as_string()) {
        if (const auto node_value = node_string->value_exact<std::string>()) {
            return new RegexFilter(package, *node_value, mode_name);
        }

        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter has no string value");
    }

    if (auto* node_array = node.as_array()) {
        auto* result = new SetOperationFilter(package, SetOperation::Union);

        std::size_t i = 0;
        for (auto&& item_any : *node_array) {
            if (auto* item_string = item_any.as_string()) {
                if (auto item_value = item_string->value_exact<std::string>()) {
                    result->add_argument(new RegexFilter(package, *item_value, mode_name));
                } else {
                    fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter #", i,
                        " has no string value");
                }
            } else {
                fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter #", i,
                    " must be a TOML string");
            }
            i++;
        }

        if (result->size() == 0) {
            fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter array has no items");
        }

        return result;
    }

    fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name,
        " filter must be a TOML string or an array of TOML strings");
}

static PackageFilter* create_set_filter(const Package* package, const toml::node& node, const SetOperation op)
{
    const std::string_view mode_name = op == SetOperation::Union ? "union" : "intersect";

    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry `" << package->cangjie_name() << "` " << mode_name << " filter" << std::endl;
    }

    auto* result = new SetOperationFilter(package, op);

    if (auto* node_array = node.as_array()) {
        std::size_t i = 0;
        for (auto&& item_any : *node_array) {
            if (auto* item_table = item_any.as_table()) {
                const auto* arg = create_filter(package, *item_table);
                result->add_argument(arg);
            } else {
                fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter #", i,
                    " must be a TOML table");
            }
            i++;
        }
    } else {
        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter must be an array of filters");
    }

    const auto size = result->size();
    if (size == 0) {
        fatal("`packages` entry `", package->cangjie_name(), "` ", mode_name, " filter array has no items");
    }

    return result;
}

PackageFilter* create_filter(const Package* package, const toml::table& table)
{
    auto* include = table.get("include");
    auto* exclude = table.get("exclude");
    auto* filter = table.get("filter");
    auto* filter_not = table.get("filter-not");
    auto* set_union = table.get("union");
    auto* set_intersect = table.get("intersect");
    auto* set_not = table.get("not");

    std::size_t non_null = 0;
    if (include)
        non_null++;
    if (exclude)
        non_null++;
    if (set_union)
        non_null++;
    if (set_intersect)
        non_null++;
    if (set_not)
        non_null++;

    if (non_null == 0) {
        fatal("`packages` entry `", package->cangjie_name(),
            "` filter has no specified operations (like include, union, etc)");
    }

    if (non_null != 1) {
        fatal("`packages` entry `", package->cangjie_name(), "` filter has ", non_null,
            " operations, but only 1 is allowed simultaneously");
    }

    auto* result = [package, include, exclude, set_union, set_intersect, set_not]() -> PackageFilter* {
        if (include) {
            return create_regex_filter(package, *include, "include");
        }

        if (exclude) {
            return new NotFilter(package, create_regex_filter(package, *exclude, "exclude"));
        }

        if (set_union) {
            return create_set_filter(package, *set_union, SetOperation::Union);
        }

        if (set_intersect) {
            return create_set_filter(package, *set_intersect, SetOperation::Intersection);
        }

        if (set_not) {
            if (auto* set_not_table = set_not->as_table()) {
                return new NotFilter(package, create_filter(package, *set_not_table));
            }

            fatal("`packages` entry `", package->cangjie_name(), "` not filter must be a TOML table");
        }

        fatal("`packages` entry `", package->cangjie_name(), "` filter is unknown");
    }();

    if (filter || filter_not) {
        auto* intersect = new SetOperationFilter(package, SetOperation::Intersection);
        intersect->add_argument(result);

        if (filter) {
            const auto* regex_filter = create_regex_filter(package, *filter, "filter");
            intersect->add_argument(regex_filter);
        }

        if (filter_not) {
            const auto* regex_filter = create_regex_filter(package, *filter_not, "filter-not");
            intersect->add_argument(new NotFilter(package, regex_filter));
        }

        return intersect;
    }

    return result;
}

} // namespace objcgen
