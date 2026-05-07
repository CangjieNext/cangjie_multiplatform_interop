// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package java.lang

import java.internal.*

@JavaMirror["[]"]
public class JArray<T> <: JObject {
    public init(length: Int32)
    public let length: Int32
    public operator func [](index: Int32): T
    public operator func [](index: Int32, value!: T): Unit
}