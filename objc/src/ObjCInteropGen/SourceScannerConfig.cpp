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
#include "ExpandString.h"
#include "FatalException.h"
#include "Logging.h"

namespace objcgen {

static void toml_array_to_vector(const toml::Table& table, const std::string& source_name,
    std::vector<std::string>& results, const std::string& property_name)
{
    auto property_it = table.find(property_name);
    if (property_it == table.end()) {
        if (verbosity >= LogLevel::DEBUG) {
            std::cerr << "Source `" << source_name << "` property `" << property_name << "` is missing" << std::endl;
        }
        return;
    }

    const auto& property_any = property_it->second;
    if (!property_any.is<toml::Array>()) {
        fatal("Source `", source_name, "` property `", property_name, "` should be a TOML array");
    }
    std::size_t i = 0;
    for (auto&& item_any : property_any.as<toml::Array>()) {
        if (!item_any.is<std::string>()) {
            fatal("Source `", source_name, "` array `", property_name, "` item #", i, " is not a TOML string");
        }
        results.push_back(item_any.as<std::string>());
        i++;
    }
}

[[nodiscard]] static bool is_mixin_applicable(
    const std::string& source_name, const std::string& item_name, const std::string& mixin_source_string)
{
    try {
        auto mixin_source_regex = std::regex(mixin_source_string);
        const auto regex_match = std::regex_match(source_name.begin(), source_name.end(), mixin_source_regex);

        if (verbosity >= LogLevel::DEBUG) {
            std::cerr << item_name << " (`" << mixin_source_string << "`) does" << (regex_match ? "" : " NOT")
                      << " match source `" << source_name << '`' << std::endl;
        }

        return regex_match;
    } catch (const std::regex_error& e) {
        fatal(item_name, " (`", mixin_source_string, "`) has thrown an error: ", describe_regex_error(e.code()));
    }
}

[[nodiscard]] static bool is_mixin_applicable(
    const std::string& source_name, const std::string& mixin_name, const toml::Array& mixin_sources_array)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` " << mixin_name << " is being checked for applicability to `sources` entry `"
                  << source_name << '`' << std::endl;
    }

    std::size_t i = 0;
    for (auto&& mixin_source_any : mixin_sources_array) {
        std::string item_name = "`sources-mixins` " + mixin_name + " array `sources` item #" + std::to_string(i);
        if (!mixin_source_any.is<std::string>()) {
            fatal(item_name, " is not a TOML string");
        }

        const auto& mixin_source_string = mixin_source_any.as<std::string>();
        if (is_mixin_applicable(source_name, item_name, mixin_source_string)) {
            return true;
        }

        i++;
    }

    return false;
}

static void merge_array_property(toml::Table& current, const toml::Table& mixin, const std::string& source_name,
    const std::string& mixin_name, const std::string& property_name)
{
    auto& current_array = current.try_emplace(property_name, toml::Array()).first->second;
    if (!current_array.is<toml::Array>()) {
        fatal("`sources` entry `", source_name, "` property `", property_name, "` should be a TOML array");
    }

    // Merge by appending mixin value to current value
    auto mixin_it = mixin.find(property_name);
    if (mixin_it == mixin.end()) {
        return;
    }

    const auto& mixin_any = mixin_it->second;
    if (!mixin_any.is<toml::Array>()) {
        fatal("`sources-mixins` ", mixin_name, " property `", property_name, "` should be a TOML array");
    }
    for (auto&& item_any : mixin_any.as<toml::Array>()) {
        current_array.push(item_any);
    }
}

static void apply_mixin(
    const std::string& source_name, const std::string& mixin_name, toml::Table& table, const toml::Table& mixin)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` " << mixin_name << " is being applied to `sources` entry `" << source_name << '`'
                  << std::endl;
    }

    merge_array_property(table, mixin, source_name, mixin_name, "arguments-prepend");
    merge_array_property(table, mixin, source_name, mixin_name, "arguments-append");
}

