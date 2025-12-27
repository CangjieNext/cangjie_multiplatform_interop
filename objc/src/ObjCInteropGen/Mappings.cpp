// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Mappings.h"

#include <iostream>

#include "FatalException.h"
#include "Universe.h"

namespace objcgen {

std::deque<TypeMapping> mappings;

NamedTypeSymbol& TypeMapping::map() const
{
    auto* result = Universe::get().type(to_);
    if (!result) {
        fatal("Unknown type ", to_, " specified in [[mappings]]");
    }
    return *result;
}

void initialize_mappings()
{
    add_non_generic_mapping("BOOL", "Bool");
    read_toml_mappings();
}

void add_non_generic_mapping(std::string from, std::string to)
{
    mappings.emplace_back(std::move(from), std::move(to));
}

} // namespace objcgen
