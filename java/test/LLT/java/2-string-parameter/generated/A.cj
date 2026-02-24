// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.interop.tests

import interoplib.interop.*

@C
protected func Java_A_initCJObject(env: JNIEnv_ptr, obj: jobject): jlong {
    withExceptionHandling(env) {
        let a = A(Java_CFFI_newGlobalReference(env, Java_CFFI_JavaEntity(JOBJECT, obj), true))
        Java_CFFI_put_to_registry_1(a)
    }
}

@C
protected func Java_A_deleteCJObject(env: JNIEnv_ptr, clazz: jclass, self: jlong): Unit {
    withExceptionHandling(env) {
        Java_CFFI_deleteCJObject<A>(env, self) { v => v.obj }
    }
}

@C
protected func Java_A_fieldInitStatic(env: JNIEnv_ptr, clazz: jclass, value: jint): jint {
    A.fieldInitStatic(value)
}

@C
protected func Java_A_gooImpl(env: JNIEnv_ptr, _: jclass, self: jlong, arg: jstring): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.goo(env, arg)
    }
}

@C
protected func Java_A_gooMultipleArgsImpl(env: JNIEnv_ptr, _: jclass, self: jlong, arg1: jstring, arg2: jint): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.gooMultipleArgs(env, arg1, arg2)
    }
}

public open class A {
    private var g: Int32 = 0

    let obj: Java_CFFI_JavaEntity

    init(obj: Java_CFFI_JavaEntity) {
        this.obj = obj
    }

    public static func fieldInitStatic(f: Int32): Int32 {
        f * 2
    }

    protected func gooMultipleArgs(env: JNIEnv_ptr, arg1: jstring, arg2: Int32): Unit {
        print("Hello from overridden A gooMultipleArgs(String, int) ")
        println(Java_CFFI_JavaStringToCangjie(env, Java_CFFI_JavaEntity(JSTRING, arg1)))
        
        let superMsg = Java_CFFI_CangjieStringToJava(env, "Invokes Java super.gooMultipleArgs(String, int)")
        let clazz = Java_CFFI_Class(env, "M")
        let signature = Java_CFFI_parseMethodSignature("(Ljava/lang/String;I)V")
        let id = Java_CFFI_MethodID.constr(env, clazz, "gooMultipleArgs", signature)
        Java_CFFI_callMethod(env, obj, id, [superMsg, Java_CFFI_JavaEntity(JINT, 8)], Java_CFFI_JavaCallNest(2))
    }

    protected func goo(env: JNIEnv_ptr, arg: jstring): Unit {
        print("Hello from overridden A goo(String) ")
        println(Java_CFFI_JavaStringToCangjie(env, Java_CFFI_JavaEntity(JSTRING, arg)))
        
        let superMsg = Java_CFFI_CangjieStringToJava(env, "Invokes Java super.goo(String)")
        let clazz = Java_CFFI_Class(env, "M")
        let gooSignature = Java_CFFI_parseMethodSignature("(Ljava/lang/String;)V")
        let gooId = Java_CFFI_MethodID.constr(env, clazz, "goo", gooSignature)
        Java_CFFI_callMethod(env, obj, gooId, [superMsg], Java_CFFI_JavaCallNest(1))

        print("Get super.f field value ")
        let fSignature = Java_CFFI_parseComponentSignature("I")
        let fId = Java_CFFI_FieldID.constr(env, clazz, "f", fSignature)
        var superFCached = Java_CFFI_getField(env, obj, fId)
        println(unsafe{ superFCached.asJInt() })

        print("Set super.f field value to ")
        Java_CFFI_setField(env, obj, fId, Java_CFFI_JavaEntity(JINT, 5))
        superFCached = Java_CFFI_getField(env, obj, fId)
        println(unsafe { superFCached.asJInt() })
        
        // Java memory model allows us to not re-read super.f field here
        // while there's no writes to it since last read and this field is not volatile
        let h = unsafe { superFCached.asJInt() } + 1
        println(h)
    }
}
