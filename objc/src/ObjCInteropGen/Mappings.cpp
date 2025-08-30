// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Mappings.h"

#include <iostream>

#include "Universe.h"

namespace objcgen {

#ifdef OBJCGEN_NO_WARNINGS
std::vector<TypeMapping*> g_mappings;
#else
std::vector<TypeMapping*> mappings;
#endif

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
#ifdef OBJCGEN_NO_WARNINGS
    g_mappings.push_back(mapping);
#else
    mappings.push_back(mapping);
#endif
    return *mapping;
}

} // namespace objcgen
