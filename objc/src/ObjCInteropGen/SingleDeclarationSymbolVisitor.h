// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SINGLEDECLARATIONSYMBOLVISITOR_H
#define SINGLEDECLARATIONSYMBOLVISITOR_H

#include "Symbol.h"

namespace objcgen {

class SingleDeclarationSymbolVisitor : public SymbolVisitor {
    bool initial_allow_recurse_;
    bool allow_recurse_;

public:
    explicit SingleDeclarationSymbolVisitor(const bool allow_recurse)
        : initial_allow_recurse_(allow_recurse), allow_recurse_(allow_recurse)
    {
    }

protected:
    virtual void visit_impl(FileLevelSymbol* owner, FileLevelSymbol* value, SymbolProperty property, bool foreign) = 0;

    void visit_impl(FileLevelSymbol* owner, FileLevelSymbol* value, SymbolProperty property) final;
};

} // namespace objcgen

#endif // SINGLEDECLARATIONSYMBOLVISITOR_H
