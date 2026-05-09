// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package objc.lang

import objc.internal.*

public struct ObjCPointer<T> { 
    public ObjCPointer(let ptr: CPointer<Unit>) {}
    public func isNull(): Bool { ptr.isNull() }
    public func read(): T { unsafe { zeroValue<T>() } }
    public func write(_: T): Unit {}
}
