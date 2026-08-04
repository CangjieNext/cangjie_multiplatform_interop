// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#import <stdint.h>

/**
 * Initialize Cangjie runtime and call LoadCJLibraryWithInit for `cj_gluecode_lib_name` library.
 */
bool initCJRuntime(const char* cj_gluecode_lib_name);
