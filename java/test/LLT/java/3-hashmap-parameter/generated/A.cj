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
protected func Java_A_gooImpl(env: JNIEnv_ptr, _: jclass, self: jlong, arg: jobject): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.goo(env, arg)
    }
}

@C
protected func Java_A_booImpl(env: JNIEnv_ptr, _: jclass, self: jlong, size: jfloat): jdouble {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.boo(env, size)
    }
}

public open class A {
    let obj: Java_CFFI_JavaEntity

    init(obj: Java_CFFI_JavaEntity) {
        this.obj = obj
    }

    protected init(env: JNIEnv_ptr, arg: Int32) {
        let clazz = Java_CFFI_Class(env, "A")
        let signature = Java_CFFI_parseMethodSignature("(IZ)V")
        let id = Java_CFFI_MethodID.constr(env, clazz, "<init>", signature)
        let a = Java_CFFI_newJavaObject(env, clazz, id, [Java_CFFI_JavaEntity(JINT, arg), Java_CFFI_JavaEntity(JBOOLEAN, 0)], Java_CFFI_JavaCallNest(2))
        this.obj = Java_CFFI_newGlobalReference(env, a, true) 
    }

    protected func goo(env: JNIEnv_ptr, arg: jobject): Unit {
        println("Hello from overridden A goo(HashMap)")
        let map = Java_CFFI_JavaEntity(JOBJECT, arg)
        
        let integerClazz = Java_CFFI_Class(env, "java/lang/Integer")

        let valueOfSignature = Java_CFFI_parseMethodSignature("(I)Ljava/lang/Integer;")
        let valueOfId = Java_CFFI_MethodID.constrStatic(env, integerClazz, "valueOf", valueOfSignature)
        let intObj = Java_CFFI_callStaticMethod(env, integerClazz, valueOfId, [Java_CFFI_JavaEntity(JINT, 1)], Java_CFFI_JavaCallNest(1))
        let strObj = Java_CFFI_CangjieStringToJava(env, "Value for key 1 from Java")

        let hashMapClazz = Java_CFFI_Class(env, "java/util/HashMap")
        let putSignature = Java_CFFI_parseMethodSignature("(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")
        let putId = Java_CFFI_MethodID.constr(env, hashMapClazz, "put", putSignature)
        Java_CFFI_callVirtualMethod(env, map, putId, [intObj, strObj], Java_CFFI_JavaCallNest(2))

        let mClazz = Java_CFFI_Class(env, "M")
        let gooSignature = Java_CFFI_parseMethodSignature("(Ljava/util/HashMap;)V")
        let gooId = Java_CFFI_MethodID.constr(env, mClazz, "goo", gooSignature)
        Java_CFFI_callMethod(env, obj, gooId, [map], Java_CFFI_JavaCallNest(1))

        let a = A(env, 5)
        Java_CFFI_putToRegistry(env, a.obj, a)

        let intSize = unsafe {
            let sizeSignature = Java_CFFI_parseMethodSignature("()I")
            let sizeId = Java_CFFI_MethodID.constr(env, hashMapClazz, "size", sizeSignature)
            Java_CFFI_callVirtualMethod(env, map, sizeId, [], Java_CFFI_JavaCallNest(0)).asJInt()
        }
        let floatSize = Float32(intSize)
        let doubleSize = a.boo(env, floatSize)
        println("A.boo -> ${doubleSize}");
    }

    protected func boo(env: JNIEnv_ptr, size: jfloat): jdouble {
        println("Hello from A.boo(${size})")

        return unsafe {
            let clazz = Java_CFFI_Class(env, "M")
            let signature = Java_CFFI_parseMethodSignature("(F)D")
            let id = Java_CFFI_MethodID.constr(env, clazz, "boo", signature)
            let callNest = Java_CFFI_JavaCallNest(1)
            Java_CFFI_callMethod(env, obj, id, [Java_CFFI_JavaEntity(JFLOAT, size)], callNest).asJDouble()
        }
    }
}
