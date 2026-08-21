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

std::deque<TypeMapping*> mappings;

struct NonGenericMapping final : TypeMapping {
    explicit NonGenericMapping(std::string from, std::string to) : from_(std::move(from)), to_(std::move(to))
    {
    }

    [[nodiscard]] bool can_map(const NamedTypeSymbol& type) const noexcept override
    {
        return from_ == type.name();
    }

    TypeLikeSymbol& map(NamedTypeSymbol& type) const override;

private:
    std::string from_;
    std::string to_;
};

TypeLikeSymbol& NonGenericMapping::map([[maybe_unused]] NamedTypeSymbol& type) const
{
    assert(can_map(type));
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
    auto* mapping = new NonGenericMapping(std::move(from), std::move(to));
    mappings.push_back(mapping);
}

static void set_type_mappings() noexcept
{
    for (auto&& type : Universe::get().types()) {
        for (const auto* mapping : mappings) {
            if (mapping->can_map(type)) {
                if (type.mapping()) {
                    std::cerr << "Warning: multiple mappings can be applied to type `" << type.name()
                              << "`.  Only the first one will be used." << std::endl;
                    break;
                }

                type.set_mapping(mapping);
            }
        }
    }
}

static void do_map(NonTypeSymbol& symbol)
{
    for (auto&& parameter : symbol.parameters()) {
        parameter.type().map();
    }
    symbol.return_type().map();
}

static void do_map()
{
    auto& universe = Universe::get();
    for (auto& top_level : universe.top_level()) {
        do_map(top_level);
    }
    for (auto&& decl : universe.types()) {
        if (auto* type = dynamic_cast<TypeDeclarationSymbol*>(&decl)) {
            for (auto&& member : type->members()) {
                if (!member.is_property()) {
                    do_map(member);
                }
            }
        } else if (auto* alias = dynamic_cast<TypeAliasSymbol*>(&decl)) {
            auto& target = alias->target();
            if (target.has_symbol_assigned()) {
                target.map();
            }
        }
    }
}

void apply_mappings()
{
    // Apply mappings
    set_type_mappings();
    do_map();
}
} // namespace objcgen
