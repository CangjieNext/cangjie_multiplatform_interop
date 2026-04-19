// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef UNIVERSE_H
#define UNIVERSE_H

#include <deque>
#include <unordered_map>

#include "Symbol.h"

namespace objcgen {

enum class TypeNamespace : std::uint8_t {
    Primary,

    // In Objective-C, protocols and interfaces can have same names.  In the output
    // Cangjie code we will resolve the conflicts by adding the `Protocol` suffix to
    // protocol names when printing.
    Protocols,

    // These are built-in Cangjie type names, which are keywords.  If an Objective-C
    // symbol name conflicts with one of them, the conflict will be resolved by
    // enclosing the name in the `` ticks when printing.
    Keywords,

    // In the C language, type symbols tagged with struct/union/enum can share names
    // with non-tagged symbols.
    Tagged,

    Max = Tagged,
};

constexpr std::uint8_t TYPE_NAMESPACE_COUNT = static_cast<std::uint8_t>(TypeNamespace::Max) + 1;

using type_order_t = std::vector<std::pair<TypeNamespace, std::string>>;

template <bool constant> class UniverseNamedTypeIterator final {
    using Iterator = std::conditional_t<constant, type_order_t::const_iterator, type_order_t::iterator>;
    using Value = std::conditional_t<constant, const NamedTypeSymbol, NamedTypeSymbol>;

public:
    explicit UniverseNamedTypeIterator(Iterator it) noexcept : it_(it)
    {
    }

    [[nodiscard]] Value& operator*() const;

    auto& operator++() noexcept
    {
        ++it_;
        return *this;
    }

    [[nodiscard]] friend bool operator!=(
        const UniverseNamedTypeIterator& it1, const UniverseNamedTypeIterator& it2) noexcept
    {
        return it1.it_ != it2.it_;
    }

private:
    Iterator it_;
};

template <bool constant> class UniverseTypeDeclarationIterator final {
    using Iterator = std::conditional_t<constant, type_order_t::const_iterator, type_order_t::iterator>;

public:
    explicit UniverseTypeDeclarationIterator(Iterator it) : it_(it)
    {
        get();
    }

    [[nodiscard]] auto& operator*() const noexcept
    {
        return *symbol_;
    }

    auto& operator++()
    {
        ++it_;
        get();
        return *this;
    }

    [[nodiscard]] friend bool operator!=(
        const UniverseTypeDeclarationIterator& it1, const UniverseTypeDeclarationIterator& it2) noexcept
    {
        return it1.it_ != it2.it_;
    }

private:
    Iterator it_;
    std::conditional_t<constant, const TypeDeclarationSymbol, TypeDeclarationSymbol>* symbol_;

    void get();
};

class TopLevel final {
public:
    [[nodiscard]] auto members() const noexcept
    {
        return ConstCollection(members_);
    }

    [[nodiscard]] auto members() noexcept
    {
        return Collection(members_);
    }

    [[nodiscard]] NonTypeSymbol& add_function(std::string name, Type return_type, Modifiers modifiers);

private:
    std::deque<NonTypeSymbol> members_;
};

class Universe final : NonCopyable {
    using type_map_t = std::unordered_map<std::string, NamedTypeSymbol*>;

    template <bool constant> friend class UniverseNamedTypeIterator;
    template <bool constant> friend class UniverseTypeDeclarationIterator;

    static constexpr int PREALLOCATED_TYPE_COUNT = 8192;

    Universe();

    [[nodiscard]] type_map_t& types_map(const TypeNamespace index) noexcept
    {
        return types_[static_cast<std::uint8_t>(index)];
    }

    [[nodiscard]] const type_map_t& types_map(const TypeNamespace index) const noexcept
    {
        return types_[static_cast<std::uint8_t>(index)];
    }

    TopLevel top_level_;

    type_map_t types_[TYPE_NAMESPACE_COUNT];
    type_order_t type_order_;

    PrimitiveTypeSymbol unit_;
    PrimitiveTypeSymbol bool_;
    PrimitiveTypeSymbol int8_;
    PrimitiveTypeSymbol int16_;
    PrimitiveTypeSymbol int32_;
    PrimitiveTypeSymbol int64_;
    PrimitiveTypeSymbol uint8_;
    PrimitiveTypeSymbol uint16_;
    PrimitiveTypeSymbol uint32_;
    PrimitiveTypeSymbol uint64_;
    PrimitiveTypeSymbol float16_;
    PrimitiveTypeSymbol float32_;
    PrimitiveTypeSymbol float64_;
    TypeDeclarationSymbol class_;
    TypeDeclarationSymbol id_;
    TypeDeclarationSymbol sel_;
    BuiltInTypeSymbol pointer_;
    BuiltInTypeSymbol func_;
    BuiltInTypeSymbol block_;
    BuiltInTypeSymbol varray_;

public:
    [[nodiscard]] static Universe& get();

