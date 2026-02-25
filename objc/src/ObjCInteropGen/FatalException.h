// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef FATALEXCEPTION_H
#define FATALEXCEPTION_H

#include <iostream>

namespace objcgen {

template <class T, class... Args> [[noreturn]] void fatal(T&& what, Args&&... args);

class FatalException : public std::exception {
private:
    // Use the `fatal` function for throwing this exception
    FatalException() = default;

    template <class T, class... Args> friend void fatal(T&& what, Args&&... args);
};

template <class T, class... Args> [[noreturn]] void fatal(T&& what, Args&&... args)
{
    ((std::cerr << std::forward<T>(what)) << ... << std::forward<Args>(args)) << std::endl;
    throw FatalException();
}

} // namespace objcgen

#endif // FATALEXCEPTION_H
