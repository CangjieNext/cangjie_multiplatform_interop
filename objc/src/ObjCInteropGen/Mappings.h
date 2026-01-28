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

    virtual bool can_map([[maybe_unused]] NamedTypeSymbol* type)
    {
        return true;
    }

    virtual TypeLikeSymbol* map(NamedTypeSymbol* type) = 0;
};

class NonGenericMapping final : public TypeMapping {
    std::unordered_set<std::string> from_;
    std::string to_;

public:
    explicit NonGenericMapping(const std::string_view to) : to_(to)
    {
    }

    NonGenericMapping& add_from(const std::string_view from)
    {
        from_.emplace(from);
        return *this;
    }

    bool can_map(NamedTypeSymbol* type) override
    {
        const auto name = std::string(type->name());
        return from_.find(name) != from_.end();
    }

    TypeLikeSymbol* map(NamedTypeSymbol* type) override;
};

extern std::vector<TypeMapping*> mappings;

void initialize_mappings();

void read_toml_mappings();

NonGenericMapping& add_non_generic_mapping(std::string_view to);

} // namespace objcgen

#endif // MAPPINGS_H
