// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SYMBOLVISITOR_H
#define SYMBOLVISITOR_H

#include "Symbol.h"

namespace objcgen {

class SymbolVisitor : NonCopyable {
public:
    virtual ~SymbolVisitor() = default;

    /** One of the following:
     * - Target type of a type alias declaration
     * - Parameter or return type of a method or global function
     * - Type of a field or instance variable
     * - Type argument of a type declaration
     * - Pointee of a pointer type
     * - Parameter or return type of a function-like type
     * - Element type of VArray
     * - Underlying type of an unexposed type
     */
    void visit_type(const Type& type, bool recurse)
    {
        visit_type_impl(type);
        type.visit_impl(*this, !initial_allow_recurse_ && recurse);
    }

    /** One of the following:
     * - Base type of a type declaration
     * - Underlying type of an enumeration declaration
     */
    void visit_type(const NamedTypeSymbol& type_symbol)
    {
        visit_type_impl(type_symbol);
        type_symbol.visit_impl(*this, !initial_allow_recurse_);
    }

    /** Member (that is, non-type) of a type declaration */
    void visit_member(const NonTypeSymbol& member)
    {
        visit_member_impl(member);

        // Members should be walked fully if we still walk them.
        member.visit_impl(*this, true);
    }

    void visit(const FileLevelSymbol& symbol)
    {
        // We just started the walk, don't bail out immediately.
        assert(initial_allow_recurse_ || symbol.is<TypeLikeSymbol>());
        visit_impl(symbol);
        symbol.visit_impl(*this, initial_allow_recurse_);
    }

    void visit(const Type& type)
    {
        visit(type.symbol());
        type.visit_impl(*this, initial_allow_recurse_);
    }

protected:
    explicit SymbolVisitor(bool initial_allow_recurse) noexcept : initial_allow_recurse_(initial_allow_recurse)
    {
    }

private:
    const bool initial_allow_recurse_;

    virtual void visit_type_impl(const Type& type) = 0;

    virtual void visit_type_impl(const NamedTypeSymbol& type_symbol) = 0;

    virtual void visit_member_impl(const NonTypeSymbol& type_symbol) = 0;

    virtual void visit_impl(const FileLevelSymbol& symbol) = 0;
};

} // namespace objcgen

#endif // SYMBOLVISITOR_H
