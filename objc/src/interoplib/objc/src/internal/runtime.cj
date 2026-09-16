// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package objc.internal

import std.sync.Mutex

@Deprecated["Legacy APIs, which are no longer used by the compiler Interop desugaring"]
/**
 * To avoid std.sync.Mutex dependency at objc glue-code.
 */
protected type ForwarderMutex = Mutex

/**
 * id or Instance Method Pointer type.
 */
protected type NativeObjCId = CPointer<Unit>

/**
 * Internal marker used as argument for generated constructors in mirror and impl ObjC interop classes.
 * It is meant to remove possible clashes of internal constructors with user defined ones.
 * Do not use it! Usage in user-defined constructors can lead to various ambiguous compilation errors.
 */
protected struct NativeObjCIdMarker {
    internal NativeObjCIdMarker() {}
}
protected let __NATIVE_OBJC_ID_MARKER = NativeObjCIdMarker()

/**
 * SEL.
 */
protected type NativeObjCSel = CPointer<Unit>

/**
 * Class.
 */
protected type NativeObjCClass = CPointer<Unit>

/**
 * Protocol.
 */
protected type NativeObjCProtocol = CPointer<Unit>

/**
 * Ivar.
 */
protected type NativeObjCIvar = CPointer<Unit>

/**
 * Instance Method Pointer type.
 * Note: Should be cast to proper CFunc<(NativeObjCId, NativeObjCSel, ...ArgTypes) -> RetType>(...) type before use.
 */
protected type NativeObjCImp = CPointer<Unit>

@C
/**
 * objc_super
 */
protected struct NativeObjCSuper {
    public NativeObjCSuper(let receiver: NativeObjCId, let super_class: NativeObjCClass) {}
}

/**
 * objc_super*
 * Could be convenient for msgSendSuper calls.
 */
protected type NativeObjCSuperPtr = CPointer<NativeObjCSuper>

/**
 * An opaque identifier for Cangjie mirror objects.
 */
protected type RegistryId = Int64

/**
 * An exception that is used to mark an unreachable code.
 */
protected class ObjCUnreachableCodeException <: Exception {}

/**
 * An exception that has to be used when runtime calls static methods directly on interfaces (protocols).
 */
protected class ObjCStaticMethodCallOnInterfaceException <: Exception {}

@Deprecated["Legacy APIs, which are no longer used by the compiler Interop desugaring"]
@Frozen
/**
 * Gets a Cangjie object of type R from registry by the pointer of its Objective-C counterpart.
 */
protected unsafe func getFromRegistryByNativeHandle<R>(id: NativeObjCId): R {
    const REGISTRY_ID_IDENT = "$registryId"
    getFromRegistryById<R>(
        CFunc<(NativeObjCId, NativeObjCSel) -> RegistryId>(objCMsgSend())(id, registerName(REGISTRY_ID_IDENT))
    )
}

private const REGISTRY_ID_SEL_STR = "$registryId"
private var REGISTRY_ID_SEL = NativeObjCSel()

protected unsafe func getRegistryId(id: NativeObjCId): RegistryId {
    if (CPointer<UIntNative>(inout REGISTRY_ID_SEL).read() == 0) {
        REGISTRY_ID_SEL = registerName(REGISTRY_ID_SEL_STR)
    }

    CFunc<(NativeObjCId, NativeObjCSel) -> RegistryId>(objCMsgSend())(id, REGISTRY_ID_SEL)
}

private const SET_REGISTRY_ID_SEL_STR = "set$registryId:"
private var SET_REGISTRY_ID_SEL = NativeObjCSel()

protected unsafe func setRegistryId(id: NativeObjCId, regId: RegistryId): Unit {
    if (CPointer<UIntNative>(inout SET_REGISTRY_ID_SEL).read() == 0) {
        SET_REGISTRY_ID_SEL = registerName(SET_REGISTRY_ID_SEL_STR)
    }

    CFunc<(NativeObjCId, NativeObjCSel, RegistryId) -> Unit>(objCMsgSend())(id, SET_REGISTRY_ID_SEL, regId)
}

@Frozen
/**
 * Gets a Cangjie object of type R from registry by the id.
 */
