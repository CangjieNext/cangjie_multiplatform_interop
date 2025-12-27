// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef PRINTUTILS_H
#define PRINTUTILS_H

#include <iosfwd>

namespace objcgen {

/** Print a list of comma-delimited items */
template <class It, class PrintItem, class Delimiter = const char*>
void print_list(std::ostream& stream, It begin, It end, PrintItem print_item, const Delimiter& delimiter = ", ")
{
    if (begin != end) {
        print_item(stream, *begin);
        for (++begin; begin != end; ++begin) {
            stream << delimiter;
            print_item(stream, *begin);
        }
    }
}

/** Print a list of comma-delimited items */
template <class Container, class PrintItem, class Delimiter = const char*>
void print_list(
    std::ostream& stream, const Container& container, PrintItem print_item, const Delimiter& delimiter = ", ")
{
    print_list(stream, container.begin(), container.end(), print_item, delimiter);
}

} // namespace objcgen

#endif // PRINTUTILS_H
