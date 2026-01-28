// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Mappings.h"

#include <iostream>

#include "Universe.h"

namespace objcgen {

std::vector<TypeMapping*> mappings;

TypeLikeSymbol* NonGenericMapping::map([[maybe_unused]] NamedTypeSymbol* type)
{
    assert(can_map(type));
    auto* result = Universe::get().type(to_);
    if (!result) {
        std::cerr << "Unknown type " << to_ << " specified in [[mappings]]" << std::endl;
        exit(1);
    }
    return result;
}

void initialize_mappings()
{
    add_non_generic_mapping("Bool").add_from("BOOL");
    read_toml_mappings();
}

NonGenericMapping& add_non_generic_mapping(const std::string_view to)
{
    auto* mapping = new NonGenericMapping(to);
    mappings.push_back(mapping);
    return *mapping;
}

} // namespace objcgen
