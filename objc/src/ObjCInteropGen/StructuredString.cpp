// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "StructuredString.h"

#include <cassert>
#include <ostream>

#include "StructuredStringStream.h"
#include "Symbol.h"

namespace objcgen {

void StructuredString::push_string(std::string text)
{
    assert(text.find('\n') == std::string::npos);
    assert(text.find('\r') == std::string::npos);
    events_.push_back({EventKind::String, std::move(text), nullptr});
}

void StructuredString::push_event(const EventKind kind, const FileLevelSymbol* symbol)
{
    events_.push_back({kind, {}, symbol});
}

void StructuredString::append(const StructuredString& other)
{
    events_.insert(events_.end(), other.events_.begin(), other.events_.end());
}

void StructuredString::print(std::ostream& out) const
{
    render(out, *this, nullptr);
}

void StructuredString::append_text(const std::string_view text)
{
    std::string_view fragment = text;
    while (!fragment.empty()) {
        const auto newline_pos = fragment.find('\n');
        if (newline_pos == std::string_view::npos) {
            push_string(std::string(fragment));
            break;
        }
        if (newline_pos > 0) {
            push_string(std::string(fragment.substr(0, newline_pos)));
        }
        push_event(EventKind::NewLine);
        fragment.remove_prefix(newline_pos + 1);
    }
}

StructuredString& operator<<(StructuredString& stream, PushLineCommentTag)
{
    stream.push_event(StructuredString::EventKind::PushLineComment);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, PopLineCommentTag)
{
    stream.push_event(StructuredString::EventKind::PopLineComment);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, PushBlockCommentTag)
{
    stream.push_event(StructuredString::EventKind::PushBlockComment);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, PopBlockCommentTag)
{
    stream.push_event(StructuredString::EventKind::PopBlockComment);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, IndentTag)
{
    stream.push_event(StructuredString::EventKind::Indent);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, DedentTag)
{
    stream.push_event(StructuredString::EventKind::Dedent);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, std::string_view text)
{
    stream.append_text(text);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, const char* text)
{
    return stream << std::string_view(text);
}

StructuredString& operator<<(StructuredString& stream, const std::string& text)
{
    return stream << std::string_view(text);
}

StructuredString& operator<<(StructuredString& stream, const char ch)
{
    if (ch == '\n') {
        stream.push_event(StructuredString::EventKind::NewLine);
    } else {
        assert(ch != '\r');
        stream.push_string(std::string(1, ch));
    }
    return stream;
}

StructuredString& operator<<(StructuredString& stream, const FileLevelSymbol& symbol)
{
    stream.push_event(StructuredString::EventKind::SymbolReference, &symbol);
    return stream;
}

StructuredString& operator<<(StructuredString& stream, const StructuredString& other)
{
    stream.append(other);
    return stream;
}

} // namespace objcgen