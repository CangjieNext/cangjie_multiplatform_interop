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

namespace {
const std::regex primitive_regex(
    R"(_*((u?int|float)(_(least|fast))?(8|16|32|64|native|max|ptr)|wchar|ptrdiff|s?size)(_t)?_*)", std::regex::icase);

struct PrimitiveMapping final : TypeMapping {
    explicit PrimitiveMapping() = default;

    [[nodiscard]] bool can_map(const NamedTypeSymbol& type) const noexcept override
    {
        if (type.is(NamedTypeSymbol::Kind::TypeDef)) {
            const auto canonical_type = type.as<TypeAliasSymbol>().canonical_type();
            if (canonical_type.kind() == Type::Kind::Named && canonical_type.symbol().is<PrimitiveTypeSymbol>()) {
                const auto category = canonical_type.symbol().as<PrimitiveTypeSymbol>().category();
                if (category == PrimitiveTypeCategory::Unit) {
                    return false;
                }

                const auto& name = type.name();
                return std::regex_match(name.begin(), name.end(), primitive_regex);
            }
        }
        return false;
    }

    [[nodiscard]] Type map(const NamedTypeSymbol& type) const override;
};

struct NonGenericMapping final : TypeMapping {
    explicit NonGenericMapping(std::string from, std::string to) : from_(std::move(from)), to_(std::move(to))
    {
    }

    [[nodiscard]] bool can_map(const NamedTypeSymbol& type) const noexcept override
    {
        return from_ == type.name();
    }

    [[nodiscard]] Type map(const NamedTypeSymbol& type) const override;

private:
    std::string from_;
    std::string to_;
};
} // namespace

Type PrimitiveMapping::map(const NamedTypeSymbol& type) const
{
    assert(can_map(type));
    return Type(type.as<TypeAliasSymbol>().canonical_type().symbol());
}

Type NonGenericMapping::map([[maybe_unused]] const NamedTypeSymbol& type) const
{
    assert(can_map(type));
    auto* result = Universe::get().type(to_);
    if (!result) {
        fatal("Unknown type ", to_, " specified in [[mappings]]");
    }
    return Type(*result);
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
