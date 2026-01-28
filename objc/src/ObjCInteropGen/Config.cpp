// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Config.h"

#include <filesystem>
#include <iostream>
#include <unordered_set>

#include "FatalException.h"
#include "Logging.h"

namespace objcgen {

toml::table config;

static void append_to_left(toml::array& lhs, const toml::array& rhs)
{
    for (auto&& item : rhs) {
        lhs.push_back(item);
    }
}

static void merge_to_left_array(toml::table& lhs, const toml::table& rhs, const std::string_view property_name)
{
    auto* lhs_any = lhs.get(property_name);
    auto* rhs_any = rhs.get(property_name);
    auto* lhs_array = lhs_any ? lhs_any->as_array() : nullptr;
    auto* rhs_array = rhs_any ? rhs_any->as_array() : nullptr;
    if (lhs_any && !lhs_array) {
        fatal("TOML property `", property_name, "` should be an array");
    }

    if (rhs_any && !rhs_array) {
        fatal("TOML property `", property_name, "` should be an array");
    }

    if (lhs_array) {
        if (rhs_array) {
            append_to_left(*lhs_array, *rhs_array);
        }
        return;
    }

    if (rhs_array) {
        lhs.emplace(property_name, *rhs_array);
    }
}

static void merge_to_left(toml::table& lhs, const toml::table& rhs)
{
    // No need to merge:
    // * output-roots
    // * sources, sources-mixins
    // Imports are "merged" by the caller
    merge_to_left_array(lhs, rhs, "packages");
    merge_to_left_array(lhs, rhs, "mappings");
}

static toml::table parse_toml_file(const std::string_view path, std::unordered_set<std::string> imported)
{
    if (verbosity >= LogLevel::INFO) {
        std::cerr << "Reading TOML file `" << path << "`" << std::endl;
    }

    if (auto [_, new_path] = imported.emplace(path); !new_path) {
        fatal("TOML file `", "` is recursive");
    }

    if (!std::filesystem::exists(path)) {
        fatal("TOML file `", "` doesn't exist");
    }

    auto config = toml::parse_file(path);
    if (auto* imports_any = config.get("imports")) {
        if (auto* imports_array = imports_any->as_array()) {
            std::size_t i = 0;
            for (auto&& item_any : *imports_array) {
                if (auto* item_string = item_any.as_string()) {
                    if (auto item_value = item_string->value_exact<std::string>()) {
                        if (item_value->empty()) {
                            fatal("`imports` in `", path, "` item #", i, " is empty");
                        }

                        const auto& import_path = *item_value;
                        auto import_config = parse_toml_file(import_path, imported);

                        if (verbosity >= LogLevel::INFO) {
                            std::cerr << "Merging TOML file `" << import_path << "` into `" << path << "`" << std::endl;
                        }

                        merge_to_left(config, import_config);
                    } else {
                        fatal("`imports` in `", path, "` item #", i, " has no string value");
                    }
                } else {
                    fatal("`imports` in `", path, "` item #", i, " should be a string");
                }
                i++;
            }
        } else {
            fatal("`imports` in `", path, "` should be a TOML array of strings");
        }
    }

    return config;
}

void parse_toml_config_file(const std::string_view path)
{
    config = parse_toml_file(path, {});
}

} // namespace objcgen
