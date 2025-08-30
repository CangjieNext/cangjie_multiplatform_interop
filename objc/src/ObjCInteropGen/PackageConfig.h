// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SCOPECONFIG_H
#define SCOPECONFIG_H

#include <iostream>

#include "FatalException.h"
#include "Package.h"
#ifdef OBJCGEN_NO_WARNINGS
#if defined __clang__
#if __clang_major__ >= 16
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#elif defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#endif
#include "toml.hpp"
#ifdef OBJCGEN_NO_WARNINGS
#if defined __clang__
#if __clang_major__ >= 16
#pragma clang diagnostic pop
#endif
#elif defined __GNUC__
#pragma GCC diagnostic pop
#endif
#endif

namespace objcgen {

template <typename T, typename Desc>
std::string get_string_value(
    const toml::table& config, const Desc& package_name, const std::string_view property_name, T fallback)
{
    if (auto* package_name_any = config.get(property_name)) {
        if (auto* package_name_string = package_name_any->as_string()) {
            if (auto package_name_value = package_name_string->value_exact<std::string>()) {
                if (package_name_value->empty()) {
                    fatal("`packages` entry ", package_name, " string `", property_name, "` is empty");
                }

                return *package_name_value;
            }

            fatal("`packages` entry ", package_name, " string `", property_name, "` has no string value");
        }

        fatal("`packages` entry ", package_name, " property `", property_name, "` should be a TOML string");
    }

    return fallback(config);
}

[[nodiscard]] PackageFilter* create_filter(const Package* package, const toml::table& table);

std::string compute_output_path(
    const std::string& name, const toml::table& config, std::string_view package_cangjie_name);

} // namespace objcgen

#endif // SCOPECONFIG_H
