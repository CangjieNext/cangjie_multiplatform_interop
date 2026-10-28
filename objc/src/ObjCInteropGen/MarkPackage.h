// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef MARKPACKAGE_H
#define MARKPACKAGE_H

namespace objcgen {

struct MarkPackage {
    MarkPackage() = delete;

    [[nodiscard]] static bool mark_package();

    static void filter_name_clashing_symbols();
};

} // namespace objcgen

#endif // MARKPACKAGE_H
