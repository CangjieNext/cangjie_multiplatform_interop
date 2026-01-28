// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef CLANGSESSION_H
#define CLANGSESSION_H

#include <memory>
#include <string>
#include <vector>

namespace objcgen {

class ClangSession {
public:
    [[nodiscard]] static std::unique_ptr<ClangSession> create();

    virtual ~ClangSession() = default;

    virtual void parse_sources(const std::vector<std::string>& files, const std::vector<std::string>& arguments) = 0;

protected:
    explicit ClangSession() = default;
};

} // namespace objcgen

#endif // CLANGSESSION_H
