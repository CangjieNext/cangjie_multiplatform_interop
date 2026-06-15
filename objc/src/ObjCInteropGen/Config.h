// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include "toml.h"

namespace objcgen {

using ClosureDepthType = uint64_t;

constexpr ClosureDepthType UNLIMITED_CLOSURE_DEPTH = std::numeric_limits<ClosureDepthType>::max();

class Config {
public:
    static void parse_from_toml_file(const std::string& path);

    static const toml::Value* find(const std::string& key);

    static ClosureDepthType closure_depth() noexcept;
};

} // namespace objcgen

#endif // CONFIG_H
