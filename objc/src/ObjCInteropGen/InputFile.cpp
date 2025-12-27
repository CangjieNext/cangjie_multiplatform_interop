// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "InputFile.h"

#include "Symbol.h"

namespace objcgen {

Inputs inputs;

bool InputFile::SymbolComparator::operator()(
    const FileLevelSymbol* symbol1, const FileLevelSymbol* symbol2) const noexcept
{
    return *symbol1 < *symbol2;
}

void InputFile::add_symbol(FileLevelSymbol& symbol)
{
    assert(symbol.is_file_level());
    symbols_.insert(&symbol);
}

void InputFile::next_translation()
{
    cursors_up_to_this_translation_.merge(cursors_in_this_translation_);
    cursors_in_this_translation_.clear();
}

bool InputFile::add_cursor(const LineCol& location)
{
    if (cursors_up_to_this_translation_.find(location) != cursors_up_to_this_translation_.end()) {
        return false;
    }
    cursors_in_this_translation_.emplace(location);
    return true;
}

InputFile::InputFile(std::filesystem::path path) noexcept : path_(std::move(path))
{
}

InputFile& Inputs::operator[](const std::filesystem::path& path)
{
    for (auto* file : files_) {
        if (file->path() == path) {
            return *file;
        }
    }
    return add_file(*new InputFile(path));
}

void Inputs::next_translation()
{
    for (auto* file : files_) {
        file->next_translation();
    }
    builtin_cursors_up_to_this_translation_.merge(builtin_cursors_in_this_translation_);
    builtin_cursors_in_this_translation_.clear();
}

bool Inputs::add_cursor(const Location& location, const std::string& name)
{
    if (location.is_null()) {
        if (builtin_cursors_up_to_this_translation_.find(name) != builtin_cursors_up_to_this_translation_.end()) {
            return false;
        }
        builtin_cursors_in_this_translation_.emplace(name);
        return true;
    }
    return (*this)[location.file_].add_cursor(location.pos_);
}

} // namespace objcgen