    [[nodiscard]] NonTypeSymbol& register_top_level_function(std::string name, Type return_type, Modifiers modifiers);

    void register_type(NamedTypeSymbol& symbol);

    [[nodiscard]] PrimitiveTypeSymbol& unit() noexcept
    {
        return unit_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& boolean() noexcept
    {
        return bool_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& int8() noexcept
    {
        return int8_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& int16() noexcept
    {
        return int16_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& int32() noexcept
    {
        return int32_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& int64() noexcept
    {
        return int64_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& uint8() noexcept
    {
        return uint8_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& uint16() noexcept
    {
        return uint16_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& uint32() noexcept
    {
        return uint32_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& uint64() noexcept
    {
        return uint64_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& float16() noexcept
    {
        return float16_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& float32() noexcept
    {
        return float32_;
    }

    [[nodiscard]] PrimitiveTypeSymbol& float64() noexcept
    {
        return float64_;
    }

    [[nodiscard]] TypeDeclarationSymbol& clazz() noexcept
    {
        return class_;
    }

    [[nodiscard]] TypeDeclarationSymbol& id() noexcept
    {
        return id_;
    }

    [[nodiscard]] TypeDeclarationSymbol& sel() noexcept
    {
        return sel_;
    }

    [[nodiscard]] BuiltInTypeSymbol& pointer() noexcept
    {
        return pointer_;
    }

    [[nodiscard]] BuiltInTypeSymbol& func() noexcept
    {
        return func_;
    }

    [[nodiscard]] BuiltInTypeSymbol& block() noexcept
    {
        return block_;
    }

    [[nodiscard]] BuiltInTypeSymbol& varray() noexcept
    {
        return varray_;
    }

    // Find the registered type symbol by its name and kind.  Return nullptr if no
    // such type has been registered.
    [[nodiscard]] NamedTypeSymbol* type(NamedTypeSymbol::Kind where, const std::string& name) const noexcept;

    // Find the registered type symbol by its name and namespace.  Return nullptr if
    // no such type has been registered.
    [[nodiscard]] NamedTypeSymbol* type(TypeNamespace where, const std::string& name) const;

    // Find a registered type symbol by its name.  Return nullptr if no such type
    // has been registered.
    [[nodiscard]] NamedTypeSymbol* type(const std::string& name) const;

    void process_rename(NamedTypeSymbol& symbol, const std::string& old_name);

    [[nodiscard]] auto top_level() const noexcept
    {
        return top_level_.members();
    }

    [[nodiscard]] auto top_level() noexcept
    {
        return top_level_.members();
    }

    [[nodiscard]] auto all_declarations() const noexcept
    {
        using TypeOrder = decltype(type_order_);
        return ConstCollection<const TypeOrder, UniverseNamedTypeIterator<true>>(type_order_);
    }

    [[nodiscard]] auto all_declarations() noexcept
    {
        using TypeOrder = decltype(type_order_);
        return Collection<TypeOrder, UniverseNamedTypeIterator<true>, UniverseNamedTypeIterator<false>>(type_order_);
    }

    [[nodiscard]] auto type_definitions() const noexcept
    {
        using TypeOrder = decltype(type_order_);
        return ConstCollection<const TypeOrder, UniverseTypeDeclarationIterator<true>>(type_order_);
    }

    [[nodiscard]] auto type_definitions() noexcept
    {
        using TypeOrder = decltype(type_order_);
        return Collection<TypeOrder, UniverseTypeDeclarationIterator<true>, UniverseTypeDeclarationIterator<false>>(
            type_order_);
    }
};

template <bool constant>
typename UniverseNamedTypeIterator<constant>::Value& UniverseNamedTypeIterator<constant>::operator*() const
{
    assert(it_ != Universe::get().type_order_.end());
    auto& el = *it_;
    auto* symbol = Universe::get().type(el.first, el.second);
    assert(symbol);
    return *symbol;
}

template <bool constant> void UniverseTypeDeclarationIterator<constant>::get()
{
    const auto& universe = Universe::get();
    for (auto e = universe.type_order_.end();; ++it_) {
        if (it_ == e) {
            symbol_ = nullptr;
            break;
        }
        auto& el = *it_;
        auto* s = universe.type(el.first, el.second);
        assert(s);
        symbol_ = dynamic_cast<TypeDeclarationSymbol*>(s);
        if (symbol_) {
            break;
        }
    }
}

} // namespace objcgen

#endif // UNIVERSE_H
