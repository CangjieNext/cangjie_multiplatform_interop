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

struct TypeMapping {
    virtual ~TypeMapping() = default;

    [[nodiscard]] virtual bool can_map([[maybe_unused]] NamedTypeSymbol& type) const = 0;

    virtual TypeLikeSymbol& map(NamedTypeSymbol& type) = 0;
};

extern std::deque<TypeMapping*> mappings;

void initialize_mappings();

void read_toml_mappings();

void add_non_generic_mapping(std::string from, std::string to);

} // namespace objcgen

#endif // MAPPINGS_H
