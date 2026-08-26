// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "StructuredStringStream.h"

#include <cassert>
#include <functional>

#include "Package.h"
#include "Symbol.h"

namespace objcgen {

namespace {

constexpr char INDENT[] = "    ";
constexpr char LINE_COMMENT[] = "// ";

} // namespace

struct StructuredStringRenderer {
    static void render(std::ostream& out, const StructuredString& structured,
        const std::function<void(const FileLevelSymbol&)>* on_reference);
};

void StructuredStringRenderer::render(std::ostream& out, const StructuredString& structured,
    const std::function<void(const FileLevelSymbol&)>* on_reference)
{
    size_t indent_level = 0;
    size_t line_comment_depth = 0;
    size_t block_comment_depth = 0;
    bool at_line_start = true;

    const auto emit_line_prefix = [&] {
        if (!at_line_start) {
            return;
        }
        for (size_t i = 0; i < indent_level; ++i) {
            out << INDENT;
        }
        for (size_t i = 0; i < line_comment_depth; ++i) {
            out << LINE_COMMENT;
        }
        at_line_start = false;
    };

    for (const auto& event : structured.events_) {
        switch (event.kind) {
            case StructuredString::EventKind::String:
                emit_line_prefix();
                out << event.text;
                break;
            case StructuredString::EventKind::NewLine:
                emit_line_prefix();
                out << '\n';
                at_line_start = true;
                break;
            case StructuredString::EventKind::Indent:
                ++indent_level;
                break;
            case StructuredString::EventKind::Dedent:
                assert(indent_level > 0);
                --indent_level;
                break;
            case StructuredString::EventKind::PushLineComment:
                ++line_comment_depth;
                break;
            case StructuredString::EventKind::PopLineComment:
                assert(line_comment_depth > 0);
                --line_comment_depth;
                break;
            case StructuredString::EventKind::PushBlockComment:
                emit_line_prefix();
                out << "/*";
                ++block_comment_depth;
                break;
            case StructuredString::EventKind::PopBlockComment:
                emit_line_prefix();
                // When nested inside another block comment, close with "* /" so the
                // outer comment is not terminated prematurely.
                out << (block_comment_depth > 1 ? "* /" : "*/");
                assert(block_comment_depth > 0);
                --block_comment_depth;
                break;
            case StructuredString::EventKind::SymbolReference:
                if (on_reference && line_comment_depth == 0 && block_comment_depth == 0) {
                    (*on_reference)(*event.symbol);
                }
                break;
            default:
                assert(false);
                return;
        }
    }

    assert(indent_level == 0);
    assert(line_comment_depth == 0);
    assert(block_comment_depth == 0);
}

void render(std::ostream& out, const StructuredString& structured,
    const std::function<void(const FileLevelSymbol&)>* on_reference)
{
    StructuredStringRenderer::render(out, structured, on_reference);
}

StructuredStringStream::StructuredStringStream(const Package& package) : std::ostream(&buf_), package_(package)
{
}

void StructuredStringStream::on_symbol_reference(const FileLevelSymbol& symbol)
{
    const auto* symbol_package = symbol.package();
    if (symbol_package && symbol_package != &package_) {
        imports_.emplace(symbol_package->cangjie_name() + '.' + symbol.name());
    }
}

StructuredStringStream& StructuredStringStream::operator<<(const StructuredString& structured)
{
    std::function on_reference = [this](const FileLevelSymbol& symbol) { on_symbol_reference(symbol); };
    render(*this, structured, &on_reference);
    return *this;
}

std::string StructuredStringStream::str() const
{
    return buf_.str();
}

BraceScope::BraceScope(StructuredString& output) : output_(output)
{
    output_ << " {\n" << indent;
}

BraceScope::~BraceScope()
{
    output_ << dedent << '}';
}

} // namespace objcgen