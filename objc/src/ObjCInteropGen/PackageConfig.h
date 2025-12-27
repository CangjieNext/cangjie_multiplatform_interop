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

[[nodiscard]] std::optional<std::string> get_string_value(
    const toml::Table& config, std::string_view package_name, const std::string& property_name);

[[nodiscard]] PackageFilter& create_filter(const Package& package, const toml::Table& table);

[[nodiscard]] std::string compute_output_path(
    const std::string& name, const toml::Table& config, std::string_view package_cangjie_name);

} // namespace objcgen

#endif // SCOPECONFIG_H
