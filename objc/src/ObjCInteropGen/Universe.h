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

template <class TypeSymbol> type_order_t::const_iterator find_first(type_order_t::const_iterator it);

template <class TypeSymbol> class UniverseIterator final {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TypeSymbol;
    using pointer = value_type*;   // or also value_type*
    using reference = value_type&; // or also value_type&

    type_order_t::const_iterator it_;

    [[nodiscard]] reference get() const;

public:
    explicit UniverseIterator(type_order_t::const_iterator it) : it_(std::move(it))
    {
    }

    reference operator*() const
    {
        return get();
    }
    pointer operator->()
    {
        return &get();
    }

    // Prefix increment
    UniverseIterator& operator++();

    friend bool operator==(const UniverseIterator& lhs, const UniverseIterator& rhs)
    {
        return lhs.it_ == rhs.it_;
    }

    friend bool operator!=(const UniverseIterator& lhs, const UniverseIterator& rhs)
    {
        return !(lhs == rhs);
    }
};

template <class TypeSymbol> struct UniverseTypes final {
    [[nodiscard]] UniverseIterator<TypeSymbol> cbegin() const;

    [[nodiscard]] UniverseIterator<TypeSymbol> cend() const;

    [[nodiscard]] auto begin() const
    {
        return cbegin();
    }

    [[nodiscard]] auto end() const
    {
        return cend();
    }
};

class TopLevelIterator final {
public:
    explicit TopLevelIterator(std::deque<NonTypeSymbol>& members) noexcept : members_(members)
    {
    }

    [[nodiscard]] auto begin() const noexcept
    {
        return members_.begin();
    }

    [[nodiscard]] auto end() const noexcept
    {
        return members_.end();
    }

private:
    std::deque<NonTypeSymbol>& members_;
};

class TopLevel final {
public:
    auto get_iterator() noexcept
    {
        return TopLevelIterator(members_);
    }

    NonTypeSymbol& add_function(std::string name, TypeLikeSymbol& return_type, uint16_t modifiers);

private:
    std::deque<NonTypeSymbol> members_;
};

class Universe final {
    using type_map_t = std::unordered_map<std::string, NamedTypeSymbol*>;

    template <class TypeSymbol> friend class UniverseIterator;
    template <class TypeSymbol> friend struct UniverseTypes;

    static constexpr int PREALLOCATED_TYPE_COUNT = 8192;

    Universe();

    type_map_t& types_map(const TypeNamespace index)
    {
        return types_[static_cast<std::uint8_t>(index)];
    }

    const type_map_t& types_map(const TypeNamespace index) const
    {
        return types_[static_cast<std::uint8_t>(index)];
    }

    template <class TypeSymbol> friend type_order_t::const_iterator find_first(type_order_t::const_iterator it);

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
    TypeDeclarationSymbol int128_;
    TypeDeclarationSymbol uint128_;
    TypeDeclarationSymbol class_;
    TypeDeclarationSymbol id_;
    TypeDeclarationSymbol sel_;

public:
    static Universe& get();

    NonTypeSymbol& register_top_level_function(std::string name, TypeLikeSymbol& return_type, uint16_t modifiers);

    void register_type(NamedTypeSymbol* symbol);

    [[nodiscard]] auto& unit() noexcept
    {
        return unit_;
    }

    [[nodiscard]] auto& boolean() noexcept
    {
        return bool_;
    }

    [[nodiscard]] auto& int8() noexcept
    {
        return int8_;
    }

    [[nodiscard]] auto& int16() noexcept
    {
        return int16_;
    }

    [[nodiscard]] auto& int32() noexcept
    {
        return int32_;
    }

    [[nodiscard]] auto& int64() noexcept
    {
        return int64_;
    }

    [[nodiscard]] auto& uint8() noexcept
    {
        return uint8_;
    }

    [[nodiscard]] auto& uint16() noexcept
    {
        return uint16_;
    }

    [[nodiscard]] auto& uint32() noexcept
    {
        return uint32_;
    }

    [[nodiscard]] auto& uint64() noexcept
    {
        return uint64_;
    }

    [[nodiscard]] auto& float16() noexcept
    {
        return float16_;
    }

    [[nodiscard]] auto& float32() noexcept
    {
        return float32_;
    }

    [[nodiscard]] auto& float64() noexcept
    {
        return float64_;
    }

    [[nodiscard]] auto& int128() noexcept
    {
        return int128_;
    }

    [[nodiscard]] auto& uint128() noexcept
    {
        return uint128_;
    }

    [[nodiscard]] auto& clazz() noexcept
    {
        return class_;
    }

    [[nodiscard]] auto& id() noexcept
    {
        return id_;
    }

    [[nodiscard]] auto& sel() noexcept
    {
        return sel_;
    }

    [[nodiscard]] NamedTypeSymbol* primitive_type(PrimitiveTypeCategory category, size_t size) noexcept;

    // Find the registered type symbol by its name and kind.  Return nullptr if no
    // such type has been registered.
    [[nodiscard]] NamedTypeSymbol* type(NamedTypeSymbol::Kind where, const std::string& name) const noexcept;

    // Find the registered type symbol by its name and namespace.  Return nullptr if
    // no such type has been registered.
    [[nodiscard]] NamedTypeSymbol* type(TypeNamespace where, const std::string& name) const;

    // Find a registered type symbol by its name.  Return nullptr if no such type
    // has been registered.
    [[nodiscard]] NamedTypeSymbol* type(const std::string& name) const;

    void process_rename(NamedTypeSymbol* symbol, const std::string& old_name);

    TopLevelIterator top_level() noexcept;

    auto all_declarations()
    {
        return UniverseTypes<NamedTypeSymbol>{};
    }

    auto type_definitions()
    {
        return UniverseTypes<TypeDeclarationSymbol>{};
    }
};

template <class TypeSymbol> typename UniverseIterator<TypeSymbol>::reference UniverseIterator<TypeSymbol>::get() const
{
    auto* symbol = Universe::get().type(it_->first, it_->second);
    assert(symbol);
    if constexpr (std::is_same_v<TypeSymbol, NamedTypeSymbol>) {
        return *symbol;
    } else {
        assert(dynamic_cast<TypeSymbol*>(symbol));
        return static_cast<TypeSymbol&>(*symbol);
    }
}

template <class TypeSymbol> type_order_t::const_iterator find_first(type_order_t::const_iterator it)
{
    if constexpr (!std::is_same_v<TypeSymbol, NamedTypeSymbol>) {
        const auto& universe = Universe::get();
        for (auto e = universe.type_order_.cend(); it != e; ++it) {
            auto* symbol = universe.type(it->first, it->second);
            assert(symbol);
            if (dynamic_cast<TypeSymbol*>(symbol)) {
                break;
            }
        }
    }
    return it;
}

template <class TypeSymbol> UniverseIterator<TypeSymbol>& UniverseIterator<TypeSymbol>::operator++()
{
    it_ = find_first<TypeSymbol>(++it_);
    return *this;
}

template <class TypeSymbol> UniverseIterator<TypeSymbol> UniverseTypes<TypeSymbol>::cbegin() const
{
    return UniverseIterator<TypeSymbol>{find_first<TypeSymbol>(Universe::get().type_order_.cbegin())};
}

template <class TypeSymbol> UniverseIterator<TypeSymbol> UniverseTypes<TypeSymbol>::cend() const
{
    return UniverseIterator<TypeSymbol>{Universe::get().type_order_.cend()};
}

} // namespace objcgen

#endif // UNIVERSE_H
