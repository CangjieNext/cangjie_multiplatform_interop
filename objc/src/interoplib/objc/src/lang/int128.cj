// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package objc.lang

public import interoplib.objc.*

@C
public struct ObjCInt128 {
    public ObjCInt128(public let lo!: Int64, public let hi!: Int64) {}
    public init() { this(lo: 0, hi: 0) }
}

@C
public struct ObjCUInt128 {
    public ObjCUInt128(public let lo!: Int64, public let hi!: Int64) {}
    public init() { this(lo: 0, hi: 0) }
}
