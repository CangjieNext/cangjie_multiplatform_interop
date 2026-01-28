// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef STRINGS_H
#define STRINGS_H

#include <string>
#include <string_view>

namespace objcgen {

using namespace std::string_view_literals;

inline bool starts_with(const std::string_view str, const std::string_view prefix)
{
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

inline bool ends_with(const std::string_view str, const std::string_view suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

[[nodiscard]] inline std::string_view remove_prefix(std::string_view str, std::string_view prefix)
{
    return starts_with(str, prefix) ? str.substr(prefix.size()) : str;
}

inline void remove_prefix_in_place(std::string& str, const std::string_view prefix)
{
    if (starts_with(str, prefix)) {
        str.erase(0, prefix.size());
    }
}

[[nodiscard]] inline std::string_view remove_suffix(std::string_view str, std::string_view suffix)
{
    return ends_with(str, suffix) ? str.substr(0, str.size() - suffix.size()) : str;
}

inline void remove_suffix_in_place(std::string& str, const std::string_view prefix)
{
    if (ends_with(str, prefix)) {
        str.resize(str.size() - prefix.size());
    }
}

} // namespace objcgen

#endif // STRINGS_H
