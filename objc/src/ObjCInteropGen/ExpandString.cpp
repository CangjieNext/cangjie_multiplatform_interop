// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ExpandString.h"

#include <regex>
#include <unordered_map>

namespace objcgen {

std::regex variable_regex(R"(\$(\w+|\{\w+\}))");
std::unordered_map<std::string, std::string> variable_map;

template <typename T> T advanced(T it, std::size_t offset)
{
    std::advance(it, offset);
    return it;
}

void add_constant(const std::string_view name, const std::string_view value)
{
    variable_map.insert_or_assign(std::string(name), std::string(value));
}

std::string expand_string(const std::string_view input)
{
    auto result = std::string(input);
    std::size_t offset = 0;

    std::smatch match;
    while (std::regex_search(advanced(result.cbegin(), offset), result.cend(), match, variable_regex)) {
        std::string name = match.str(1);

        // Remove the braces
        if (name.front() == '{' && name.back() == '}') {
            name.erase(name.begin());
            name.erase(name.end() - 1);
        }

        const auto match_position = static_cast<std::size_t>(match.position());
        const auto match_length = static_cast<std::size_t>(match.length());
        const auto map_it = variable_map.find(name);
        if (map_it != variable_map.end()) {
            const auto& value = map_it->second;
            result.replace(offset + match_position, match_length, value);
        } else {
            offset += match_position + match_length;
        }

        match = {};
    }

    return result;
}

} // namespace objcgen