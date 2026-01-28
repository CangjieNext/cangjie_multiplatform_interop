// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Diagnostics.h"

#include <iostream>

#include "InputFile.h"
#include "Universe.h"

namespace objcgen {

void check_marked_symbols()
{
    for (auto&& type : Universe::get().all_declarations()) {
        if (type.package()) {
            if (type.is(NamedTypeSymbol::Kind::Union)) {
                const auto* input_file = type.defining_file();
                assert(input_file);
                std::cerr << input_file->path().u8string() << ": union `" << type.name()
                          << "` is an unsupported feature" << std::endl;
            }
            if (auto* t = dynamic_cast<TypeDeclarationSymbol*>(&type)) {
                for (const auto& member : t->members()) {
                    if (member.is_bit_field()) {
                        const auto* input_file = type.defining_file();
                        assert(input_file);
                        std::cerr << input_file->path().u8string() << ": bit-field `" << member.name() << "` of `"
                                  << type.name() << "` is an unsupported feature" << std::endl;
                    }
                }
            }
        }
    }
}

} // namespace objcgen
