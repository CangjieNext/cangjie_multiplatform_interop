// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef MARKSCOPE_H
#define MARKSCOPE_H

namespace objcgen {

void mark_roots();
bool collect_symbols();
void add_all_symbol_references();
void mark_hidden();

} // namespace objcgen

#endif // MARKSCOPE_H
