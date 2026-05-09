// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjworld

import objc.internal.*

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

@C
public func CJImpl_ObjC_cjworld_Vector_dot(registryId: RegistryId, anotherRegistryId: RegistryId): Int64 {
    getFromRegistryById<Vector>(registryId).dot(
        getFromRegistryById<Vector>(anotherRegistryId)
    )
}

@C
public func CJImpl_ObjC_cjworld_Vector_add(registryId: RegistryId, anotherRegistryId: RegistryId): RegistryId {
    let res = getFromRegistryById<Vector>(registryId).add(
        getFromRegistryById<Vector>(anotherRegistryId)
    )
    putToRegistry(res)
}

@C
public func CJImpl_ObjC_cjworld_Vector_processPrimitive(intArg: Int32, floatArg: Float64, boolArg: Bool): Unit {
   Vector.processPrimitive(intArg, floatArg, boolArg)
}

@C
public func CJImpl_ObjC_cjworld_Vector_initCJObject(x: Int32, y:Int32): RegistryId {
    putToRegistry(Vector(x, y))
}

@C
public func CJImpl_ObjC_cjworld_Vector_XField_get(registryId: RegistryId): Int32 {
    getFromRegistryById<Vector>(registryId).x
}

@C
public func CJImpl_ObjC_cjworld_Vector_YField_get(registryId: RegistryId): Int32 {
    getFromRegistryById<Vector>(registryId).y
}

@C
public func CJImpl_ObjC_cjworld_Vector_deleteCJObject(registryId: RegistryId): Unit {
    removeFromRegistry(registryId)
}
