// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjworld

import objc.lang.*

@ObjC
open foreign class M {
    public func pointerToUnit(): ObjCPointer<Unit>
    public func pointerToPrimitive(): ObjCPointer<Int64>
    public func pointerToPointer(): ObjCPointer<ObjCPointer<Float64>>
    public func pointerToMirror(): ObjCPointer<M>
    public func pointerToImpl(): ObjCPointer<A>
    public open func baseline(): Float64

    @ForeignName["init"]
    public init()

    public open func run(): Unit
}
