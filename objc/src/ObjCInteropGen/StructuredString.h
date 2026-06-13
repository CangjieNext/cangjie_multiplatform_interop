// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef STRUCTUREDSTRING_H
#define STRUCTUREDSTRING_H

#include <iosfwd>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace objcgen {

class FileLevelSymbol;
struct StructuredStringRenderer;

/** Manipulator tags for StructuredString operator<<. */
struct PushLineCommentTag {};

struct PopLineCommentTag {};

struct PushBlockCommentTag {};

struct PopBlockCommentTag {};

struct IndentTag {};

struct DedentTag {};

inline constexpr PushLineCommentTag push_line_comment{};
inline constexpr PopLineCommentTag pop_line_comment{};
inline constexpr PushBlockCommentTag push_block_comment{};
inline constexpr PopBlockCommentTag pop_block_comment{};
inline constexpr IndentTag indent{};
inline constexpr DedentTag dedent{};

/**
 * A sequence of formatting events that can be rendered to text.  Symbol references
 * are recorded as events and turned into import lines only when rendered through
 * StructuredStringStream (non-commented references only).
 *
 * Line-comment push/pop events add a "// " prefix to subsequent line starts.
 * Block-comment push/pop write the opener/closer, and track nesting depth for import suppression.
 */
class StructuredString {
public:
    void print(std::ostream& out) const;

    void append(const StructuredString& other);

private:
    enum class EventKind {
        String,
        NewLine,
        PushLineComment,
        PopLineComment,
        PushBlockComment,
        PopBlockComment,
        Indent,
        Dedent,
        SymbolReference,
    };

    struct Event {
        EventKind kind;
        std::string text;
        const FileLevelSymbol* symbol = nullptr;
    };

    void push_string(std::string text);

    void push_event(EventKind kind, const FileLevelSymbol* symbol = nullptr);

    void append_text(std::string_view text);

    std::vector<Event> events_;

    friend struct StructuredStringRenderer;

    friend StructuredString& operator<<(StructuredString& stream, PushLineCommentTag);

    friend StructuredString& operator<<(StructuredString& stream, PopLineCommentTag);

    friend StructuredString& operator<<(StructuredString& stream, PushBlockCommentTag);

    friend StructuredString& operator<<(StructuredString& stream, PopBlockCommentTag);

    friend StructuredString& operator<<(StructuredString& stream, IndentTag);

    friend StructuredString& operator<<(StructuredString& stream, DedentTag);

    friend StructuredString& operator<<(StructuredString& stream, std::string_view text);

    friend StructuredString& operator<<(StructuredString& stream, const char* text);

    friend StructuredString& operator<<(StructuredString& stream, const std::string& text);

    friend StructuredString& operator<<(StructuredString& stream, char ch);

    friend StructuredString& operator<<(StructuredString& stream, const FileLevelSymbol& symbol);

    friend StructuredString& operator<<(StructuredString& stream, const StructuredString& other);
};

StructuredString& operator<<(StructuredString& stream, PushLineCommentTag);

StructuredString& operator<<(StructuredString& stream, PopLineCommentTag);

StructuredString& operator<<(StructuredString& stream, PushBlockCommentTag);

StructuredString& operator<<(StructuredString& stream, PopBlockCommentTag);

StructuredString& operator<<(StructuredString& stream, IndentTag);

StructuredString& operator<<(StructuredString& stream, DedentTag);

StructuredString& operator<<(StructuredString& stream, std::string_view text);

StructuredString& operator<<(StructuredString& stream, const char* text);

StructuredString& operator<<(StructuredString& stream, const std::string& text);

StructuredString& operator<<(StructuredString& stream, char ch);

template <class T>
std::enable_if_t<std::is_arithmetic_v<T>, StructuredString&> operator<<(StructuredString& stream, T value)
{
    return stream << std::to_string(value);
}

StructuredString& operator<<(StructuredString& stream, const FileLevelSymbol& symbol);

StructuredString& operator<<(StructuredString& stream, const StructuredString& other);

} // namespace objcgen

#endif // STRUCTUREDSTRING_H