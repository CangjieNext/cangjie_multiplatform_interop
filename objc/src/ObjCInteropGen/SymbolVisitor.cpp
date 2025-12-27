// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "SymbolVisitor.h"

namespace objcgen {

template <class Value> void SymbolVisitor::do_visit_type(const Value& value)
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

void SymbolVisitor::visit_type(const Type& value)
{
    do_visit_type(value);
}

void SymbolVisitor::visit_type(const NamedTypeSymbol& value)
{
    do_visit_type(value);
}

void SymbolVisitor::visit_type_argument(const TypeLikeSymbol& owner, const Type& value)
{
    visit_type_argument_impl(owner, value);
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

void SymbolVisitor::visit_member(const NonTypeSymbol& value)
{
    // If recursion is not allowed, skip this property (don't collect and don't recurse into it).
    if (allow_recurse_) {
        visit_member_impl(value);

        // Members should be walked fully if we still walk them.
        value.visit_impl(*this);
    }
}

void SymbolVisitor::visit(const FileLevelSymbol& symbol)
{
    // We just started the walk, don't bail out immediately.
    assert(allow_recurse_ || is<const TypeLikeSymbol&>(symbol));
    visit_impl(symbol);
    symbol.visit_impl(*this);
}

void SymbolVisitor::visit(const Type& type)
{
    visit(type.symbol());
    type.visit_impl(*this);
}

} // namespace objcgen
