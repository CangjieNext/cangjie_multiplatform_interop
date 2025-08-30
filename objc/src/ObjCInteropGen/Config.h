// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <string_view>
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

#ifdef OBJCGEN_NO_WARNINGS
extern toml::table g_config;
#else
extern toml::table config;
#endif

void parse_toml_config_file(std::string_view path);

} // namespace objcgen

#endif // CONFIG_H
