// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <iostream>

#include "Config.h"
#include "FatalException.h"
#include "Logging.h"
#include "Mappings.h"

namespace objcgen {

static void read_table_mappings(const toml::Table& mapping, std::size_t i)
{
    // It makes sense to consider supporting packages-mixins
    for (auto&& [key, value] : mapping) {
        if (value.is<std::string>()) {
            const auto& value_string = value.as<std::string>();
            if (value_string.empty()) {
                fatal("`mappings` entry #", i, " for key `", key, "` is empty");
            }
            add_non_generic_mapping(value_string).add_from(key);
        } else {
            fatal("`mappings` entry #", i, " for key `", key, "` is not a TOML string");
        }
    }
}

void read_toml_mappings()
{
    if (const auto* mappings_any = config.find("mappings")) {
        if (mappings_any->is<toml::Array>()) {
            std::size_t i = 0;
            for (auto&& mapping_any : mappings_any->as<toml::Array>()) {
                if (mapping_any.is<toml::Table>()) {
                    read_table_mappings(mapping_any.as<toml::Table>(), i);
                } else {
                    fatal("`mappings` entry #", i, " is not a TOML table");
                }
                i++;
            }
        } else {
            fatal("`mappings` should be a TOML array of tables");
        }
    }
}

} // namespace objcgen
