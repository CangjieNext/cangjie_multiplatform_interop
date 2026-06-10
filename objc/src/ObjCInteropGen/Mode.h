// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef MODE_H
#define MODE_H

#include <limits>

namespace objcgen {

extern enum class Mode {
    // Mode that is compatible with the current FE.  The generated mirrors are
    // compilable and can be used in interop. This mode is default.
    NORMAL,

    // For internal testing only.  May be removed in the future.  The generator uses
    // all features, including those not supported by the current FE.  The generated
    // mirrors therefore may not be compilable.
    EXPERIMENTAL,

    // For internal testing only.  May be removed in the future.  The same as
    // `EXPERIMENTAL`, but additionally the generator tries its best to create
    // mirrors that can be compiled by cjc, though they will not enable actual
    // interop.  This is achieved by removing the `@ObjcCMirror` attribute and
    // creating fake bodies for methods of `@ObjCMirror` classes/interfaces.
    GENERATE_DEFINITIONS
} mode;

[[nodiscard]] inline bool normal_mode() noexcept
{
    return mode == Mode::NORMAL;
}

[[nodiscard]] inline bool generate_definitions_mode() noexcept
{
    return mode == Mode::GENERATE_DEFINITIONS;
}

using ClosureDepthType = unsigned;

constexpr ClosureDepthType UNLIMITED_CLOSURE_DEPTH = std::numeric_limits<ClosureDepthType>::max();

extern ClosureDepthType g_closure_depth;

} // namespace objcgen

#endif // MODE_H
