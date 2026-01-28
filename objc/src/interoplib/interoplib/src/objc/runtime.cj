// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.objc

import interoplib.common.REGISTRY

/**
 * id or Instance Method Pointer type.
 */
public type NativeObjCId = CPointer<Unit>

/**
 * SEL.
 */
public type NativeObjCSel = CPointer<Unit>

/**
 * Class.
 */
public type NativeObjCClass = CPointer<Unit>

/**
 * Protocol.
 */
public type NativeObjCProtocol = CPointer<Unit>

/**
 * Ivar.
 */
public type NativeObjCIvar = CPointer<Unit>

/**
 * objc_super*
 * Could be convenient for msgSendSuper calls.
 */
public type NativeObjCSuperPtr = CPointer<NativeObjCSuper>

@C
/**
 * objc_super
 */
public struct NativeObjCSuper {
    public NativeObjCSuper(let receiver: NativeObjCId, let super_class: NativeObjCClass) {}
}

/**
 * An opaque identifier for Cangjie mirror objects.
 */
public type RegistryId = Int64

/**
 * An exception that has to be used to mark an unreachable code (e.g. instantiation @ObjCImpl objects from Cangjie side).
 */
public class ObjCUnreachableCodeException <: Exception {}

@Frozen
/**
 * Gets a Cangjie object of type R from registry by the pointer of its Objective-C counterpart.
 */
public unsafe func getFromRegistryByHandle<R>(id: NativeObjCId): R {
    const REGISTRY_ID_IDENT = "$registryId"

    getFromRegistryById<R>(
        CFunc<(NativeObjCId, NativeObjCSel) -> RegistryId>(ObjCRuntime.msgSend)(id, registerName(REGISTRY_ID_IDENT))
    )
}

@Frozen
/**
 * Gets a Cangjie object of type R from registry by the id.
 */
public unsafe func getFromRegistryById<R>(registryId: RegistryId): R {
    REGISTRY.get<R>(registryId).getOrThrow()
}

/**
 * Puts a Cangjie object to registry.
 */
public unsafe func putToRegistry(any: Any): RegistryId {
    REGISTRY.put(any)
}

/**
 * Removes a Cangjie object from registry by its id.
 */
public unsafe func removeFromRegistry(registryId: RegistryId): Unit {
    REGISTRY.remove(registryId)
}

/**
 * Allocates a pointer to the class by its name.
 * Effectively a wrapper arround [cls alloc], but with Cangjie String as a parameter for convenience.
 * Note: MUST be called in a transaction with corresponding init* method of the class and the pointer, returned by this function, MUST NOT be stored anywhere.
 */
public unsafe func alloc(className: String): NativeObjCId {
    const ALLOC_SEL = "alloc"

    let cls = getClass(className)
    let allocSel = registerName(ALLOC_SEL)

    CFunc<(NativeObjCId, NativeObjCSel) -> NativeObjCId>(ObjCRuntime.msgSend)(cls, allocSel)
}

/**
 * A wrapper for a method or a prop's accessor body (action) if it needs all of the following:
 * 1. To be executed within an @autoreleasepool.
 * 2. To call ObjCRuntime.msgSendSuper with NativeObjCSuperPtr.
 * 3. To return an object.
 * Note: the result of this call supposed to be sent to a constructor of a desired return result.
 */
public unsafe func withMethodEnvObj(id: NativeObjCId, action: (NativeObjCSuperPtr) -> NativeObjCId): NativeObjCId {
      withObjCSuper(id) { objCSuper: NativeObjCSuperPtr => withAutoreleasePoolObj( {=> action(objCSuper)} ) }
}

@Frozen
/**
 * A wrapper for a method or a prop's accessor body (action) if it needs all of the following:
 * 1. To be executed within an @autoreleasepool.
 * 2. To call ObjCRuntime.msgSendSuper with NativeObjCSuperPtr.
 * 3. To return an Objective-C compatible primitive type value.
 */
