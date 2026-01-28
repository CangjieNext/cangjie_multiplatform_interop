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

static void read_table_mappings(const toml::table& mapping, std::size_t i)
{
    // TODO: consider supporting packages-mixins
    for (auto&& [key, value] : mapping) {
        if (auto* value_string = value.as_string()) {
            if (auto value_value = value_string->value_exact<std::string>()) {
                if (value_value->empty()) {
                    fatal("`mappings` entry #", i, " for key `", key, "` is empty");
                }
                add_non_generic_mapping(*value_value).add_from(key);
            } else {
                fatal("`mappings` entry #", i, " for key `", key, "` has no string value");
            }
        } else {
            fatal("`mappings` entry #", i, " for key `", key, "` is not a TOML string");
        }
    }
}

void read_toml_mappings()
{
    if (auto* mappings_any = config.get("mappings")) {
        if (auto* mappings = mappings_any->as_array()) {
            std::size_t i = 0;
            for (auto&& mapping_any : *mappings) {
                if (const auto* mapping = mapping_any.as_table()) {
                    read_table_mappings(*mapping, i);
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
