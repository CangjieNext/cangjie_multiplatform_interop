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
    assert(symbol.defining_file());
    symbols_.insert(&symbol);
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
    return add_file(path);
}

} // namespace objcgen
