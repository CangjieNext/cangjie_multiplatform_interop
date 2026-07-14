// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Universe.h"

#include <algorithm>

namespace objcgen {

Universe& Universe::get()
{
    static Universe universe;
    return universe;
}

NonTypeSymbol& TopLevel::add_function(
    std::string name, Type return_type, std::vector<ParameterSymbol> parameters, Modifiers modifiers)
{
    return members_.emplace_back(
        std::move(name), NonTypeSymbol::Kind::GlobalFunction, std::move(return_type), std::move(parameters), modifiers);
}

Universe::Universe()
    : unit_("Unit", PrimitiveTypeCategory::Unit, PrimitiveSize::Zero),
      bool_("Bool", PrimitiveTypeCategory::Boolean, PrimitiveSize::One),
      int8_("Int8", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::One),
      int16_("Int16", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::Two),
      int32_("Int32", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::Four),
      int64_("Int64", PrimitiveTypeCategory::SignedInteger, PrimitiveSize::Eight),
      uint8_("UInt8", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::One),
      uint16_("UInt16", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::Two),
      uint32_("UInt32", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::Four),
      uint64_("UInt64", PrimitiveTypeCategory::UnsignedInteger, PrimitiveSize::Eight),
      float16_("Float16", PrimitiveTypeCategory::FloatingPoint, PrimitiveSize::Two),
      float32_("Float32", PrimitiveTypeCategory::FloatingPoint, PrimitiveSize::Four),
      float64_("Float64", PrimitiveTypeCategory::FloatingPoint, PrimitiveSize::Eight),
      class_(NamedTypeSymbol::Kind::Interface, "ObjCClass"),
      id_(NamedTypeSymbol::Kind::Protocol, "ObjCId"),
      sel_(NamedTypeSymbol::Kind::Interface, "SEL" /* "ObjCSelector" */),

      // `pointer_` and `func_` do not have fixed names.  They can be "CPointer/CFunc"
      // or "ObjCPointer/ObjCFunc", depending on the parameter types.
      pointer_({}),
      func_({}),

      block_("ObjCBlock"),
      varray_("VArray")
{
    for (auto& map : types_) {
        map.reserve(PREALLOCATED_TYPE_COUNT);
    }
    type_order_.reserve(PREALLOCATED_TYPE_COUNT);
    register_type(unit_);
    register_type(bool_);
    register_type(int8_);
    register_type(int16_);
    register_type(int32_);
    register_type(int64_);
    register_type(uint8_);
    register_type(uint16_);
    register_type(uint32_);
    register_type(uint64_);
    register_type(float16_);
    register_type(float32_);
    register_type(float64_);
    register_type(class_);
    register_type(id_);
    register_type(sel_);
}

NonTypeSymbol& Universe::register_top_level_function(
    std::string name, Type return_type, std::vector<ParameterSymbol> parameters, Modifiers modifiers)
{
    return top_level_.add_function(std::move(name), std::move(return_type), std::move(parameters), modifiers);
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

void Universe::register_type(NamedTypeSymbol& symbol)
{
    std::string_view name = symbol.name();
    auto type_namespace = kind_to_typename(symbol.kind());

    auto& types_map = this->types_map(type_namespace);
    assert(types_map.find(name) == types_map.end());

    types_map.try_emplace(name, &symbol);
    type_order_.emplace_back(TypeOrderElement{type_namespace, name});
}

NamedTypeSymbol* Universe::type(NamedTypeSymbol::Kind where, std::string_view name) const noexcept
{
    assert(where != NamedTypeSymbol::Kind::Primitive);
    return this->type(kind_to_typename(where), name);
}

NamedTypeSymbol* Universe::type(TypeNamespace where, std::string_view name) const
{
    auto& types_map = this->types_map(where);
    const auto it = types_map.find(name);
    return it == types_map.end() ? nullptr : it->second;
}

NamedTypeSymbol* Universe::type(std::string_view name) const
{
    for (std::uint8_t i = 0; i < TYPE_NAMESPACE_COUNT; ++i) {
        if (auto* result = type(static_cast<TypeNamespace>(i), name)) {
            return result;
        }
    }
    return nullptr;
}

void Universe::rename_type(NamedTypeSymbol& symbol, std::string new_name)
{
    assert(!new_name.empty());
    std::string_view old_name = symbol.name();
    assert(!old_name.empty());
    assert(new_name != old_name);
    const auto type_namespace = kind_to_typename(symbol.kind());
    auto& types_map = this->types_map(type_namespace);

    auto type_order_it = std::find(type_order_.begin(), type_order_.end(), TypeOrderElement{type_namespace, old_name});
    assert(type_order_it != type_order_.end());
    auto node = types_map.extract(old_name);
    assert(node);
    assert(node.key() == old_name);
    assert(node.mapped() == &symbol);
    symbol.rename(std::move(new_name));
    std::string_view new_name_view = symbol.name();
    node.key() = new_name_view;
    types_map.insert(std::move(node));
    type_order_it->name = new_name_view;
}

const NonTypeSymbol* Universe::global_non_type_symbol(std::string_view name) const
{
    auto top_level = this->top_level();
    auto top_level_end = top_level.end();
    auto it = std::find_if(top_level.begin(), top_level_end, [name](const auto& global_func) {
        assert(global_func.is_global_function());
        return global_func.name() == name;
    });
    return it == top_level_end ? nullptr : &*it;
}

} // namespace objcgen
