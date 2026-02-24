// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.interop.tests

import interoplib.interop.*

import java.lang.*
import std.console.Console

@C
protected func Java_Main_foo(env: JNIEnv_ptr, _: jclass): jint {
    withExceptionHandling<jint>(env) {
        try {
            Java_CFFI_withLocalFrameUnit(env, 1) {
                let clazz = Java_CFFI_Class(env, "Main")
                let valueId = Java_CFFI_FieldID.constrStatic(env, clazz, "VALUE", Java_CFFI_parseComponentSignature("Ljava/lang/String;"))

                let stringOuter = Java_CFFI_withLocalFrame(env, 1) {
                    let stringInner = Java_CFFI_getStaticField(env, clazz, valueId)
                    println(Java_CFFI_JavaStringToCangjie(env, stringInner))
                    stringInner
                }

                println(Java_CFFI_JavaStringToCangjie(env, stringOuter))
                Console.stdOut.flush()

                throw Exception("test")
            }
            102
        } catch (e: Exception) {
            if (e.message == "test") { 100 } else { 101 }
        }
    }
}
