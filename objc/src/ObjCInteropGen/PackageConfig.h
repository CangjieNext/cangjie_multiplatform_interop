// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SCOPECONFIG_H
#define SCOPECONFIG_H

#include <iostream>
#include <optional>

#include "FatalException.h"
#include "Package.h"
#include "toml.h"

namespace objcgen {

template <typename T, typename Desc>
std::string get_string_value(
    const toml::Table& config, const Desc& package_name, const std::string& property_name, T fallback)
{
    auto package_name_it = config.find(property_name);
    if (package_name_it == config.end()) {
        return fallback(config);
    }
    const auto& package_name_any = package_name_it->second;
    if (!package_name_any.is<std::string>()) {
        fatal("`packages` entry ", package_name, " property `", property_name, "` should be a TOML string");
    }
    const auto& package_name_string = package_name_any.as<std::string>();
    if (package_name_string.empty()) {
        fatal("`packages` entry ", package_name, " string `", property_name, "` is empty");
    }

    return package_name_string;
}

[[nodiscard]] PackageFilter* create_filter(const Package* package, const toml::Table& table);

std::string compute_output_path(
    const std::string& name, const toml::Table& config, std::string_view package_cangjie_name);

} // namespace objcgen

#endif // SCOPECONFIG_H
