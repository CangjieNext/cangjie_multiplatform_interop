// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef OBJCINTEROPGEN_STAGE_H
#define OBJCINTEROPGEN_STAGE_H
#include <map>

namespace objcgen {

extern enum class Stage {
    PARSE_CMD = 1,
    PREPARE_MAPPINGS = 2,
    PARSE_OBJC = 3,
    CREATE_CJ_PACKAGES = 4,
    FILL_CJ_PACKAGES = 5,
    TRANSFORM = 6,
    APPLY_MAPPINGS = 7,
    CHECK_SOURCES = 8,
    WRITE_CJ_OUTPUTS = 9
} stage;

inline std::map<Stage, const char*> stage_to_description = {
    {Stage::PARSE_CMD, "Parsing command line options"},
    {Stage::PREPARE_MAPPINGS, "Preparing mappings"},
    {Stage::PARSE_OBJC, "Parsing Objective-C sources"},
    {Stage::CREATE_CJ_PACKAGES, "Creating Cangjie packages"},
    {Stage::FILL_CJ_PACKAGES, "Filling Cangjie packages"},
    {Stage::TRANSFORM, "Transforming sources"},
    {Stage::APPLY_MAPPINGS, "Applying mappings"},
    {Stage::CHECK_SOURCES, "Checking sources"},
    {Stage::WRITE_CJ_OUTPUTS, "Writing Cangjie outputs"},
};

} // namespace objcgen

#endif // OBJCINTEROPGEN_STAGE_H
