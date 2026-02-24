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
protected func Java_A_gooImpl(env: JNIEnv_ptr, clazz: jclass, self: jlong): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.goo(env)
    }
}

@C
protected func Java_A_gooCJThrowGetImpl(env: JNIEnv_ptr, clazz: jclass, self: jlong): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.gooCJThrowGet(env)
    }
}

@C
protected func Java_A_gooCJThrowPutImpl(env: JNIEnv_ptr, clazz: jclass, self: jlong): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.gooCJThrowPut(env)
    }
}

public open class A {
    let obj: Java_CFFI_JavaEntity

    init(obj: Java_CFFI_JavaEntity) {
        this.obj = obj
    }

    protected func goo(env: JNIEnv_ptr): Unit {
        println("Hello from A goo(). Then we expect gooExceptionStatic thrown and caught exception")
        try {
            let clazz = Java_CFFI_Class(env, "Utils")
            let signature = Java_CFFI_parseMethodSignature("()V")
            let id = Java_CFFI_MethodID.constrStatic(env, clazz, "gooExceptionStatic", signature)
            let callNest = Java_CFFI_JavaCallNest(0)
            Java_CFFI_callStaticMethod(env, clazz, id, [], callNest)
        } catch (ex: Exception) {
            println(ex.toString())
        }

        println("Hello from A goo(). Then we expect gooExceptionStatic thrown and uncaught exception")
        let clazz = Java_CFFI_Class(env, "Utils")
        let signature = Java_CFFI_parseMethodSignature("()V")
        let id = Java_CFFI_MethodID.constrStatic(env, clazz, "gooExceptionStatic", signature)
        let callNest = Java_CFFI_JavaCallNest(0)
        Java_CFFI_callStaticMethod(env, clazz, id, [], callNest)
    }

    protected func gooCJThrowPut(env: JNIEnv_ptr): Unit {
        if (true) {
            throw Exception("CJ Exception thrown from gooCJThrowPut")
        }
        let clazz = Java_CFFI_Class(env, "Utils")
        let signature = Java_CFFI_parseMethodSignature("()V")
        let id = Java_CFFI_MethodID.constrStatic(env, clazz, "gooCJThrowPut", signature)
        let callNest = Java_CFFI_JavaCallNest(0)
        Java_CFFI_callStaticMethod(env, clazz, id, [], callNest)
    }

    protected func gooCJThrowGet(env: JNIEnv_ptr): Unit {
        println("Hello from A gooCJThrowGet(). Then we expect gooCJThrowPut thrown CJ exception")
        gooCJThrowPut(env)
    }
}