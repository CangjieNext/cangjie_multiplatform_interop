// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#import <stddef.h>
#import <Foundation/NSObjCRuntime.h>

/**
 * Initialize Cangjie runtime and call LoadCJLibraryWithInit for `cj_gluecode_lib_name` library.
 */
bool initCJRuntime(const char* cj_gluecode_lib_name);

/**
 * Calculate override mask for methods in `methods` array of `baseCls` class that may be overridden in `selfCls` class.
 * The length of `methods` array is passed as `len`. If it is greater than 64 only first 64 methods participate in the mask.
 * The i-th bit of the calculated mask is set to 1, when the i-th method is overridden in `selfCls`.
 */
uint64_t calcOverrideMask(Class baseCls, Class selfCls, SEL* methods, int len);