public unsafe func withMethodEnv<R>(id: NativeObjCId, action: (NativeObjCSuperPtr) -> R): R where R <: CType {
    withObjCSuper(id) { objCSuper: NativeObjCSuperPtr => withAutoreleasePool( {=> action(objCSuper)} ) }
}

@Frozen
/**
 * A wrapper for a method or a prop's accessor body (action) if it needs to call ObjCRuntime.msgSendSuper with NativeObjCSuperPtr.
 */
public unsafe func withObjCSuper<R>(id: NativeObjCId, action: (NativeObjCSuperPtr) -> R): R where R <: CType {
        let instanceClass = object_getClass(id)
        let superClass = class_getSuperclass(instanceClass)

        let objCSuper = LibC.malloc<NativeObjCSuper>()
        objCSuper.write(NativeObjCSuper(id, superClass))
        try {
            action(objCSuper)
        } finally {
            LibC.free(objCSuper)
        }
}

@Frozen
/*
 * Performs an `action` within an @autoreleasepool.
 * Note: This function is intended to be used with action returns primitive types. For instancetype(ObjCId) use `autoreleasePoolObj` (or autorelease manually).
 */
public unsafe func withAutoreleasePool<R>(action: () -> R): R where R <: CType {
    let pool = objc_autoreleasePoolPush()
    try {
        action()
    } finally {
        objc_autoreleasePoolPop(pool)
    }
}

/*
 * Performs an `action` within an @autoreleasepool.
 * The overall effect of this function for return value is retain + autorelease.
 */
public unsafe func withAutoreleasePoolObj(action: () -> NativeObjCId): NativeObjCId {
    withAutoreleasePool<NativeObjCId>( {=> objc_retainAutoreleasedReturnValue(action())} )
}

/**
 * Registers a selector in the Objective-C runtime by its name.
 * Effectively a wrapper arround sel_registerName(sel), but with Cangjie String as a parameter for convenience.
 */
public unsafe func registerName(selName: String): NativeObjCSel {
    let selCName = LibC.mallocCString(selName)

    let sel = sel_registerName(selCName)
    LibC.free(selCName)

    sel
}

/**
 * Gets a value of an object's instance variable.
 * Effectively a wrapper arround object_getIvar(id, ivar), but with Cangjie String as a parameter for convenience.
 *
 * Note: this method MUST BE used only with instance variables of object types.
 */
public unsafe func getInstanceVariableObj(id: NativeObjCId, name: String): NativeObjCId {
    let ivar = getIvar(id, name)
    object_getIvar(id, ivar)
}

/**
 * Sets a value for an object's instance variable.
 * Effectively a wrapper arround object_setIvar(id, ivar, value), but with Cangjie String as a parameter for convenience.
 *
 * Note: this method MUST BE used only with instance variables of object types.
 */
public unsafe func setInstanceVariableObj(id: NativeObjCId, name: String, value: NativeObjCId): Unit {
    let ivar = getIvar(id, name)
    object_setIvar(id, ivar, value)
}

@Frozen
/**
 * Gets a value of an object's instance variable.
 *
 * Note: this method MUST BE used only with instance variables of Objective-C compatible primitive types.
 */
public unsafe func getInstanceVariable<R>(id: NativeObjCId, name: String): R where R <: CType {
    let fieldPtr = getFieldPtr<R>(id, name)
    fieldPtr.read()
}

@Frozen
/**
 * Sets a value for an object's instance variable.
 *
 * Note: this method MUST BE used only with instance variables of Objective-C compatible primitive types.
 */
public unsafe func setInstanceVariable<T>(id: NativeObjCId, name: String, value: T): Unit where T <: CType {
    let fieldPtr = getFieldPtr<T>(id, name)
    fieldPtr.write(value)
}

