// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjworld.cjworld

import interoplib.interop.*

private func Java_CFFI_callNonVirtualMethod_raw(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, className: String, methodName: String, methodSignature: String, args: Array<Java_CFFI_JavaEntity>): Java_CFFI_JavaEntity {
    let clazz = Java_CFFI_Class(env, className)
    let signature = Java_CFFI_parseMethodSignature(methodSignature)
    let id = Java_CFFI_MethodID.constr(env, clazz, methodName, signature)
    let callNest = Java_CFFI_JavaCallNest(args.size)

    Java_CFFI_callMethod(env, obj, id, args, callNest)
}

private func Java_CFFI_callVirtualMethod_raw(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, className: String, methodName: String, methodSignature: String, args: Array<Java_CFFI_JavaEntity>): Java_CFFI_JavaEntity {
    let clazz = Java_CFFI_Class(env, className)
    let signature = Java_CFFI_parseMethodSignature(methodSignature)
    let id = Java_CFFI_MethodID.constr(env, clazz, methodName, signature)
    let callNest = Java_CFFI_JavaCallNest(args.size)

    Java_CFFI_callVirtualMethod(env, obj, id, args, callNest)
}

private func Java_CFFI_callStaticMethod_raw(env: JNIEnv_ptr, className: String, methodName: String, methodSignature: String, args: Array<Java_CFFI_JavaEntity>): Java_CFFI_JavaEntity {
    let clazz = Java_CFFI_Class(env, className)
    let signature = Java_CFFI_parseMethodSignature(methodSignature)
    let id = Java_CFFI_MethodID.constrStatic(env, clazz, methodName, signature)
    let callNest = Java_CFFI_JavaCallNest(args.size)

    Java_CFFI_callStaticMethod(env, clazz, id, args, callNest)
}

private func Java_CFFI_getField_raw(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, className: String, fieldName: String, fieldSignature: String): Java_CFFI_JavaEntity {
    let clazz = Java_CFFI_Class(env, className)
    let signature = Java_CFFI_parseComponentSignature(fieldSignature)
    let id = Java_CFFI_FieldID.constr(env, clazz, fieldName, signature)

    Java_CFFI_getField(env, obj, id)
}

private func Java_CFFI_setField_raw(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, className: String, fieldName: String, fieldSignature: String, value: Java_CFFI_JavaEntity): Unit {
    let clazz = Java_CFFI_Class(env, className)
    let signature = Java_CFFI_parseComponentSignature(fieldSignature)
    let id = Java_CFFI_FieldID.constr(env, clazz, fieldName, signature)

    Java_CFFI_setField(env, obj, id, value)
}

@C
public func Java_A_initCJObject(env: JNIEnv_ptr, obj: jobject, sort: jboolean, shouldLog: jboolean): jlong {
    withExceptionHandling(env) {
        let a = A(env, Java_CFFI_newGlobalReference(env, Java_CFFI_JavaEntity(JOBJECT, obj), true), sort, shouldLog)
        Java_CFFI_put_to_registry_1(a)
    }
}

@C
public func Java_A_deleteCJObject(env: JNIEnv_ptr, _: jobject, self: jlong): Unit {
    withExceptionHandling(env) {
        Java_CFFI_deleteCJObject<A>(env, self) { v => v.obj }
    }
}

@C
public func Java_A_gooImpl(env: JNIEnv_ptr, _: jclass, self: jlong): Unit {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<A>(env, self)
        v.goo(env)
    }
}

extend Java_CFFI_JavaEntity {
    func asBool() {
        unsafe { asJBoolean() } != 0
    }
}

public open class A {
    let obj: Java_CFFI_JavaEntity
    let sort: Bool

    init(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, sort: jboolean, shouldLog: jboolean) {
        this.obj = obj
        this.sort = sort != 0
        Java_CFFI_setField_raw(env, obj, "M", "shouldLog", "Z", Java_CFFI_JavaEntity(JBOOLEAN, shouldLog))
    }

    private func logSort(env: JNIEnv_ptr, logBuffer: StringBuilder, message: String) {
        if (Java_CFFI_getField_raw(env, obj, "M", "shouldLog", "Z").asBool()) {
            logBuffer.append(message)
        }
    }

