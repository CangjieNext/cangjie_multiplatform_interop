// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Mappings.h"

#include <iostream>
#include <regex>

#include "FatalException.h"
#include "Universe.h"

namespace objcgen {

std::deque<TypeMapping*> mappings;
std::regex primitive_regex(R"(_*((u?int|float)(_(least|fast))?(\d+|native|max|ptr)|wchar|ptrdiff|s?size)(_t)?_*)", std::regex::icase);

struct PrimitiveMapping final : TypeMapping {
    explicit PrimitiveMapping() = default;

    bool can_map(NamedTypeSymbol& type) const override
    {
        if (type.is(NamedTypeSymbol::Kind::TypeDef)) {
            auto& canonical_type = type.canonical_type();
            if (canonical_type.is<PrimitiveTypeSymbol>()) {
                auto category = canonical_type.as<PrimitiveTypeSymbol>().category();
                if (category == PrimitiveTypeCategory::Unit) {
                    return false;
                }

                const auto name = type.name();
                return std::regex_match(name.begin(), name.end(), primitive_regex);
            }
        }
        return false;
    }

    TypeLikeSymbol& map(NamedTypeSymbol& type) override;
};

TypeLikeSymbol& PrimitiveMapping::map(NamedTypeSymbol& type)
{
    assert(can_map(type));
    return type.canonical_type();
}

struct NonGenericMapping final : TypeMapping {
    explicit NonGenericMapping(std::string from, std::string to) : from_(std::move(from)), to_(std::move(to))
    {
    }

    bool can_map(NamedTypeSymbol& type) const override
    {
        const auto name = std::string(type.name());
        return from_ == name;
    }

    TypeLikeSymbol& map(NamedTypeSymbol& type) override;

private:
    std::string from_;
    std::string to_;
};

TypeLikeSymbol& NonGenericMapping::map([[maybe_unused]] NamedTypeSymbol& type)
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
    mappings.push_back(new PrimitiveMapping());
    read_toml_mappings();
}

void add_non_generic_mapping(std::string from, std::string to)
{
    auto* mapping = new NonGenericMapping(std::move(from), std::move(to));
    mappings.push_back(mapping);
}

} // namespace objcgen
