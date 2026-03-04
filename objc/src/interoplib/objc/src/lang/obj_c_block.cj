// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package objc.lang

public class ObjCBlock<F> {
    // is this malloc'ed from Cangjie?
    let cangjieOwnedPtr: Bool
    // actual pointer to block ABI structure
    let ptr: CPointer<NativeBlockABI>
    
    // this constructor is used to construct block object from Objective-C native block pointer
    public init(ptr: CPointer<NativeBlockABI>) {
        this.ptr = ptr
        this.cangjieOwnedPtr = false
    }
    
    // this constructor is used to construct block object from Cangjie
    public init(ptr: CPointer<CangjieBlockABI>) {
        this.ptr = CPointer<NativeBlockABI>(ptr)
        this.cangjieOwnedPtr = true
    }
    
    public init(f: F) {
        throw Exception("Function constructor is implemented by the compiler")
    }
    public prop call: F {
        get() { throw Exception("ObjCBlock.call is implemented by compiler") }
    }
    public unsafe func unsafeGetNativeABIPointer(): CPointer<Unit> {
        return CPointer<Unit>(ptr);
    }
    public unsafe func unsafeGetFunctionPointer(): CPointer<CFunc<(CPointer<Unit>) -> Unit>> {
        return CPointer<CFunc<(CPointer<Unit>) -> Unit>>(ptr.read().invoke);
    }
    ~init() {
        unsafe {
            if (cangjieOwnedPtr) {
                // if we own the pointer, we need to decrement counter and
                // potentially free the structure
                // but dispose callback already does both of those things
                cangjieBlockDispose(this.ptr)
                // we cannot just free it here, because there may still be users in Objective-C
            }
            objCRelease(NativeObjCId(this.ptr))
        }
    }
}
