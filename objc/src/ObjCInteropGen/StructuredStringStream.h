// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef STRUCTUREDSTRINGSTREAM_H
#define STRUCTUREDSTRINGSTREAM_H

#include <functional>
#include <iosfwd>
#include <set>
#include <sstream>
#include <string>

#include "StructuredString.h"

namespace objcgen {

class Package;

void render(std::ostream& out, const StructuredString& structured,
    const std::function<void(const FileLevelSymbol&)>* on_reference = nullptr);

/**
 * Renders StructuredString events into a string buffer and collects import names
 * for non-commented FileLevelSymbol references from other packages.  Imports are
 * derived from the same render pass as the output text, so they stay in sync with
 * what is actually printed.
 */
class StructuredStringStream final : public std::ostream {
public:
    explicit StructuredStringStream(const Package& package);

    StructuredStringStream& operator<<(const StructuredString& structured);

    [[nodiscard]] std::string str() const;

    [[nodiscard]] const std::set<std::string>& imports() const noexcept
    {
        return imports_;
    }

private:
    void on_symbol_reference(const FileLevelSymbol& symbol);

    std::stringbuf buf_;
    const Package& package_;
    std::set<std::string> imports_;
};

/**
 * RAII helper: opening brace, newline, indent on construction; dedent, closing
 * brace on destruction.
 */
class BraceScope {
public:
    explicit BraceScope(StructuredString& output);

    ~BraceScope();

    BraceScope(const BraceScope&) = delete;

    BraceScope& operator=(const BraceScope&) = delete;

private:
    StructuredString& output_;
};

} // namespace objcgen

#endif // STRUCTUREDSTRINGSTREAM_H