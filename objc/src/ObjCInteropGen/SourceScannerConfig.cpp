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

static bool is_mixin_applicable(
    const std::string& source_name, const std::string& mixin_name, const toml::Array& mixin_sources_array)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` entry `" << mixin_name
                  << "` is being being checked for applicability to `sources` entry `" << source_name << "`"
                  << std::endl;
    }

    std::size_t i = 0;
    for (auto&& mixin_source_any : mixin_sources_array) {
        if (!mixin_source_any.is<std::string>()) {
            fatal("`sources-mixins` entry `", mixin_name, "` array `sources` item #", i, " is not a TOML string");
        }
        const auto& mixin_source_string = mixin_source_any.as<std::string>();
        try {
            auto mixin_source_regex = std::regex(mixin_source_string);
            const auto regex_match = std::regex_match(source_name.begin(), source_name.end(), mixin_source_regex);
            if (verbosity >= LogLevel::DEBUG) {
                std::cerr << "`sources-mixins` entry `" << mixin_name << "` array `sources` item #" << i << " (`"
                          << mixin_source_string << "`) does" << (regex_match ? "" : " NOT") << " match source `"
                          << source_name << "`" << std::endl;
            }
            if (regex_match) {
                return true;
            }
        } catch (const std::regex_error& e) {
            fatal("`sources-mixins` entry `", mixin_name, "` array `sources` item #", i, " (`", mixin_source_string,
                "`) has thrown an error: ", describe_regex_error(e.code()));
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
        fatal("`sources-mixins` entry `", mixin_name, "` property `", property_name, "` should be a TOML array");
    }
    for (auto&& item_any : mixin_any.as<toml::Array>()) {
        current_array.push(item_any);
    }
}

static void apply_mixin(
    const std::string& source_name, const std::string& mixin_name, toml::Table& table, const toml::Table& mixin)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`sources-mixins` entry `" << mixin_name << "` is being applied to `sources` entry `"
                  << source_name << "`" << std::endl;
    }

    merge_array_property(table, mixin, source_name, mixin_name, "arguments-prepend");
    merge_array_property(table, mixin, source_name, mixin_name, "arguments-append");
}

static void apply_mixins(const toml::Value& mixins_any, const std::string& source_name, toml::Table& entry)
{
    if (!mixins_any.is<toml::Table>()) {
        fatal("`sources-mixins` should be a TOML table");
    }
    for (auto&& [mixin_name, mixin_any] : mixins_any.as<toml::Table>()) {
        if (!mixin_any.is<toml::Table>()) {
            fatal("`sources-mixins` entry `", mixin_name, "` is not a TOML table");
        }
        const auto& mixin = mixin_any.as<toml::Table>();
        auto mixin_sources_it = mixin.find("sources");
        if (mixin_sources_it == mixin.end()) {
            fatal("`sources-mixins` entry `", mixin_name, "` has no `sources` entry");
        }
        const auto& mixin_sources_any = mixin_sources_it->second;
        if (!mixin_sources_any.is<toml::Array>()) {
            fatal("`sources-mixins` entry `", mixin_name, "` must have TOML array `sources`");
        }
        if (is_mixin_applicable(source_name, mixin_name, mixin_sources_any.as<toml::Array>())) {
            apply_mixin(source_name, mixin_name, entry, mixin);
        }
    }
}

static void parse_sources(const toml::Table& options, const std::string& source_name, ClangSession& session)
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
    const auto* sources_any = config.find("sources");
    if (!sources_any || !sources_any->is<toml::Table>()) {
        fatal("`sources` should be a TOML table");
    }
    auto session_ptr = ClangSession::create();
    auto& session = *session_ptr;

    const auto* mixins_any = config.find("sources-mixins");
    for (auto&& [source_name, source_any] : sources_any->as<toml::Table>()) {
        if (!source_any.is<toml::Table>()) {
            fatal("`sources` entry `", source_name, "` is not a TOML table");
        }
        const auto& source_table = source_any.as<toml::Table>();
        if (mixins_any) {
            auto entry = source_table;

            apply_mixins(*mixins_any, source_name, entry);

            parse_sources(entry, source_name, session);
        } else {
            parse_sources(source_table, source_name, session);
        }
    }
}

} // namespace objcgen
