// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjworld

public struct Vector {
    let x: Int32
    let y: Int32

    public init(x: Int32, y: Int32) {
        this.x = x
        this.y = y
    }

    public func dot(v: Vector): Int64 {
        let res: Int64 = Int64(x * v.x  + y * v.y)
        println("Hello from dot (${x}, ${y}) . (${v.x}, ${v.y}) = ${res}")
        return res;
    }

    public func add(v: Vector): Vector {
       let res = Vector(x + v.x, y + v.y)
       println("Hello from add (${x}, ${y}) + (${v.x}, ${v.y}), new Vector = (${res.x}, ${res.y})")
       return res
    }

    public static func processPrimitive(intArg: Int32, floatArg: Float64, boolArg: Bool): Unit {
        println("Hello from static processPrimitive: ${intArg * 2}, ${floatArg + 1.0} + ${!boolArg}")
    }
}