    //TODO consider using RefAPI due to allocations on every call
    private func mergeSort(env: JNIEnv_ptr, logBuffer: StringBuilder, indent: Int64, array: Java_CFFI_JavaEntity, n: Int64): Unit {
        for (_ in 0..indent) {
            logSort(env, logBuffer, "  ")
        }
        logSort(env, logBuffer, "Sort: array ")
        logSort(env, logBuffer, Java_CFFI_JavaStringToCangjie(env, Java_CFFI_callStaticMethod_raw(env, "M", "collectArray", "([I)Ljava/lang/String;", [array])))
        logSort(env, logBuffer, "\n")
        if (n < 2) {
            return
        }

        let m = n / 2
        let l = Java_CFFI_newJavaArray(env, "I", Int32(m))
        let r = Java_CFFI_newJavaArray(env, "I", Int32(n - m))

        var i = 0
        while (i < m) {
            Java_CFFI_callNonVirtualMethod_raw(env, obj, "M", "transferElement", "([II[II)V", [l, Java_CFFI_JavaEntity(JINT, i), array, Java_CFFI_JavaEntity(JINT, i)])
            i++
        }

        var j = 0
        while (i < n) {
            Java_CFFI_arraySet(env, JINT, r, Int32(j), Java_CFFI_arrayGet(env, JINT, array, Int32(i)))
            i++
            j++
        }

        mergeSort(env, logBuffer, indent + 1, l, m)
        mergeSort(env, logBuffer, indent + 1, r, j)

        for (_ in 0..indent) {
            logSort(env, logBuffer, "  ")
        }
        logSort(env, logBuffer, "Merge: ")
        logSort(env, logBuffer, Java_CFFI_JavaStringToCangjie(env, Java_CFFI_callStaticMethod_raw(env, "M", "collectArray", "([I)Ljava/lang/String;", [l])))
        logSort(env, logBuffer, " and ")
        logSort(env, logBuffer, Java_CFFI_JavaStringToCangjie(env, Java_CFFI_callStaticMethod_raw(env, "M", "collectArray", "([I)Ljava/lang/String;", [r])))
        i = 0
        j = 0
        var k = 0

        while (i < m && j < n - m) {
            let l_i = Java_CFFI_arrayGet(env, JINT, l, Int32(i))
            let r_j = Java_CFFI_arrayGet(env, JINT, r, Int32(j))

            if (Java_CFFI_callStaticMethod_raw(env, "M", "isInOrder", "(II)Z", [l_i, r_j]).asBool()) {
                Java_CFFI_callNonVirtualMethod_raw(env, obj, "M", "transferElement", "([II[II)V", [array, Java_CFFI_JavaEntity(JINT, k), l, Java_CFFI_JavaEntity(JINT, i)])
                i++
                k++
            } else {
                Java_CFFI_callNonVirtualMethod_raw(env, obj, "M", "transferElement", "([II[II)V", [array, Java_CFFI_JavaEntity(JINT, k), r, Java_CFFI_JavaEntity(JINT, j)])
                j++
                k++
            }
        }
        while (i < m) {
            Java_CFFI_arraySet(env, JINT, array, Int32(k), Java_CFFI_arrayGet(env, JINT, l, Int32(i)))
            i++
            k++
        }
        while (j < n - m) {
            Java_CFFI_arraySet(env, JINT, array, Int32(k), Java_CFFI_arrayGet(env, JINT, r, Int32(j)))
            j++
            k++
        }
        logSort(env, logBuffer, ": ")
        logSort(env, logBuffer, Java_CFFI_JavaStringToCangjie(env, Java_CFFI_callStaticMethod_raw(env, "M", "collectArray", "([I)Ljava/lang/String;", [array])))
        logSort(env, logBuffer, "\n")
    }

    private func prepareArrayAndLog(env: JNIEnv_ptr): String {
        let buffer = StringBuilder()

        if (sort) {
            let array = Java_CFFI_getField_raw(env, obj, "M", "array", "[I")
            if (Java_CFFI_callStaticMethod_raw(env, "M", "isSorted", "([I)Z", [array]).asBool()) {
                buffer.append("Array in this object is sorted, so untouched\n")
            } else {
                buffer.append("Array in this object is unsorted, so performing sort\n")
                mergeSort(env, buffer, 1, array, Int64(Java_CFFI_arrayGetLength(env, array)))
                if (!Java_CFFI_callStaticMethod_raw(env, "M", "isSorted", "([I)Z", [array]).asBool()) {
                    throw Exception("Sorting failed")
                }
            }
        } else {
            buffer.append("Sorting for this object is disabled, so array untouched\n")
        }

        buffer.toString()
    }

    public func goo(env: JNIEnv_ptr): Unit {
        let buffer = StringBuilder()
        buffer.append("Hello from Cangjie A goo(). This object contains array ")
        if (Java_CFFI_getField_raw(env, obj, "M", "shouldLog", "Z").asBool()) {
            let array = Java_CFFI_getField_raw(env, obj, "M", "array", "[I")
            buffer.append(Java_CFFI_JavaStringToCangjie(env, Java_CFFI_callStaticMethod_raw(env, "M", "collectArray", "([I)Ljava/lang/String;", [array])))
        } else {
            buffer.append("{ LOGGING DISABLED }")
        }
        buffer.append(".\nThen we expect optional array sort and Java M goo() call\n")

        buffer.append(prepareArrayAndLog(env))

        print(buffer.toString())
        Java_CFFI_callNonVirtualMethod_raw(env, obj, "M", "goo", "()V")
    }
}