static void apply_mixin(
    const std::string& source_name, const std::string& mixin_name, toml::Table& table, const toml::Value& mixin_any)
{
    if (!mixin_any.is<toml::Table>()) {
        fatal("`sources-mixins` ", mixin_name, " is not a TOML table");
    }

    const auto& mixin = mixin_any.as<toml::Table>();
    auto mixin_sources_it = mixin.find("sources");
    if (mixin_sources_it != mixin.end()) {
        const auto& mixin_sources_any = mixin_sources_it->second;
        if (mixin_sources_any.is<toml::Array>()) {
            if (!is_mixin_applicable(source_name, mixin_name, mixin_sources_any.as<toml::Array>())) {
                return;
            }
        } else if (mixin_sources_any.is<std::string>()) {
            std::string item_name = "`sources-mixins` " + mixin_name + " `sources` property";
            if (!is_mixin_applicable(source_name, item_name, mixin_sources_any.as<std::string>())) {
                return;
            }
        } else {
            fatal("`sources-mixins` ", mixin_name, " must have `sources` filter string or TOML array property");
        }
    } else if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` " << mixin_name << " is applicable to all `sources` entries" << std::endl;
    }

    apply_mixin(source_name, mixin_name, table, mixin);
}

static void apply_mixins(const toml::Value& mixins_any, const std::string& source_name, toml::Table& entry)
{
    if (mixins_any.is<toml::Array>()) {
        std::uint64_t i = 0;
        for (auto&& mixin_any : mixins_any.as<toml::Array>()) {
            std::string mixin_name = "item #" + std::to_string(i);
            apply_mixin(source_name, mixin_name, entry, mixin_any);
            i++;
        }
        return;
    }

    if (mixins_any.is<toml::Table>()) {
        for (auto&& [mixin_key, mixin_any] : mixins_any.as<toml::Table>()) {
            std::string mixin_name = "entry `" + mixin_key + '`';
            apply_mixin(source_name, mixin_name, entry, mixin_any);
        }
        return;
    }

    fatal("`sources-mixins` should be a TOML array of tables");
}

static void parse_sources(const toml::Table& options, const std::string& source_name, ClangSession& session)
{
    std::vector<std::string> files;
    toml_array_to_vector(options, source_name, files, "paths");

    std::vector<std::string> arguments;
    toml_array_to_vector(options, source_name, arguments, "arguments-prepend");
    toml_array_to_vector(options, source_name, arguments, "arguments");
    toml_array_to_vector(options, source_name, arguments, "arguments-append");

    for (auto& item : files) {
        item = expand_string(item);
    }

    for (auto& item : arguments) {
        item = expand_string(item);
    }

    session.parse_sources(files, arguments);
}

void parse_sources()
{
    const auto* sources_any = Config::find("sources");
    if (!sources_any || !sources_any->is<toml::Table>()) {
        fatal("`sources` should be a TOML table");
    }
    auto session_ptr = ClangSession::create();
    auto& session = *session_ptr;

    const auto* mixins_any = Config::find("sources-mixins");
    for (auto&& [source_name, source_any] : sources_any->as<toml::Table>()) {
        if (!source_any.is<toml::Table>()) {
            fatal("`sources` entry `", source_name, "` is not a TOML table");
        }
        const auto& source_table = source_any.as<toml::Table>();
        if (mixins_any) {
            toml::Table entry = source_table;

            if (entry.find("arguments-prepend") != entry.end()) {
                std::cerr << "`sources` entry `" << source_name
                          << "` should use `arguments` instead of `arguments-prepend`" << std::endl;
            }

            if (entry.find("arguments-append") != entry.end()) {
                std::cerr << "`sources` entry `" << source_name
                          << "` should use `arguments` instead of `arguments-append`" << std::endl;
            }

            apply_mixins(*mixins_any, source_name, entry);

            parse_sources(entry, source_name, session);
        } else {
            parse_sources(source_table, source_name, session);
        }
    }
}

} // namespace objcgen
