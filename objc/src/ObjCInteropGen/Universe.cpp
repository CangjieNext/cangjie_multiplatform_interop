// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Universe.h"

namespace objcgen {

Universe& Universe::get()
{
    static Universe universe;
    return universe;
}

NonTypeSymbol& TopLevel::add_function(std::string name, TypeLikeSymbol& return_type, uint16_t modifiers)
{
    return members_.emplace_back(
        NonTypeSymbol::Private(), std::move(name), NonTypeSymbol::Kind::GlobalFunction, &return_type, modifiers);
}

TopLevelIterator Universe::top_level() noexcept
{
    return top_level_.get_iterator();
}

Universe::Universe()
    : unit_{"Unit", PrimitiveTypeCategory::Unit, PrimitiveSize::Zero},
      bool_{"Bool", PrimitiveTypeCategory::Boolean, PrimitiveSize::One},
      int8_{"Int8", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::One},
      int16_{"Int16", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::Two},
      int32_{"Int32", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::Four},
      int64_{"Int64", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::Eight},
      uint8_{"UInt8", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::One},
      uint16_{"UInt16", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::Two},
      uint32_{"UInt32", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::Four},
      uint64_{"UInt64", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::Eight},
      float16_{"Float16", PrimitiveTypeCategory::FloatingPoint, PrimitiveSize::Two},
      float32_{"Float32", PrimitiveTypeCategory::FloatingPoint, PrimitiveSize::Four},
      float64_{"Float64", PrimitiveTypeCategory::FloatingPoint, PrimitiveSize::Eight},
      int128_{int64_, 2},
      uint128_{uint64_, 2},
      class_{NamedTypeSymbol::Kind::Interface, "ObjCClass"},
      id_{NamedTypeSymbol::Kind::Protocol, "ObjCId"},
      sel_{NamedTypeSymbol::Kind::Interface, "SEL"} // "ObjCSelector"
{
    for (auto& map : types_) {
        map.reserve(PREALLOCATED_TYPE_COUNT);
    }
    type_order_.reserve(PREALLOCATED_TYPE_COUNT);
    register_type(&unit_);
    register_type(&bool_);
    register_type(&int8_);
    register_type(&int16_);
    register_type(&int32_);
    register_type(&int64_);
    register_type(&uint8_);
    register_type(&uint16_);
    register_type(&uint32_);
    register_type(&uint64_);
    register_type(&float16_);
    register_type(&float32_);
    register_type(&float64_);
    register_type(&class_);
    register_type(&id_);
    register_type(&sel_);
}

NonTypeSymbol& Universe::register_top_level_function(std::string name, TypeLikeSymbol& return_type, uint16_t modifiers)
{
    return top_level_.add_function(std::move(name), return_type, modifiers);
}

[[nodiscard]] static TypeNamespace kind_to_typename(NamedTypeSymbol::Kind kind) noexcept
{
    switch (kind) {
        case NamedTypeSymbol::Kind::Protocol:
            return TypeNamespace::Protocols;
        case NamedTypeSymbol::Kind::Primitive:
            return TypeNamespace::Keywords;
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Enum:
        case NamedTypeSymbol::Kind::Union:
            return TypeNamespace::Tagged;
        default:
            return TypeNamespace::Primary;
    }
}

void Universe::register_type(NamedTypeSymbol* symbol)
{
    assert(symbol);
    const auto kind = symbol->kind();
    const auto name = std::string(symbol->name());
    const auto type_namespace = kind_to_typename(kind);

    auto& types_map = this->types_map(type_namespace);
    assert(types_map.find(name) == types_map.end());

    types_map.emplace(name, symbol);
    type_order_.emplace_back(type_namespace, name);
}

TypeLikeSymbol* Universe::primitive_type(PrimitiveTypeCategory category, size_t size) noexcept
{
    switch (category) {
        case PrimitiveTypeCategory::Boolean:
            return size == 1 ? &boolean() : nullptr;
        case PrimitiveTypeCategory::SignedInteger:
            switch (size) {
                case 1:
                    return &int8();
                case 2:
                    return &int16();
                case 4:
                    return &int32();
                case 8:
                    return &int64();
                case 16:
                    return &int128();
                default:
                    return nullptr;
            }
        case PrimitiveTypeCategory::UnsignedInteger:
            switch (size) {
                case 1:
                    return &uint8();
                case 2:
                    return &uint16();
                case 4:
                    return &uint32();
                case 8:
                    return &uint64();
                case 16:
                    return &uint128();
                default:
                    return nullptr;
            }
        case PrimitiveTypeCategory::FloatingPoint:
            switch (size) {
                case 2:
                    return &float16();
                case 4:
                    return &float32();
                case 8:
                    return &float64();
                default:
                    return nullptr;
            }
        default:
            return category == PrimitiveTypeCategory::Unit && size == 0 ? &unit() : nullptr;
    }
}

NamedTypeSymbol* Universe::type(NamedTypeSymbol::Kind where, const std::string& name) const noexcept
{
    assert(where != NamedTypeSymbol::Kind::Primitive);
    return this->type(kind_to_typename(where), name);
}

NamedTypeSymbol* Universe::type(TypeNamespace where, const std::string& name) const
{
    auto& types_map = this->types_map(where);
    const auto it = types_map.find(name);
    return it == types_map.end() ? nullptr : it->second;
}

NamedTypeSymbol* Universe::type(const std::string& name) const
{
    for (std::uint8_t i = 0; i < TYPE_NAMESPACE_COUNT; ++i) {
        if (auto* result = type(static_cast<TypeNamespace>(i), name)) {
            return result;
        }
    }
    return nullptr;
}

void Universe::process_rename(NamedTypeSymbol* symbol, const std::string& old_name)
{
    assert(symbol);

    const auto kind = symbol->kind();
    const auto new_name = std::string(symbol->name());
    const auto type_namespace = kind_to_typename(kind);
    assert(!new_name.empty());
    assert(!old_name.empty());
    assert(new_name != old_name);

    auto& types_map = this->types_map(type_namespace);
    auto it = types_map.find(old_name);
    assert(it != types_map.end());
    assert(it->first == old_name);
    assert(it->second == symbol);
    types_map.erase(it);

    assert(types_map.find(new_name) == types_map.end());
    types_map.emplace(new_name, symbol);

    [[maybe_unused]] auto changed = false;

    for (auto& [fst, snd] : type_order_) {
        if (fst == type_namespace && snd == old_name) {
            assert(!changed);
            snd = new_name;
            changed = true;
        }
    }

    assert(changed);
}

} // namespace objcgen
