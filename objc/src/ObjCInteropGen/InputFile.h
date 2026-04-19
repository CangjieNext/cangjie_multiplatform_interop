// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef INPUTFILE_H
#define INPUTFILE_H

#include <climits>
#include <deque>
#include <filesystem>
#include <set>
#include <unordered_set>
#include <vector>

#include "Collection.h"

namespace objcgen {

class FileLevelSymbol;
class InputDirectory;

struct LineCol {
    struct Hash {
        [[nodiscard]] size_t operator()(const LineCol& pos) const noexcept
        {
            constexpr auto half_width = sizeof(size_t) * CHAR_BIT / 2;
            return (static_cast<size_t>(pos.line_) << half_width) + pos.line_;
        }
    };

    [[nodiscard]] friend bool operator<(const LineCol& loc1, const LineCol& loc2) noexcept
    {
        return loc1.line_ < loc2.line_ || (loc1.line_ == loc2.line_ && loc1.col_ < loc2.col_);
    }

    [[nodiscard]] friend bool operator==(const LineCol& loc1, const LineCol& loc2) noexcept
    {
        return loc1.line_ == loc2.line_ && loc1.col_ == loc2.col_;
    }

    unsigned line_;
    unsigned col_;
};

struct Location {
    std::filesystem::path file_;
    LineCol pos_;

    [[nodiscard]] bool is_null() const noexcept
    {
        return file_.empty();
    }
};

class InputFile final {
    struct SymbolComparator {
        bool operator()(const FileLevelSymbol* symbol1, const FileLevelSymbol* symbol2) const noexcept;
    };

    const std::filesystem::path path_;

    std::unordered_set<LineCol, LineCol::Hash> cursors_up_to_this_translation_;
    std::unordered_set<LineCol, LineCol::Hash> cursors_in_this_translation_;

    std::multiset<FileLevelSymbol*, SymbolComparator> symbols_;

    friend class FileLevelSymbol;

    void add_symbol(FileLevelSymbol& symbol);

public:
    [[nodiscard]] InputFile(std::filesystem::path path) noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

    [[nodiscard]] auto begin() const noexcept
    {
        return PointerIterator<decltype(symbols_.begin())>(symbols_.begin());
    }

    [[nodiscard]] auto end() const noexcept
    {
        return PointerIterator<decltype(symbols_.end())>(symbols_.end());
    }

    void next_translation();

    [[nodiscard]] bool add_cursor(const LineCol& location);
};

class InputDirectory final {
    const std::filesystem::path path_;
    std::vector<InputFile*> files_;

public:
    [[nodiscard]] explicit InputDirectory(std::filesystem::path path) : path_(std::move(path))
    {
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

    [[nodiscard]] auto begin() const noexcept
    {
        return files_.begin();
    }

    [[nodiscard]] auto end() const noexcept
    {
        return files_.end();
    }

    [[nodiscard]] InputFile& add_file(InputFile& input_file)
    {
        return *files_.emplace_back(&input_file);
    }
};

class Inputs final {
public:
    [[nodiscard]] InputFile& operator[](const std::filesystem::path& path);

    [[nodiscard]] auto begin() const noexcept
    {
        return PointerIterator<decltype(files_.begin())>(files_.begin());
    }

    [[nodiscard]] auto end() const noexcept
    {
        return PointerIterator<decltype(files_.end())>(files_.end());
    }

    [[nodiscard]] InputFile& add_file(InputFile& input_file)
    {
        return *files_.emplace_back(&input_file);
    }

    void next_translation();

    [[nodiscard]] bool add_cursor(const Location& location, const std::string& name);

private:
    std::deque<InputFile*> files_;

    std::set<std::string> builtin_cursors_up_to_this_translation_;
    std::set<std::string> builtin_cursors_in_this_translation_;
};

extern Inputs inputs;

} // namespace objcgen

#endif // INPUTFILE_H
