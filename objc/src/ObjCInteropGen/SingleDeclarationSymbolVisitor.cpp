// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "SingleDeclarationSymbolVisitor.h"

namespace objcgen {

static bool should_always_collect(const SymbolProperty property)
{
    switch (property) {
        case SymbolProperty::None:
            // We just started the walk, don't bail out immediately.
            return true;
        case SymbolProperty::TypeArgument:
        case SymbolProperty::TupleItem:
        case SymbolProperty::FunctionParametersTuple:
        case SymbolProperty::FunctionReturnType:
            // Everything that is present in Cangjie type reference in the source code returns true
            return true;
        case SymbolProperty::Base:
        case SymbolProperty::Member:
        case SymbolProperty::AliasTarget:
        case SymbolProperty::ParameterType:
        case SymbolProperty::ReturnType:
            return false;
        default:
            assert(false);
            return false;
    }
}

static bool should_disable_recursion(const SymbolProperty property)
{
    switch (property) {
        case SymbolProperty::None:
            // We just started the walk, don't bail out immediately.
            return false;
        case SymbolProperty::Member:
            // Members should be walked fully if we still walk them.
            return false;
        case SymbolProperty::TypeArgument:
        case SymbolProperty::Base:
        case SymbolProperty::TupleItem:
        case SymbolProperty::FunctionParametersTuple:
        case SymbolProperty::FunctionReturnType:
        case SymbolProperty::AliasTarget:
        case SymbolProperty::ParameterType:
        case SymbolProperty::ReturnType:
            // These go to a separate declaration, collect them, but not their contents.
            return true;
        default:
            assert(false);
            return false;
    }
}

void SingleDeclarationSymbolVisitor::visit_impl(
    FileLevelSymbol* owner, FileLevelSymbol* value, const SymbolProperty property)
{
    if (!allow_recurse_) {
        if (should_always_collect(property)) {
            assert(dynamic_cast<TypeLikeSymbol*>(value));
            visit_impl(owner, value, property, true);
            recurse(value);
            return;
        }

        // Skip this property (don't collect and don't recurse into it).
        return;
    }

    visit_impl(owner, value, property, !allow_recurse_);

    if (initial_allow_recurse_ && should_disable_recursion(property)) {
        assert(allow_recurse_);
        allow_recurse_ = false;
        // We still recurse, because we need to collect type arguments and the like.
        recurse(value);
        allow_recurse_ = true;
    } else {
        recurse(value);
    }
}

} // namespace objcgen
