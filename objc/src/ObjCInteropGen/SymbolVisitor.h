// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SYMBOLVISITOR_H
#define SYMBOLVISITOR_H

#include "NonCopyable.h"
#include "Symbol.h"

namespace objcgen {

enum class SymbolProperty {
    None,
    TypeArgument,
    Base,
    Member,
    TupleItem,
    FunctionParametersTuple,
    FunctionReturnType,
    AliasTarget,
    ParameterType,
    ReturnType,
};

class SymbolVisitor : NonCopyable {
public:
    explicit SymbolVisitor(const bool allow_recurse)
        : initial_allow_recurse_(allow_recurse), allow_recurse_(allow_recurse)
    {
    }

    virtual ~SymbolVisitor() = default;

    void recurse(FileLevelSymbol* value);

    void visit(FileLevelSymbol* owner, FileLevelSymbol* value, const SymbolProperty property);

    void visit(FileLevelSymbol* symbol)
    {
        visit(nullptr, symbol, SymbolProperty::None);
    }

protected:
    virtual void visit_impl(FileLevelSymbol* owner, FileLevelSymbol* value, SymbolProperty property) = 0;

private:
    bool initial_allow_recurse_;
    bool allow_recurse_;
};

} // namespace objcgen

#endif // SYMBOLVISITOR_H
