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
     */
    void visit_type(const Type& value)
    {
        do_visit_type(value);
    }

    /** One of the following:
     * - Base type of a type declaration
     * - Underlying type of an enumeration declaration
     */
    void visit_type(const NamedTypeSymbol& value)
    {
        do_visit_type(value);
    }

    /** One of the following:
     * - Type argument of a type declaration
     * - Pointee of a pointer type
     * - Parameter or return type of a function-like type
     * - Element type of VArray
     * - Underlying type of an unexposed type
     */
    void visit_type_argument(const Type& value)
    {
        visit_type_argument_impl(value);
        if (allow_recurse_ && initial_allow_recurse_) {
            // These go to a separate declaration, collect them, but not their contents.
            allow_recurse_ = false;
            // We still recurse, because we need to collect type arguments and the like.
            value.visit_impl(*this);
            allow_recurse_ = true;
        } else {
            // Everything that is present in Cangjie type reference in the source code
            // should be always collected
            value.visit_impl(*this);
        }
    }

    /** Member (that is, non-type) of a type declaration */
    void visit_member(const NonTypeSymbol& value)
    {
        // If recursion is not allowed, skip this property (don't collect and don't recurse into it).
        if (allow_recurse_) {
            visit_member_impl(value);

            // Members should be walked fully if we still walk them.
            value.visit_impl(*this);
        }
    }

    void visit(const FileLevelSymbol& symbol)
    {
        // We just started the walk, don't bail out immediately.
        assert(allow_recurse_ || symbol.is<TypeLikeSymbol>());
        visit_impl(symbol);
        symbol.visit_impl(*this);
    }

    void visit(const Type& type)
    {
        visit(type.symbol());
        type.visit_impl(*this);
    }

protected:
    explicit SymbolVisitor(bool initial_allow_recurse) noexcept
        : initial_allow_recurse_(initial_allow_recurse), allow_recurse_(initial_allow_recurse)
    {
    }

private:
    const bool initial_allow_recurse_;
    bool allow_recurse_;

    template <class Value> void do_visit_type(const Value& value)
    {
        // If recursion is not allowed, skip this property (don't collect and don't recurse into it).
        if (allow_recurse_) {
            visit_type_impl(value);
            if (initial_allow_recurse_) {
                // These go to a separate declaration, collect them, but not their contents.
                allow_recurse_ = false;
                // We still recurse, because we need to collect type arguments and the like.
                value.visit_impl(*this);
                allow_recurse_ = true;
            } else {
                value.visit_impl(*this);
            }
        }
    }

    virtual void visit_type_impl(const Type& value) = 0;

    virtual void visit_type_impl(const NamedTypeSymbol& value) = 0;

    virtual void visit_type_argument_impl(const Type& value) = 0;

    virtual void visit_member_impl(const NonTypeSymbol& value) = 0;

    virtual void visit_impl(const FileLevelSymbol& symbol) = 0;
};

} // namespace objcgen

#endif // SYMBOLVISITOR_H
