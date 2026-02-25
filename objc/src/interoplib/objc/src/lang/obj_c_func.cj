// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package objc.lang

public struct ObjCFunc<F> {
    @Frozen
    public ObjCFunc(let ptr: CPointer<CFunc<() -> Unit>>) {}
    public prop call: F {
        get() { throw Exception("ObjCFunc.call is implemented by compiler") }
    }

    public unsafe func unsafeGetFunctionPointer(): CPointer<CFunc<() -> Unit>> {
        return ptr;
    }
}