protected unsafe func getFromRegistryById<R>(registryId: RegistryId): R {
    REGISTRY.get<R>(registryId).getOrThrow()
}

/**
 * Puts a Cangjie object to registry.
 */
protected unsafe func putToRegistry(any: Any): RegistryId {
    REGISTRY.put(any)
}

/**
 * Removes a Cangjie object from registry by its id.
 */
protected unsafe func removeFromRegistry(registryId: RegistryId): Unit {
    REGISTRY.remove(registryId)
}

@Deprecated["Legacy APIs, which are no longer used by the compiler Interop desugaring"]
/**
 * @note: use `objCAlloc` instead.
 * Allocates a pointer to the class by its name.
 * Effectively a wrapper around `objc_alloc`, but with Cangjie String as a parameter for convenience.
 * Note: MUST be called in a transaction with corresponding init* method of the class and the pointer, returned by this function, MUST NOT be stored anywhere.
 */
protected unsafe func alloc(className: String): NativeObjCId {
    objc_alloc(getClass(className))
}

@Frozen
/**
 * Allocates a pointer to the class by Objective-C class pointer.
 * Effectively a wrapper around `objc_alloc`.
 * Note: MUST be called in a transaction with corresponding init* method of the class and the pointer, returned by this function, MUST NOT be stored anywhere.
 */
protected unsafe func objCAlloc(clazz: NativeObjCClass): NativeObjCId {
    objc_alloc(clazz)
}

/**
 * Returns super_class field from NativeObjCSuperPtr
 */
protected unsafe func getSuperClass(objCSuper: NativeObjCSuperPtr): NativeObjCClass {
    return objCSuper.read().super_class
}

@When[os == "macOS" || os == "iOS"]
@Frozen
/**
 * A wrapper for a method or a prop's accessor body (action) if it needs to call objc_msgSendSuper with NativeObjCSuperPtr.
 * objc_msgSendSuper is intended to be called like that:
 * CFunc<(CPointer<Unit>, NativeObjCSel, ...ArgTypes) -> RetType>(objCMsgSendSuper(objCSuper, sel))(receiver, sel, ...)
 * where: receiver is the first `action` param and objCSuper is the second `action` param
 */
protected unsafe func withObjCSuper<R>(id: NativeObjCId, objCClassName: String, action: (CPointer<Unit>, NativeObjCSuperPtr) -> R): R where R <: CType {
    let objCSuper = getObjCSuper(id, objCClassName)
    try {
        action(CPointer<Unit>(objCSuper), objCSuper)
    } finally {
        LibC.free(objCSuper)
    }
}

@When[os == "Linux" || os == "Windows"]
@Frozen
/**
 * A wrapper for a method or a prop's accessor body (action) if it needs to call objc_msgSendSuper with NativeObjCSuperPtr.
 * objc_msgSendSuper is intended to be called like that:
 * CFunc<(CPointer<Unit>, NativeObjCSel, ...ArgTypes) -> RetType>(objCMsgSendSuper(objCSuper, sel))(receiver, sel, ...)
 * where: receiver is the first `action` param and objCSuper is the second `action` param
 */
protected unsafe func withObjCSuper<R>(id: NativeObjCId, objCClassName: String, action: (CPointer<Unit>, NativeObjCSuperPtr) -> R): R where R <: CType {
    let objCSuper = getObjCSuper(id, objCClassName)
    try {
        // Note: for linux implementation we need to use `id` as a receiver, see [objCMsgSendSuper]
        action(id, objCSuper)
    } finally {
        LibC.free(objCSuper)
    }
}

@Frozen
/*
 * Performs an `action` within an @autoreleasepool.
 * Note: This function is intended to be used with action returns primitive types. For instancetype(ObjCId) use `autoreleasePoolObj` (or autorelease manually).
 */