/**
 * Gets a class NativeObjCId by it's name.
 * Effectively a wrapper arround objc_getClass(classname), but with Cangjie String as a parameter for convenience.
 *
 * Note: NativeObjCId (not NativeObjCClass) is used as a return type to meet an Objective-C runtime API.
 */
public unsafe func getClass(className: String): NativeObjCId {
    let classCName = LibC.mallocCString(className)
    let cls = objc_getClass(classCName)
    LibC.free(classCName)

    cls
}

@Frozen
private unsafe func getFieldPtr<R>(id: NativeObjCId, name: String): CPointer<R> where R <: CType {
    let ivar = getIvar(id, name)
    let offset = ivar_getOffset(ivar)

    let idBytes = CPointer<Byte>(id) // cjlint-ignore !FFI.C.7

    CPointer<R>(idBytes + Int64(offset)) // cjlint-ignore !FFI.C.7
}

private unsafe func getIvar(id: NativeObjCId, fieldName: String): NativeObjCIvar {
    let cls = object_getClass(id)

    let fieldCName = LibC.mallocCString(fieldName)
    let ivar = class_getInstanceVariable(cls, fieldCName)
    LibC.free(fieldCName)

    ivar
}

/**
 * Exports interface of an Objective-C runtime.
 * Note: please, DO NOT provide overloads in the API, as they cannot be used in cjc-frontend.
 */
public struct ObjCRuntime {
    /**
     * Should be cast to proper CFunc<(NativeObjCId, NativeObjCSel, ...ArgTypes) -> RetType>(...) type before use.
     */
    public static let msgSend: CPointer<Unit> = CPointer<Unit>(objc_msgSend) // cjlint-ignore !G.NAM.05

    @When[os == "macOS" || os == "iOS"]
    /**
     * Should be cast to proper CFunc<(NativeObjCSuperPtr, NativeObjCSel, ...ArgTypes) -> RetType>(...) type before use.
     * Note: Doesn't work with libobjc2, as it doesn't have the objc_msgSendSuper implementation.
     */
    public static let msgSendSuper: CPointer<Unit> = CPointer<Unit>(objc_msgSendSuper) // cjlint-ignore !G.NAM.05
    public static let release: CFunc<(NativeObjCId) -> Unit> = objc_release // cjlint-ignore !G.NAM.05
    public static let retain: CFunc<(NativeObjCId) -> NativeObjCId> = objc_retain // cjlint-ignore !G.NAM.05
    public static let autorelease: CFunc<(NativeObjCId) -> NativeObjCId> = objc_autorelease // cjlint-ignore !G.NAM.05
    public static let autoreleaseReturnValue: CFunc<(NativeObjCId) -> NativeObjCId> = objc_autorelease // cjlint-ignore !G.NAM.05
    public static let retainAutorelease: CFunc<(NativeObjCId) -> NativeObjCId> = objc_retainAutorelease // cjlint-ignore !G.NAM.05
    public static let retainAutoreleaseReturnValue: CFunc<(NativeObjCId) -> NativeObjCId> = objc_retainAutoreleaseReturnValue // cjlint-ignore !G.NAM.05
    public static let retainAutoreleasedReturnValue: CFunc<(NativeObjCId) -> NativeObjCId> = objc_retainAutoreleasedReturnValue // cjlint-ignore !G.NAM.05
}

/**
 * id: NativeObjCId, sel: NativeObjCSel, ...
 */
foreign func objc_msgSend(): Unit

/**
 * super: NativeObjCSuperPtr, sel: NativeObjCSel, ...
 */
foreign func objc_msgSendSuper(): Unit

@FastNative
/**
 * Returns the class with the specified name, if one has been registered with
 * the runtime, or nil if one does not exist.  If no class of this name is
 * loaded, it calls the _objc_lookup_class() callback to allow an external
 * library to load the module providing this class.
 */
foreign func objc_getClass(name: CString): NativeObjCId

@FastNative
/**
 * Releases an object. Equivalent to [obj release].
 */
