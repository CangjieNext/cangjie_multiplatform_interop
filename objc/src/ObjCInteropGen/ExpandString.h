// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef EXPANDSTRING_H
#define EXPANDSTRING_H

#include <string>
#include <string_view>

namespace objcgen {

void add_constant(std::string_view name, std::string_view value);

std::string expand_string(std::string_view input);

} // namespace objcgen

#endif // EXPANDSTRING_H