protected unsafe func withAutoreleasePool<R>(action: () -> R): R where R <: CType {
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
protected unsafe func withAutoreleasePoolObj(action: () -> NativeObjCId): NativeObjCId {
    withAutoreleasePool<NativeObjCId>( {=> objc_retainAutoreleasedReturnValue(action())} )
}

@Frozen
/**
 * Registers a selector in the Objective-C runtime by its name.
 * Effectively a wrapper around sel_registerName(sel), but with Cangjie String as a parameter for convenience.
 */
protected unsafe func registerName(selName: String): NativeObjCSel {
    let selCName = LibC.mallocCString(selName)

    let sel = sel_registerName(selCName)
    LibC.free(selCName)

    sel
}

/**
 * Gets a value of an object's instance variable.
 * Effectively a wrapper around object_getIvar(id, ivar), but with Cangjie String as a parameter for convenience.
 *
 * Note: this method MUST BE used only with instance variables of object types.
 */
protected unsafe func getInstanceVariableObj(id: NativeObjCId, name: String): NativeObjCId {
    let ivar = getIvar(id, name)
    object_getIvar(id, ivar)
}

/**
 * Sets a value for an object's instance variable.
 * Effectively a wrapper around object_setIvar(id, ivar, value), but with Cangjie String as a parameter for convenience.
 *
 * Note: this method MUST BE used only with instance variables of object types.
 */
protected unsafe func setInstanceVariableObj(id: NativeObjCId, name: String, value: NativeObjCId): Unit {
    let ivar = getIvar(id, name)
    object_setIvar(id, ivar, value)
}

@Frozen
/**
 * Gets a value of an object's instance variable.
 *
 * Note: this method MUST BE used only with instance variables of Objective-C compatible primitive types.
 */
protected unsafe func getInstanceVariable<R>(id: NativeObjCId, name: String): R where R <: CType {
    let fieldPtr = getFieldPtr<R>(id, name)
    fieldPtr.read()
}

@Frozen
/**
 * Sets a value for an object's instance variable.
 *
 * Note: this method MUST BE used only with instance variables of Objective-C compatible primitive types.
 */
protected unsafe func setInstanceVariable<T>(id: NativeObjCId, name: String, value: T): Unit where T <: CType {
    let fieldPtr = getFieldPtr<T>(id, name)
    fieldPtr.write(value)
}

@Frozen
/**
 * Gets a class NativeObjCId by it's name.
 * Effectively a wrapper around objc_getClass(classname), but with Cangjie String as a parameter for convenience.
 *
 * Note: NativeObjCId (not NativeObjCClass) is used as a return type to meet an Objective-C runtime API.
 */
protected unsafe func getClass(className: String): NativeObjCId {
    let classCName = LibC.mallocCString(className)
    let cls = objc_getClass(classCName)
    LibC.free(classCName)

    cls
}

/**
 * Gets a protocol NativeObjCProtocol by it's name.
 * Effectively a wrapper around objc_getProtocol(protoname), but with Cangjie String as a parameter for convenience.
 */
protected unsafe func getProtocol(protoName: String): NativeObjCProtocol {
    let protoCName = LibC.mallocCString(protoName)
    let proto = objc_getProtocol(protoCName)
    LibC.free(protoCName)

    proto
}

@Frozen
/**
 * Returns a pointer to objc_msgSend implementation.
 * The result should be cast to proper CFunc<(NativeObjCId, NativeObjCSel, ...ArgTypes) -> RetType>(...) type before use.
 */
protected func objCMsgSend(): CPointer<Unit> {
    return CPointer<Unit>(objc_msgSend)
}

@When[os == "macOS" || os == "iOS"]
@Frozen
/**
 * Returns a pointer to objc_msgSendSuper implementation.
 * The result should be cast to proper CFunc<(NativeObjCSuperPtr, NativeObjCSel, ...ArgTypes) -> RetType>(...) type before use.
 */
protected func objCMsgSendSuper(_: NativeObjCSuperPtr, _: NativeObjCSel): CPointer<Unit> {
    CPointer<Unit>(objc_msgSendSuper)
}

@When[os == "Linux" || os == "Windows"]
/**
 * Returns a pointer to objc_msgSendSuper implementation.
 * The result should be cast to proper CFunc<(NativeObjCId, NativeObjCSel, ...ArgTypes) -> RetType>(...) type before use.
 * Note: Result MUST be called with a pointer to the message receiver, not with a pointer to NativeObjCSuper struct.
 */
protected func objCMsgSendSuper(objCSuper: NativeObjCSuperPtr, sel: NativeObjCSel): CPointer<Unit> {
    unsafe {
        objc_msg_lookup_super(objCSuper, sel)
    }
}

private const NSSTRING_CLASS_IDENT = "NSString"
private var NSSTRING_CLASS = NativeObjCClass()
private const INIT_WITH_UTF8_STRING_SEL_STR = "initWithUTF8String:"
private var INIT_WITH_UTF8_STRING_SEL = NativeObjCSel()

/**
 * Converts Cangjie String to NSString.
 */
protected unsafe func convertToNSString(str: String): NativeObjCId {
    if (CPointer<UIntNative>(inout NSSTRING_CLASS).read() == 0) {
        NSSTRING_CLASS = getClass(NSSTRING_CLASS_IDENT)
    }
    if (CPointer<UIntNative>(inout INIT_WITH_UTF8_STRING_SEL).read() == 0) {
        INIT_WITH_UTF8_STRING_SEL = registerName(INIT_WITH_UTF8_STRING_SEL_STR)
    }

    let cstr = LibC.mallocCString(str)
    let ptr = CPointer<Unit>(cstr.getChars())
    let result = CFunc<(NativeObjCId, NativeObjCSel, CPointer<Unit>) -> NativeObjCId>(objCMsgSend())(
        objCAlloc(NSSTRING_CLASS),
        INIT_WITH_UTF8_STRING_SEL,
        ptr
    )
    LibC.free(cstr)
    return result
}

private const DESCRIPTION_SEL_STR = "description"
private var DESCRIPTION_SEL = NativeObjCSel()
private const UTF8STRING_SEL_STR = "UTF8String"
private var UTF8STRING_SEL = NativeObjCSel()

/**
 * Gets object description and converts it to Cangjie String.
 */
protected unsafe func descriptionAsString(id: NativeObjCId): String {
    if (CPointer<UIntNative>(inout DESCRIPTION_SEL).read() == 0) {
        DESCRIPTION_SEL = registerName(DESCRIPTION_SEL_STR)
    }
    if (CPointer<UIntNative>(inout UTF8STRING_SEL).read() == 0) {
        UTF8STRING_SEL = registerName(UTF8STRING_SEL_STR)
    }

    let description = CFunc<(NativeObjCId, NativeObjCSel) -> NativeObjCId>(objCMsgSend())(id, DESCRIPTION_SEL)
    let result = CFunc<(NativeObjCId, NativeObjCSel) -> CPointer<Unit>>(objCMsgSend())(
        description,
        UTF8STRING_SEL
    )
    let str = CString(CPointer<UInt8>(result))
    return str.toString()
}

@Frozen
/**
 * Autoreleases the argument. Equivalent to [id autorelease].
 */
protected unsafe func objCAutorelease(id: NativeObjCId): NativeObjCId {
    objc_autorelease(id)
}

@Frozen
/**
 * Autoreleases the argument by objc_autoreleaseReturnValue call.
 */
protected unsafe func objCAutoreleaseReturnValue(id: NativeObjCId): NativeObjCId {
    objc_autoreleaseReturnValue(id)
}

@Frozen
/**
 * Decreases the reference counter by one.
 */
protected unsafe func objCRelease(id: NativeObjCId): Unit {
    objc_release(id)
}

@Frozen
/**
 * Increases the reference counter by one.
 */
protected unsafe func objCRetain(id: NativeObjCId): NativeObjCId {
    objc_retain(id)
}

@Frozen
/**
 * Autoreleases and retains the argument by objc_retainAutoreleaseReturnValue call.
 */
protected unsafe func objCRetainAutoreleaseReturnValue(id: NativeObjCId): NativeObjCId {
    objc_retainAutoreleaseReturnValue(id)
}

@Frozen
protected unsafe func objCRetainAutoreleasedReturnValue(id: NativeObjCId): NativeObjCId {
    objc_retainAutoreleasedReturnValue(id)
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
    var ivar = zeroValue<NativeObjCIvar>()

    let fieldCName = LibC.mallocCString(fieldName)
    ivar = class_getInstanceVariable(cls, fieldCName)
    LibC.free(fieldCName)

    if (CPointer<UIntNative>(inout ivar).read() == 0) {
        throw Exception("No instance variable with name '${fieldName}' found in class '${class_getName(cls)}'")
    }

    ivar
}

/**
 * Checks if a given object is an instance of the given class, including superclasses
 */
protected unsafe func isKindOfClass(id: NativeObjCId, clazz: NativeObjCClass): Bool {
    var realClass = object_getClass(id);
    var clazzPtr = clazz
    while (CPointer<UIntNative>(inout realClass).read() != 0) {
        // Can't do the same trick with `clazz` ptr, because it is immutable
        if (CPointer<UIntNative>(inout realClass).read() == CPointer<UIntNative>(inout clazzPtr).read()) {
            return true
        }
        realClass = class_getSuperclass(realClass)
    }
    return false
}

/**
 * Checks if a given object is an instance of the given protocol, including superclasses
 */
protected unsafe func conformsToProtocol(id: NativeObjCId, proto: NativeObjCProtocol): Bool {
    var realClass = object_getClass(id);
    while (CPointer<UIntNative>(inout realClass).read() != 0) {
        if (class_conformsToProtocol(realClass, proto)) {
            return true
        }
        realClass = class_getSuperclass(realClass)
    }
    return false
}

@Frozen
/**
 * Checks if a given class instances respond to a given selector.
 * Effectively exports Objective-C runtime `class_respondsToSelector` function.
 */
protected unsafe func respondsToSelector(cls: NativeObjCClass, sel: NativeObjCSel): Bool {
    class_respondsToSelector(cls, sel)
}

@Frozen
/**
 * Checks if a given class instances respond to a given selector.
 * Effectively exports Objective-C runtime `object_getClass` function.
 */
protected unsafe func objectGetClass(id: NativeObjCId): NativeObjCClass {
    object_getClass(id)
}

/**
 * Returns objc_super* for `id` and `objCClassName`.
 * Note: callee is responsible to free the result ptr.
 * Note: callee is responsible to ensure that `id` is an instance of `objCClassName` class on Objective-C side.
 */
private unsafe func getObjCSuper(id: NativeObjCId, objCClassName: String): NativeObjCSuperPtr {
    let instanceClass = getClass(objCClassName)
    let superClass = class_getSuperclass(instanceClass)

    let objCSuper = LibC.malloc<NativeObjCSuper>()
    objCSuper.write(NativeObjCSuper(id, superClass))

    objCSuper
}

@FastNative
/**
 * id: NativeObjCId, sel: NativeObjCSel, ...
 * NOTE: technically, msgSend is not applicable for @FastNative because it can call
 * arbitrary code. However, it is called through a pointer that need to point exactly
 * at the original function from objc runtime. Calling through pointer always produces
 * non-fastnative call, so the contract is not violated.
 */
foreign func objc_msgSend(): Unit

@When[os == "macOS" || os == "iOS"]
/**
 * super: NativeObjCSuperPtr, sel: NativeObjCSel, ...
 */
foreign func objc_msgSendSuper(): Unit

@FastNative
/**
 * Returns the protocol with the specified name, if one has been registered with
 * the runtime, or nil if one does not exist.
 */
foreign func objc_getProtocol(name: CString): NativeObjCProtocol

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
 * Checks if a given class conforms to a given protocol
 * (does not check superclasses)
 */
foreign func class_conformsToProtocol(cls: NativeObjCClass, proto: NativeObjCProtocol): Bool

@FastNative
/**
 * Checks if a given class responds to a given selector
 * (does check superclasses)
 */
foreign func class_respondsToSelector(cls: NativeObjCClass, sel: NativeObjCSel): Bool

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

@When[os == "Linux" || os == "Windows"]
@FastNative
/**
 * The message lookup function used for messages sent to super in the GCC ABI.
 * This specifies both the class and the function (either a method or a forwarding hook) that should be called in
 * response to a given message.
 * Note: nonportable
 */
foreign func objc_msg_lookup_super(objCSuper: NativeObjCSuperPtr, sel: NativeObjCSel): NativeObjCImp

@FastNative
/**
 * Objective-C runtime function that allocates an object of a given class
 * Effectively objc_alloc(ClassObject) is the same as [ClassObject alloc]
 * but works directly in runtime without expensive method dispatching
 */
foreign func objc_alloc(clazz: NativeObjCClass): NativeObjCId
