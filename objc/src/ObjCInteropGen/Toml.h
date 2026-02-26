// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef TOML_H
#define TOML_H

#include <filesystem>

#ifdef OBJCGEN_NO_WARNINGS
#ifdef _WIN32
#define gmtime_r(t, r) (gmtime_s((r), (t)) ? nullptr : (r))
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#endif
#include "toml/toml.h"
#ifdef OBJCGEN_NO_WARNINGS
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef _WIN32
#undef gmtime_r
#endif
#endif

namespace objcgen {

class TomlParseError : public std::runtime_error {
public:
    TomlParseError(const std::string& what) : std::runtime_error(what)
    {
    }

    TomlParseError(const char* what) : std::runtime_error(what)
    {
    }
};

} // namespace objcgen

#endif // TOML_H
