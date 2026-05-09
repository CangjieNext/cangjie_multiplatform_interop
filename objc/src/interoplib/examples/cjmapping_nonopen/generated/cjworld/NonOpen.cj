// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjworld

import objc.internal.*

// the class does not keep NativeObjCId as it is not suppose to support Foo overridden in heirs at ObjC side
public class NonOpen {
    func Foo(): Unit {
        println("[CJ]: NonOpen.Foo()")
    }
}

@C
public func CJImpl_ObjC_cjworld_NonOpen_initCJObject(): RegistryId {
    putToRegistry(NonOpen())
}

@C
public func CJImpl_ObjC_cjworld_NonOpen_Foo(registryId: RegistryId): Unit {
    getFromRegistryById<NonOpen>(registryId).Foo()
}

@C
public func CJImpl_ObjC_cjworld_NonOpen_deleteCJObject(registryId: RegistryId): Unit {
    removeFromRegistry(registryId)
}
