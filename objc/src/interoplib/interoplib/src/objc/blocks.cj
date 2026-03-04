// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.objc

import std.sync.*

/*
* Cangjie does not support referencing global symbols that are not functions,
* so we provide a function declaration instead
*/
foreign func _NSConcreteStackBlock(): Unit

/*
* block descriptor, use all defaults for 'our' blocks
*/
@C
public struct NativeBlockDescriptorABI {
    NativeBlockDescriptorABI(
        let reserved!: UInt64 = 0, // should be zero
        let size!: UInt64, // equals to the size of block (= sizeOf<NativeBlockABI> or = sizeOf<CangjieBlockABI>),
        let copy!: CFunc<(CPointer<NativeBlockABI>, CPointer<NativeBlockABI>) -> Unit>, // copy routine
        let dispose!: CFunc<(CPointer<NativeBlockABI>) -> Unit> // dispose routine
    ) {}
}

/*
* block structure, see block ABI
* default values are defaults that we provide when constructing block from Cangjie
*/
@C
public struct NativeBlockABI {
    public NativeBlockABI(
        public let isa!: CPointer<Unit>,
        public let flags!: Int32, // see ABI description
        public let reserved!: Int32, // must be zero
        public let invoke!: CPointer<CFunc<(CPointer<NativeBlockABI>) -> Unit>>, // the actual function pointer to implementation
        public let descriptor!: CPointer<NativeBlockDescriptorABI> // pointer to descriptor
    ) {}
}

/*
* for blocks created from Cangjie, we additionally store the registry id in the structure
*/
@C
public struct CangjieBlockABI {
    public CangjieBlockABI(
        let base!: NativeBlockABI,
        let registryId!: RegistryId
    ) {}
}

/*
* reference counter box for lambda objects, used internally by generated code
*/
public class ObjCLambdaWrapper {
    ObjCLambdaWrapper(
        let lambda: Any,
        let counter!: AtomicInt64 = AtomicInt64(1)
    ) {}
}

/*
* default copy callback, read lambda wrapper from registry and increment ref counter
*/
@C
public func cangjieBlockCopy(src: CPointer<NativeBlockABI>, _: CPointer<NativeBlockABI>): Unit {
    unsafe {
        let regid = CPointer<CangjieBlockABI>(src).read().registryId
        getFromRegistryById<ObjCLambdaWrapper>(regid).counter.fetchAdd(1)
    }
}

/*
* default disposed callback, decrement the counter and if it reaches 0, remove and free the object
*/
@C
public func cangjieBlockDispose(src: CPointer<NativeBlockABI>): Unit {
    unsafe {
        let regid = CPointer<CangjieBlockABI>(src).read().registryId
        if (getFromRegistryById<ObjCLambdaWrapper>(regid).counter.fetchSub(1) <= 2) {
            removeFromRegistry(regid)
            LibC.free(src)
        }
    }
}

/*
* default descriptor for cangjie-created blocks
*/
private var cangjieBlockDescriptor = NativeBlockDescriptorABI(
    reserved: 0, // must be zero
    size: unsafe { UInt64(sizeOf<CangjieBlockABI>()) },
    copy: cangjieBlockCopy,
    dispose: cangjieBlockDispose
)

/*
* trick function to take pointer from global variable, a little fragile, please pay attention
*/ 
@C
private func pCangjieBlockDescriptor(ptr: CPointer<NativeBlockDescriptorABI>): CPointer<NativeBlockDescriptorABI> {
    return ptr
}

/*
* For a given lambda f of type F, the wrapping glue code looks like this:
* registerCangjieLambdaAsBlock(f, CFunc { native: CPointer<NativeBlockABI>, args ... =>
*     // the same lambda object from registry
*     let f = getCangjieLambdaFromBlock<F>(native)
*     // real gluecode may omit the temporary variable
*     return UNWRAP(f(WRAP(args)...))
* })
*
* for example, for f: (A, Int64) -> Float64
* registerCangjieLambdaAsBlock(f, CFunc<(CPointer<NativeBlockABI>, NativeObjCId, Int64) -> Float64> {
*       native, arg1, arg2 =>
*       let f: (A, Int64) -> Float64 = getCangjieLambdaFromBlock<(A, Int64) -> Float64>(native)
*       return f(arg1.$obj, arg2)
* })
* The result of this function is malloc'ed and needs to be freed using LibC.free
*/
public func registerCangjieLambdaAsBlock(ref: Any, body: CPointer<CFunc<(CPointer<NativeBlockABI>) -> Unit>>): CPointer<CangjieBlockABI> {
    unsafe {
        let regid = putToRegistry(ObjCLambdaWrapper(ref))
        let resultStack = CangjieBlockABI(
            base: NativeBlockABI(
                isa: CPointer<Unit>(_NSConcreteStackBlock),
                flags: 1 << 27, // required to employ copy/destroy functions, see block ABI
                reserved: 0, // must be zero
                invoke: body,
                descriptor: pCangjieBlockDescriptor(inout cangjieBlockDescriptor)
            ),
            registryId: regid
        )
        let resultHeap = LibC.malloc<CangjieBlockABI>()
        resultHeap.write(resultStack)
        return resultHeap
    }
}

// read lambda object from block ABI pointer
// assumes that this block is created by Cangjie
@Frozen
public func getCangjieLambdaFromBlock<F>(block: CPointer<NativeBlockABI>): F {
    unsafe {
        // here we could additionally access the descriptor and check that size >= sizeof<CangjieBlockABI>()
        // but this can only happen if the user uses this function explicitly
        let cangjieBlock = CPointer<CangjieBlockABI>(block).read()
        let wrapper = getFromRegistryById<ObjCLambdaWrapper>(cangjieBlock.registryId)
        return (wrapper.lambda as F) ?? throw IllegalStateException("Illegal type")
    }
}
