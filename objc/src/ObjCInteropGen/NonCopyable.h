// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

namespace objcgen {

class NonCopyable {
protected:
    NonCopyable() = default;

    ~NonCopyable() = default;

private:
    NonCopyable(const NonCopyable&) = delete;

    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&) = delete;

    NonCopyable& operator=(NonCopyable&&) = delete;
};

} // namespace objcgen

#endif // NONCOPYABLE_H