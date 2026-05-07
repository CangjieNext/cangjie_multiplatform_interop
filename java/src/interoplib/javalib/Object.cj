// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package java.lang

public import java.internal.*

/*
 * Warning: package name `java.lang` and `JObject` name do matter for cjc
 */
@JavaMirror["java.lang.Object"]
public open class JObject {
    public init()
    @ForeignName["hashCode"]
    public open func hashCode32(): Int32
    public open func equals(obj: ?JObject): Bool
    public func toString(): String
    @ForeignName["toString"]
    public open func toJString(): JString
    public func wait(arg0: Int64, arg1: Int32): Unit
    public func wait(arg0: Int64): Unit
    public func wait(): Unit
    public func notifyAll(): Unit
    public func notify(): Unit
}

extend JObject {
    public func hashCode(): Int64 {
	    Int64(hashCode32())
    }
}
