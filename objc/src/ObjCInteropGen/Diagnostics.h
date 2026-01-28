// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

namespace objcgen {

/// Checks symbols that passed through the filters and prints warnings to
/// `std::cerr`, if any.
void check_marked_symbols();

} // namespace objcgen

#endif // DIAGNOSTICS_H
