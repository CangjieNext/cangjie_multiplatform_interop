// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "SourceScannerConfig.h"

#include <cassert>
#include <iostream>
#include <regex>

#include "ClangSession.h"
#include "Config.h"
#include "FatalException.h"
#include "Logging.h"

namespace objcgen {

static void toml_array_to_vector(const toml::table& table, const std::string& source_name,
    std::vector<std::string>& results, const std::string_view property_name)
{
    auto* property_any = table.get(property_name);
    if (!property_any) {
        if (verbosity >= LogLevel::DEBUG) {
            std::cerr << "Source `" << source_name << "` property `" << property_name << "` is missing" << std::endl;
        }
        return;
    }

    if (auto* property_array = property_any->as_array()) {
        std::size_t i = 0;
        for (auto&& item_any : *property_array) {
            if (auto* item_string = item_any.as_string()) {
                if (auto path = item_string->value_exact<std::string>()) {
                    results.push_back(*path);
                } else {
                    fatal("Source `", source_name, "` array `", property_name, "` item #", i, " has no string value");
                }
            } else {
                fatal("Source `", source_name, "` array `", property_name, "` item #", i, " is not a TOML string");
            }
            i++;
        }
    } else {
        fatal("Source `", source_name, "` property `", property_name, "` should be a TOML array");
    }
}

static bool is_mixin_applicable(
    const toml::key& source_name, const toml::key& mixin_name, const toml::array& mixin_sources_array)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` entry `" << mixin_name
                  << "` is being being checked for applicability to `sources` entry `" << source_name << "`"
                  << std::endl;
    }

    std::size_t i = 0;
    for (auto&& mixin_source_any : mixin_sources_array) {
        if (auto* mixin_source_string = mixin_source_any.as_string()) {
            if (auto mixin_source_string_optional = mixin_source_string->value_exact<std::string>()) {
                try {
                    auto mixin_source_regex = std::regex(*mixin_source_string_optional);
                    const auto regex_match =
                        std::regex_match(source_name.begin(), source_name.end(), mixin_source_regex);
                    if (verbosity >= LogLevel::DEBUG) {
                        std::cerr << "`sources-mixins` entry `" << mixin_name << "` array `sources` item #" << i
                                  << " (`" << *mixin_source_string_optional << "`) does" << (regex_match ? "" : " NOT")
                                  << " match source `" << source_name << "`" << std::endl;
                    }
                    if (regex_match) {
                        return true;
                    }
                } catch (std::regex_error& e) {
                    fatal("`sources-mixins` entry `", mixin_name, "` array `sources` item #", i, " (`",
                        *mixin_source_string_optional, "`) has thrown an error: ", describe_regex_error(e.code()));
                }
            } else {
                fatal("`sources-mixins` entry `", mixin_name, "` array `sources` item #", i, " has no string value");
            }
        } else {
            fatal("`sources-mixins` entry `", mixin_name, "` array `sources` item #", i, " is not a TOML string");
        }
        i++;
    }
    return false;
}

static void merge_array_property(toml::table& current, const toml::table& mixin, const toml::key& source_name,
    const toml::key& mixin_name, const std::string_view property_name)
{
    if (!current.get(property_name)) {
        current.emplace<toml::array>(property_name);
    }

    // Merge by appending mixin value to current value
    if (auto* current_array = current.get_as<toml::array>(property_name)) {
        auto* mixin_any = mixin.get(property_name);
        if (!mixin_any) {
            return;
        }

        if (auto* mixin_array = mixin_any->as_array()) {
            for (auto&& item_any : *mixin_array) {
                current_array->push_back(item_any);
            }
        } else {
            fatal("`sources-mixins` entry `", mixin_name, "` property `", property_name, "` should be a TOML array");
        }
    } else {
        fatal("`sources` entry `", source_name, "` property `", property_name, "` should be a TOML array");
    }
}

static void apply_mixin(
    const toml::key& source_name, const toml::key& mixin_name, toml::table& table, const toml::table& mixin)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` entry `" << mixin_name << "` is being applied to `sources` entry `"
                  << source_name << "`" << std::endl;
    }

    merge_array_property(table, mixin, source_name, mixin_name, "arguments-prepend");
    merge_array_property(table, mixin, source_name, mixin_name, "arguments-append");
}

static void apply_mixins(const toml::node& mixins_any, const toml::key& source_name, toml::table& entry)
{
    if (auto* mixins = mixins_any.as_table()) {
        for (auto&& [mixin_name, mixin_any] : *mixins) {
            if (auto* mixin = mixin_any.as_table()) {
                if (auto* mixin_sources_any = mixin->get("sources")) {
                    if (auto* mixin_sources_array = mixin_sources_any->as_array()) {
                        if (!is_mixin_applicable(source_name, mixin_name, *mixin_sources_array)) {
                            continue;
                        }
                    } else {
                        fatal("`sources-mixins` entry `", mixin_name, "` must have TOML array `sources`");
                    }
                } else {
                    fatal("`sources-mixins` entry `", mixin_name, "` has no `sources` entry");
                }

                apply_mixin(source_name, mixin_name, entry, *mixin);
            } else {
                fatal("`sources-mixins` entry `", mixin_name, "` is not a TOML table");
            }
        }
    } else {
        fatal("`sources-mixins` should be a TOML table");
    }
}

static void parse_sources(const toml::table& options, const std::string& source_name, ClangSession& session)
{
    std::vector<std::string> files;
    toml_array_to_vector(options, source_name, files, "paths");

    std::vector<std::string> arguments;
    toml_array_to_vector(options, source_name, arguments, "arguments-prepend");
    toml_array_to_vector(options, source_name, arguments, "arguments");
    toml_array_to_vector(options, source_name, arguments, "arguments-append");

    session.parse_sources(files, arguments);
}

void parse_sources()
{
    const auto* mixins_any = config.get("sources-mixins");

    if (auto* sources = config.get_as<toml::table>("sources")) {
        auto session = ClangSession::create();

        for (auto&& [source_name, source_any] : *sources) {
            if (const auto* source = source_any.as_table()) {
                toml::table entry = *source;

                if (mixins_any) {
                    apply_mixins(*mixins_any, source_name, entry);
                }

                auto source_name_string = std::string(source_name.str());
                parse_sources(entry, source_name_string, *session);
            } else {
                fatal("`sources` entry `", source_name, "` is not a TOML table");
            }
        }
    } else {
        fatal("`sources` should be a TOML table");
    }
}

} // namespace objcgen
