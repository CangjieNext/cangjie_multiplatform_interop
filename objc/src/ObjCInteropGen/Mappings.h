// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef MAPPINGS_H
#define MAPPINGS_H

#include <string>
#include <unordered_set>

#include "Symbol.h"

namespace objcgen {

class TypeMapping final {
public:
    explicit TypeMapping(std::string from, std::string to) noexcept : from_(std::move(from)), to_(std::move(to))
    {
    }

    [[nodiscard]] bool can_map(const NamedTypeSymbol& type) const noexcept
    {
        return from_ == type.name();
    }

    [[nodiscard]] NamedTypeSymbol& map() const;

private:
    const std::string from_;
    const std::string to_;
};

extern std::deque<TypeMapping> mappings;

void initialize_mappings();

void read_toml_mappings();

void add_non_generic_mapping(std::string from, std::string to);

} // namespace objcgen

#endif // MAPPINGS_H