foreign func objc_release(obj: NativeObjCId): Unit

@FastNative
/**
 * Retains the argument. Equivalent to [obj retain].
 */
foreign func objc_retain(obj: NativeObjCId): NativeObjCId

@FastNative
/**
 * Autoreleases the argument. Equivalent to [obj autorelease].
 */
foreign func objc_autorelease(obj: NativeObjCId): NativeObjCId

@FastNative
/**
 * Autoreleases a return value.  This is equivalent to [obj autorelease], but
 * may also store the object somewhere where it can be quickly removed without
 * the need for any message sending.
 */
foreign func objc_autoreleaseReturnValue(obj: NativeObjCId): NativeObjCId

@FastNative
/**
 * Retains and autoreleases an object. Equivalent to [[obj retain] autorelease].
 */
foreign func objc_retainAutorelease(obj: NativeObjCId): NativeObjCId

@FastNative
/**
 * Retains and releases a return value. Equivalent to
 * objc_retain(objc_autoreleaseReturnValue(obj)).
 */
foreign func objc_retainAutoreleaseReturnValue(obj: NativeObjCId): NativeObjCId

@FastNative
/**
 * Retains a return value that has previously been autoreleased and returned.
 * This is equivalent to objc_retainAutoreleaseReturnValue(), but may support a
 * fast path, skipping the autorelease pool entirely.
 */
foreign func objc_retainAutoreleasedReturnValue(obj: NativeObjCId): NativeObjCId

@FastNative
/**
 * Allocates an autorelease pool and pushes it onto the top of the autorelease
 * pool stack.  Note that the returned autorelease pool is not required to be
 * an object.
 * Note: CPointer<Unit> as return type is used accordingly to the Objective-C ARC API.
 */
foreign func objc_autoreleasePoolPush(): CPointer<Unit>

@FastNative
/**
 * Pops the specified autorelease pool from the stack, sending release messages
 * to every object that has been autreleased since the pool was created.
 * Note: CPointer<Unit> as an arg type is used accordingly to the Objective-C ARC API.
 */
foreign func objc_autoreleasePoolPop(pool: CPointer<Unit>): Unit

@FastNative
/**
 * Returns the name of the class.  This string is owned by the runtime and is
 * valid for (at least) as long as the class remains loaded.
 */
foreign func class_getName(cls: NativeObjCClass): CString

@FastNative
/**
 * Returns the superclass of the specified class.
 */
foreign func class_getSuperclass(cls: NativeObjCClass): NativeObjCClass

@FastNative
/**
 * Look up the named instance variable in the class (and its superclasses)
 * returning a pointer to the instance variable definition or a null
 * pointer if no instance variable of that name was found.
 */
foreign func class_getInstanceVariable(cls: NativeObjCClass, name: CString): NativeObjCIvar

@FastNative
/**
 * Returns the class of the object.  Note: the isa pointer should not be
 * accessed directly with the GNUstep runtime.
 */
foreign func object_getClass(obj: NativeObjCId): NativeObjCClass

@FastNative
/**
 * Returns the value of the named instance variable.  This should not be used
 * with instance variables that are not pointers.
 */
foreign func object_getIvar(obj: NativeObjCId, ivar: NativeObjCIvar): NativeObjCId

@FastNative
/**
 * Sets the object value of a specified instance variable.
 */
foreign func object_setIvar(obj: NativeObjCId, ivar: NativeObjCIvar, value: NativeObjCId): Unit

@FastNative
/**
 * Returns the offset of an instance variable.  This value can be added to the
 * object pointer to get the address of the instance variable.
 * Note: origin return type is `ptrdiff_t`
 */
foreign func ivar_getOffset(ivar: NativeObjCIvar): IntNative

@FastNative
/**
 * Registers an untyped selector with the runtime.
 */
foreign func sel_registerName(selName: CString): NativeObjCSel
