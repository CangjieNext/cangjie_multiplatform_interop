// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef TOMLPARSEERROR_H
#define TOMLPARSEERROR_H

#include <stdexcept>

namespace objcgen {

class TomlParseError : public std::runtime_error {
public:
    TomlParseError(const std::string& path, const std::string& message) : std::runtime_error(message), path_(path)
    {
    }

    [[nodiscard]] const auto& path() const noexcept
    {
        return path_;
    }

private:
    std::string path_;
};

} // namespace objcgen

#endif // TOMLPARSEERROR_H
