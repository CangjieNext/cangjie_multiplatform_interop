// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.interop

import std.collection.ArrayQueue
import std.sync.AtomicBool

public type jobject = CPointer<Unit>
public type jarray = jobject
public type jclass = jobject
public type jthrowable = jobject
public type jbooleanArray = jarray
public type jbyteArray = jarray
public type jcharArray = jarray
public type jshortArray = jarray
public type jintArray = jarray
public type jlongArray = jarray
public type jfloatArray = jarray
public type jdoubleArray = jarray
public type jobjectArray = jarray
public type jmethodID = CPointer<Unit>
public type JNIEnv_ptr = CPointer<CPointer<JNINativeInterface_>>
public type jlong = Int64
public type jint = Int32
public type jstring = CPointer<Unit>
public type jboolean = UInt8
public type jfieldID = CPointer<Unit>
public type jweak = jobject
public type jvalue = UInt64
public type jsize = jint
public type jshort = Int16
public type jchar = UInt16
public type jfloat = Float32
public type jdouble = Float64
public type jbyte = Int8
type JavaVM = CPointer<JNIInvokeInterface_>

public class JNIException <: Exception {
    init(message: String, jException: jthrowable) {
        super(message)
        this.jException = jException
    }

    public var jException = jthrowable()
}

public const INTEROPLIB_VERSION: Int64 = 9

var jvm = CPointer<JavaVM>()

@C
func setJavaVM(javaVM: CPointer<JavaVM>): Unit {
    jvm = javaVM
}

@C
public struct JNINativeInterface_ {
    private var reserved0 = CPointer<Unit>()
    private var reserved1 = CPointer<Unit>()
    private var reserved2 = CPointer<Unit>()
    private var reserved3 = CPointer<Unit>()

    private var GetVersion = CPointer<Unit>()
    private var DefineClass = CPointer<Unit>()

    var FindClass = CFunc<(env: JNIEnv_ptr, name: CString) -> jclass>(CPointer<Unit>())

    private var FromReflectedMethod = CPointer<Unit>()
    private var FromReflectedField = CPointer<Unit>()

    private var ToReflectedMethod = CPointer<Unit>()

    var GetSuperclass = CFunc<(env: JNIEnv_ptr, clazz: jclass) -> jclass>(CPointer<Unit>())
    private var IsAssignableFrom = CPointer<Unit>()

    private var ToReflectedField = CPointer<Unit>()

    var Throw = CFunc<(env: JNIEnv_ptr, obj: jthrowable) -> jint>(CPointer<Unit>())
    var ThrowNew = CFunc<(env: JNIEnv_ptr, clazz: jclass, msg: CString) -> jint>(CPointer<Unit>())
    var ExceptionOccurred = CFunc<(env: JNIEnv_ptr) -> jthrowable>(CPointer<Unit>())
    var ExceptionDescribe = CFunc<(env: JNIEnv_ptr) -> Unit>(CPointer<Unit>())
    var ExceptionClear = CFunc<(env: JNIEnv_ptr) -> Unit>(CPointer<Unit>())
    private var FatalError = CPointer<Unit>()

    var PushLocalFrame = CFunc<(env: JNIEnv_ptr, capacity: jint) -> jint>(CPointer<Unit>())
    var PopLocalFrame = CFunc<(env: JNIEnv_ptr, result: jobject) -> jobject>(CPointer<Unit>())

    var NewGlobalRef = CFunc<(env: JNIEnv_ptr, obj: jobject) -> jobject>(CPointer<Unit>())
    var DeleteGlobalRef = CFunc<(env: JNIEnv_ptr, obj: jobject) -> Unit>(CPointer<Unit>())
    var DeleteLocalRef = CFunc<(env: JNIEnv_ptr, obj: jobject) -> Unit>(CPointer<Unit>())
    private var IsSameObject = CPointer<Unit>()
    var NewLocalRef = CFunc<(env: JNIEnv_ptr, obj: jobject) -> jobject>(CPointer<Unit>())
    private var EnsureLocalCapacity = CPointer<Unit>()

    private var AllocObject = CPointer<Unit>()
    var NewObject = CFunc<(env: JNIEnv_ptr, clazz: jclass, methodID: jmethodID) -> jobject>(CPointer<Unit>())

    private var NewObjectV = CPointer<Unit>()
    var NewObjectA = CFunc<(env: JNIEnv_ptr, clazz: jclass, methodID: jmethodID, arg: CPointer<jvalue>) -> jobject>(CPointer<Unit>())

    var GetObjectClass = CFunc<(env: JNIEnv_ptr, obj: jobject) -> jclass>(CPointer<Unit>())
    var IsInstanceOf = CFunc<(env: JNIEnv_ptr, obj: jobject, clazz: jclass) -> jboolean>(CPointer<Unit>())

    var GetMethodID = CFunc<(env: JNIEnv_ptr, clazz: jclass, name: CString, sig: CString) -> jmethodID>(CPointer<Unit>())

    var CallObjectMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jobject>(CPointer<Unit>())
    private var CallObjectMethodV = CPointer<Unit>()
    var CallObjectMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jobject>(CPointer<Unit>())
    
    var CallBooleanMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jboolean>(CPointer<Unit>())
    private var CallBooleanMethodV = CPointer<Unit>()
    var CallBooleanMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jboolean>(CPointer<Unit>())

    var CallByteMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jbyte>(CPointer<Unit>())
    private var CallByteMethodV = CPointer<Unit>()
    var CallByteMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jbyte>(CPointer<Unit>())

    var CallCharMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jchar>(CPointer<Unit>())
    private var CallCharMethodV = CPointer<Unit>()
    var CallCharMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jchar>(CPointer<Unit>())

    var CallShortMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jshort>(CPointer<Unit>())
    private var CallShortMethodV = CPointer<Unit>()
    var CallShortMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jshort>(CPointer<Unit>())

    var CallIntMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jint>(CPointer<Unit>())
    private var CallIntMethodV = CPointer<Unit>()
    var CallIntMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jint>(CPointer<Unit>())

    var CallLongMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jlong>(CPointer<Unit>())
    private var CallLongMethodV = CPointer<Unit>()
    var CallLongMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jlong>(CPointer<Unit>())

    var CallFloatMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jfloat>(CPointer<Unit>())
    private var CallFloatMethodV = CPointer<Unit>()
    var CallFloatMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jfloat>(CPointer<Unit>())

    var CallDoubleMethod = CFunc<(JNIEnv_ptr, jobject, jmethodID) -> jdouble>(CPointer<Unit>())
    private var CallDoubleMethodV = CPointer<Unit>()
    var CallDoubleMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> jdouble>(CPointer<Unit>())

    var CallVoidMethod = CFunc<(env: JNIEnv_ptr, obj: jobject, methodID: jmethodID) -> Unit>(CPointer<Unit>())
    private var CallVoidMethodV = CPointer<Unit>()
    var CallVoidMethodA = CFunc<(JNIEnv_ptr, jobject, jmethodID, CPointer<jvalue>) -> Unit>(CPointer<Unit>())

    var CallNonvirtualObjectMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jobject>(CPointer<Unit>())
    private var CallNonvirtualObjectMethodV = CPointer<Unit>()
    var CallNonvirtualObjectMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jobject>(CPointer<Unit>())

    var CallNonvirtualBooleanMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jboolean>(CPointer<Unit>())
    private var CallNonvirtualBooleanMethodV = CPointer<Unit>()
    var CallNonvirtualBooleanMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jboolean>(CPointer<Unit>())

    var CallNonvirtualByteMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jbyte>(CPointer<Unit>())
    private var CallNonvirtualByteMethodV = CPointer<Unit>()
    var CallNonvirtualByteMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jbyte>(CPointer<Unit>())

    var CallNonvirtualCharMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jchar>(CPointer<Unit>())
    private var CallNonvirtualCharMethodV = CPointer<Unit>()
    var CallNonvirtualCharMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jchar>(CPointer<Unit>())

    var CallNonvirtualShortMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jshort>(CPointer<Unit>())
    private var CallNonvirtualShortMethodV = CPointer<Unit>()
    var CallNonvirtualShortMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jshort>(CPointer<Unit>())

    var CallNonvirtualIntMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jint>(CPointer<Unit>())
    private var CallNonvirtualIntMethodV = CPointer<Unit>()
    var CallNonvirtualIntMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jint>(CPointer<Unit>())

    var CallNonvirtualLongMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jlong>(CPointer<Unit>())
    private var CallNonvirtualLongMethodV = CPointer<Unit>()
    var CallNonvirtualLongMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jlong>(CPointer<Unit>())

    var CallNonvirtualFloatMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jfloat>(CPointer<Unit>())
    private var CallNonvirtualFloatMethodV = CPointer<Unit>()
    var CallNonvirtualFloatMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jfloat>(CPointer<Unit>())

    var CallNonvirtualDoubleMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> jdouble>(CPointer<Unit>())
    private var CallNonvirtualDoubleMethodV = CPointer<Unit>()
    var CallNonvirtualDoubleMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> jdouble>(CPointer<Unit>())

    var CallNonvirtualVoidMethod = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID) -> Unit>(CPointer<Unit>())
    private var CallNonvirtualVoidMethodV = CPointer<Unit>()
    var CallNonvirtualVoidMethodA = CFunc<(JNIEnv_ptr, jobject, jclass, jmethodID, CPointer<jvalue>) -> Unit>(CPointer<Unit>())

    var GetFieldID = CFunc<(JNIEnv_ptr, jclass, CString, CString) -> jfieldID>(CPointer<Unit>())

    var GetObjectField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jobject>(CPointer<Unit>())
    var GetBooleanField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jboolean>(CPointer<Unit>())
    var GetByteField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jbyte>(CPointer<Unit>())
    var GetCharField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jchar>(CPointer<Unit>())
    var GetShortField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jshort>(CPointer<Unit>())
    var GetIntField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jint>(CPointer<Unit>())
    var GetLongField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jlong>(CPointer<Unit>())
    var GetFloatField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jfloat>(CPointer<Unit>())
    var GetDoubleField = CFunc<(JNIEnv_ptr, jobject, jfieldID) -> jdouble>(CPointer<Unit>())

    var SetObjectField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jobject) -> Unit>(CPointer<Unit>())
    var SetBooleanField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jboolean) -> Unit>(CPointer<Unit>())
    var SetByteField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jbyte) -> Unit>(CPointer<Unit>())
    var SetCharField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jchar) -> Unit>(CPointer<Unit>())
    var SetShortField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jshort) -> Unit>(CPointer<Unit>())
    var SetIntField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jint) -> Unit>(CPointer<Unit>())
    var SetLongField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jlong) -> Unit>(CPointer<Unit>())
    var SetFloatField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jfloat) -> Unit>(CPointer<Unit>())
    var SetDoubleField = CFunc<(JNIEnv_ptr, jobject, jfieldID, jdouble) -> Unit>(CPointer<Unit>())

    var GetStaticMethodID = CFunc<(JNIEnv_ptr, jclass, CString, CString) -> jmethodID>(CPointer<Unit>())

    var CallStaticObjectMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jobject>(CPointer<Unit>())
    private var CallStaticObjectMethodV = CPointer<Unit>()
    var CallStaticObjectMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jobject>(CPointer<Unit>())

    var CallStaticBooleanMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jboolean>(CPointer<Unit>())
    private var CallStaticBooleanMethodV = CPointer<Unit>()
    var CallStaticBooleanMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jboolean>(CPointer<Unit>())

    var CallStaticByteMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jbyte>(CPointer<Unit>())
    private var CallStaticByteMethodV = CPointer<Unit>()
    var CallStaticByteMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jbyte>(CPointer<Unit>())

    var CallStaticCharMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jchar>(CPointer<Unit>())
    private var CallStaticCharMethodV = CPointer<Unit>()
    var CallStaticCharMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jchar>(CPointer<Unit>())

    var CallStaticShortMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jshort>(CPointer<Unit>())
    private var CallStaticShortMethodV = CPointer<Unit>()
    var CallStaticShortMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jshort>(CPointer<Unit>())

    var CallStaticIntMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jint>(CPointer<Unit>())
    private var CallStaticIntMethodV = CPointer<Unit>()
    var CallStaticIntMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jint>(CPointer<Unit>())

    var CallStaticLongMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jlong>(CPointer<Unit>())
    private var CallStaticLongMethodV = CPointer<Unit>()
    var CallStaticLongMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jlong>(CPointer<Unit>())

    var CallStaticFloatMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jfloat>(CPointer<Unit>())
    private var CallStaticFloatMethodV = CPointer<Unit>()
    var CallStaticFloatMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jfloat>(CPointer<Unit>())

    var CallStaticDoubleMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> jdouble>(CPointer<Unit>())
    private var CallStaticDoubleMethodV = CPointer<Unit>()
    var CallStaticDoubleMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> jdouble>(CPointer<Unit>())

    var CallStaticVoidMethod = CFunc<(JNIEnv_ptr, jclass, jmethodID) -> Unit>(CPointer<Unit>())
    private var CallStaticVoidMethodV = CPointer<Unit>()
    var CallStaticVoidMethodA = CFunc<(JNIEnv_ptr, jclass, jmethodID, CPointer<jvalue>) -> Unit>(CPointer<Unit>())

    var GetStaticFieldID = CFunc<(JNIEnv_ptr, jclass, CString, CString) -> jfieldID>(CPointer<Unit>())
    var GetStaticObjectField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jobject>(CPointer<Unit>())
    var GetStaticBooleanField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jboolean>(CPointer<Unit>())
    var GetStaticByteField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jbyte>(CPointer<Unit>())
    var GetStaticCharField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jchar>(CPointer<Unit>())
    var GetStaticShortField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jshort>(CPointer<Unit>())
    var GetStaticIntField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jint>(CPointer<Unit>())
    var GetStaticLongField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jlong>(CPointer<Unit>())
    var GetStaticFloatField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jfloat>(CPointer<Unit>())
    var GetStaticDoubleField = CFunc<(JNIEnv_ptr, jclass, jfieldID) -> jdouble>(CPointer<Unit>())

    var SetStaticObjectField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jobject) -> Unit>(CPointer<Unit>())
    var SetStaticBooleanField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jboolean) -> Unit>(CPointer<Unit>())
    var SetStaticByteField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jbyte) -> Unit>(CPointer<Unit>())
    var SetStaticCharField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jchar) -> Unit>(CPointer<Unit>())
    var SetStaticShortField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jshort) -> Unit>(CPointer<Unit>())
    var SetStaticIntField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jint) -> Unit>(CPointer<Unit>())
    var SetStaticLongField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jlong) -> Unit>(CPointer<Unit>())
    var SetStaticFloatField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jfloat) -> Unit>(CPointer<Unit>())
    var SetStaticDoubleField = CFunc<(JNIEnv_ptr, jclass, jfieldID, jdouble) -> Unit>(CPointer<Unit>())

    private var NewString = CPointer<Unit>()
    private var GetStringLength = CPointer<Unit>()
    private var GetStringChars = CPointer<Unit>()
    private var ReleaseStringChars = CPointer<Unit>()

    var NewStringUTF = CFunc<(JNIEnv_ptr, CString) -> jstring>(CPointer<Unit>())
    var GetStringUTFLength = CFunc<(JNIEnv_ptr, jstring) -> jsize>(CPointer<Unit>())
    var GetStringUTFChars = CFunc<(JNIEnv_ptr, jstring, CPointer<jboolean>) -> CString>(CPointer<Unit>())
    var ReleaseStringUTFChars = CFunc<(JNIEnv_ptr, jstring, CString) -> Unit>(CPointer<Unit>())

    var GetArrayLength = CFunc<(JNIEnv_ptr, jarray) -> jsize>(CPointer<Unit>())

    var NewObjectArray = CFunc<(JNIEnv_ptr, jsize, jclass, jobject) -> jobjectArray>(CPointer<Unit>())
    var GetObjectArrayElement = CFunc<(JNIEnv_ptr, jobjectArray, jsize) -> jobject>(CPointer<Unit>())
    var SetObjectArrayElement = CFunc<(JNIEnv_ptr, jobjectArray, jsize, jobject) -> Unit>(CPointer<Unit>())

    var NewBooleanArray = CFunc<(JNIEnv_ptr, jsize) -> jbooleanArray>(CPointer<Unit>())
    var NewByteArray = CFunc<(JNIEnv_ptr, jsize) -> jbyteArray>(CPointer<Unit>())
    var NewCharArray = CFunc<(JNIEnv_ptr, jsize) -> jcharArray>(CPointer<Unit>())
    var NewShortArray = CFunc<(JNIEnv_ptr, jsize) -> jshortArray>(CPointer<Unit>())
    var NewIntArray = CFunc<(JNIEnv_ptr, jsize) -> jintArray>(CPointer<Unit>())
    var NewLongArray = CFunc<(JNIEnv_ptr, jsize) -> jlongArray>(CPointer<Unit>())
    var NewFloatArray = CFunc<(JNIEnv_ptr, jsize) -> jfloatArray>(CPointer<Unit>())
    var NewDoubleArray = CFunc<(JNIEnv_ptr, jsize) -> jdoubleArray>(CPointer<Unit>())

    var GetBooleanArrayElements = CFunc<(JNIEnv_ptr, jbooleanArray, CPointer<jboolean>) -> CPointer<jboolean>>(CPointer<Unit>())
    var GetByteArrayElements = CFunc<(JNIEnv_ptr, jbyteArray, CPointer<jboolean>) -> CPointer<jbyte>>(CPointer<Unit>())
    var GetCharArrayElements = CFunc<(JNIEnv_ptr, jcharArray, CPointer<jboolean>) -> CPointer<jchar>>(CPointer<Unit>())
    var GetShortArrayElements = CFunc<(JNIEnv_ptr, jshortArray, CPointer<jboolean>) -> CPointer<jshort>>(CPointer<Unit>())
    var GetIntArrayElements = CFunc<(JNIEnv_ptr, jintArray, CPointer<jboolean>) -> CPointer<jint>>(CPointer<Unit>())
    var GetLongArrayElements = CFunc<(JNIEnv_ptr, jlongArray, CPointer<jboolean>) -> CPointer<jlong>>(CPointer<Unit>())
    var GetFloatArrayElements = CFunc<(JNIEnv_ptr, jfloatArray, CPointer<jboolean>) -> CPointer<jfloat>>(CPointer<Unit>())
    var GetDoubleArrayElements = CFunc<(JNIEnv_ptr, jdoubleArray, CPointer<jboolean>) -> CPointer<jdouble>>(CPointer<Unit>())

    var ReleaseBooleanArrayElements = CFunc<(JNIEnv_ptr, jbooleanArray, CPointer<jboolean>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseByteArrayElements = CFunc<(JNIEnv_ptr, jbyteArray, CPointer<jbyte>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseCharArrayElements = CFunc<(JNIEnv_ptr, jcharArray, CPointer<jchar>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseShortArrayElements = CFunc<(JNIEnv_ptr, jshortArray, CPointer<jshort>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseIntArrayElements = CFunc<(JNIEnv_ptr, jintArray, CPointer<jint>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseLongArrayElements = CFunc<(JNIEnv_ptr, jlongArray, CPointer<jlong>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseFloatArrayElements = CFunc<(JNIEnv_ptr, jfloatArray, CPointer<jfloat>, jint) -> Unit>(CPointer<Unit>())
    var ReleaseDoubleArrayElements = CFunc<(JNIEnv_ptr, jdoubleArray, CPointer<jdouble>, jint) -> Unit>(CPointer<Unit>())

    var GetBooleanArrayRegion = CFunc<(JNIEnv_ptr, jbooleanArray, jsize, jsize, CPointer<jboolean>) -> Unit>(CPointer<Unit>())
    var GetByteArrayRegion = CFunc<(JNIEnv_ptr, jbyteArray, jsize, jsize, CPointer<jbyte>) -> Unit>(CPointer<Unit>())
    var GetCharArrayRegion = CFunc<(JNIEnv_ptr, jcharArray, jsize, jsize, CPointer<jchar>) -> Unit>(CPointer<Unit>())
    var GetShortArrayRegion = CFunc<(JNIEnv_ptr, jshortArray, jsize, jsize, CPointer<jshort>) -> Unit>(CPointer<Unit>())
    var GetIntArrayRegion = CFunc<(JNIEnv_ptr, jintArray, jsize, jsize, CPointer<jint>) -> Unit>(CPointer<Unit>())
    var GetLongArrayRegion = CFunc<(JNIEnv_ptr, jlongArray, jsize, jsize, CPointer<jlong>) -> Unit>(CPointer<Unit>())
    var GetFloatArrayRegion = CFunc<(JNIEnv_ptr, jfloatArray, jsize, jsize, CPointer<jfloat>) -> Unit>(CPointer<Unit>())
    var GetDoubleArrayRegion = CFunc<(JNIEnv_ptr, jdoubleArray, jsize, jsize, CPointer<jdouble>) -> Unit>(CPointer<Unit>())

    var SetBooleanArrayRegion = CFunc<(JNIEnv_ptr, jbooleanArray, jsize, jsize, CPointer<jboolean>) -> Unit>(CPointer<Unit>())
    var SetByteArrayRegion = CFunc<(JNIEnv_ptr, jbyteArray, jsize, jsize, CPointer<jbyte>) -> Unit>(CPointer<Unit>())
    var SetCharArrayRegion = CFunc<(JNIEnv_ptr, jcharArray, jsize, jsize, CPointer<jchar>) -> Unit>(CPointer<Unit>())
    var SetShortArrayRegion = CFunc<(JNIEnv_ptr, jshortArray, jsize, jsize, CPointer<jshort>) -> Unit>(CPointer<Unit>())
    var SetIntArrayRegion = CFunc<(JNIEnv_ptr, jintArray, jsize, jsize, CPointer<jint>) -> Unit>(CPointer<Unit>())
    var SetLongArrayRegion = CFunc<(JNIEnv_ptr, jlongArray, jsize, jsize, CPointer<jlong>) -> Unit>(CPointer<Unit>())
    var SetFloatArrayRegion = CFunc<(JNIEnv_ptr, jfloatArray, jsize, jsize, CPointer<jfloat>) -> Unit>(CPointer<Unit>())
    var SetDoubleArrayRegion = CFunc<(JNIEnv_ptr, jdoubleArray, jsize, jsize, CPointer<jdouble>) -> Unit>(CPointer<Unit>())

    private var RegisterNatives = CPointer<Unit>()
    private var UnregisterNatives = CPointer<Unit>()

    private var MonitorEnter = CPointer<Unit>()
    private var MonitorExit = CPointer<Unit>()

    private var GetJavaVM = CPointer<Unit>()

    private var GetStringRegion = CPointer<Unit>()
    private var GetStringUTFRegion = CPointer<Unit>()

    private var GetPrimitiveArrayCritical = CPointer<Unit>()
    private var ReleasePrimitiveArrayCritical = CPointer<Unit>()

    private var GetStringCritical = CPointer<Unit>()
    private var ReleaseStringCritical = CPointer<Unit>()

    var NewWeakGlobalRef = CFunc<(JNIEnv_ptr, jobject) -> jweak>(CPointer<Unit>())
    var DeleteWeakGlobalRef = CFunc<(JNIEnv_ptr, jweak) -> Unit>(CPointer<Unit>())

    private var ExceptionCheck = CPointer<Unit>()

    private var NewDirectByteBuffer = CPointer<Unit>()
    private var GetDirectBufferAddress = CPointer<Unit>()
    private var GetDirectBufferCapacity = CPointer<Unit>()

    /* New JNI 1.6 Features */

    private var GetObjectRefType = CPointer<Unit>()

    /* Module Features */

    private var GetModule = CPointer<Unit>()
}

@C
public struct JNIInvokeInterface_ {
    private var reserved0 = CPointer<Unit>()
    private var reserved1 = CPointer<Unit>()
    private var reserved2 = CPointer<Unit>()

    var DestroyJavaVM = CFunc<(CPointer<JavaVM>) -> jint>(CPointer<Unit>())

    var AttachCurrentThread = CFunc<(CPointer<JavaVM>, CPointer<JNIEnv_ptr>, CPointer<Unit>) -> jint>(CPointer<Unit>())

    var DetachCurrentThread = CFunc<(CPointer<JavaVM>) -> jint>(CPointer<Unit>())

    var GetEnv = CFunc<(CPointer<JavaVM>, CPointer<JNIEnv_ptr>, jint) -> jint>(CPointer<Unit>())

    var AttachCurrentThreadAsDaemon = CFunc<(CPointer<JavaVM>, CPointer<JNIEnv_ptr>, CPointer<Unit>) -> jint>(CPointer<Unit>())
}

public class InteropLibException <: Exception {
    public init(reason: String) {
        super(reason)
    }

    protected override func getClassName(): String {
        return "InteropLibException"
    }
}

public enum Java_CFFI_BaseSignature {
    | BYTE    // b
    | CHAR    // c
    | DOUBLE  // d
    | FLOAT   // f
    | INT     // i
    | LONG    // j
    | SHORT   // s
    | BOOLEAN // b
    | VOID    // v TODO: check that field or param cannot be VOID

    public func toString(): String {
        match (this) {
            case BYTE => "byte"
            case CHAR => "char"
            case DOUBLE => "double"
            case FLOAT => "float"
            case INT => "int"
            case LONG => "long"
            case SHORT => "short"
            case BOOLEAN => "boolean"
            case VOID => "void"
        }
    }

    func forJNI(): String {
        match (this) {
            case BYTE => "B"
            case CHAR => "C"
            case DOUBLE => "D"
            case FLOAT => "F"
            case INT => "I"
            case LONG => "J"
            case SHORT => "S"
            case BOOLEAN => "Z"
            case VOID => "V"
        }
    }
}

public enum Java_CFFI_ObjectSignature {
    | REFERENCE(String)                   // LClassName; TODO: class name correctness checks?
    | ARRAY(Java_CFFI_ComponentSignature) // [ComponentSignature

    public func toString(): String {
        match (this) {
            case REFERENCE(ref) => "REFERENCE(${ref.toString()})"
            case ARRAY(component) => "ARRAY(${component.toString()})"
        }
    }

    func forJNI(): String {
        match (this) {
            case REFERENCE(ref) => "L" + ref + ";"
            case ARRAY(component) => "[" + component.forJNI()
        }
    }
}

private func checkObjectSignature(signature: String, parsed: Java_CFFI_ComponentSignature, obj: Java_CFFI_ObjectSignature) : Unit {
    match (obj) {
        case REFERENCE(className) =>
            if (className.size + 2 != signature.size /*L and ;*/) {
                throw InteropLibException("Cannot parse object signature ${signature}")
            }
        case ARRAY(_) => { =>
            let innerClass = Java_CFFI_innermostClass(parsed)
            let innerClassName = Java_CFFI_className(innerClass)
            let dimensions = Java_CFFI_arrayDimensions(parsed)
            
            let correction = match(innerClass) {
                case PRIMITIVE(_) => -innerClassName.size + 1
                case OBJECT(obj) => match(obj) {
                    case REFERENCE(_) => 2
                    case ARRAY(_) => assert(false); -123456
                }
            }

            if (dimensions + innerClassName.size + correction != signature.size) {
                throw InteropLibException("Cannot parse array signature ${signature}")
            }
        }()
    }
}

public enum Java_CFFI_ComponentSignature {
    | PRIMITIVE(Java_CFFI_BaseSignature)
    | OBJECT(Java_CFFI_ObjectSignature)

    public func toString(): String {
        match (this) {
            case PRIMITIVE(base) => "PRIMITIVE(${base.toString()})"
            case OBJECT(obj) => "OBJECT(${obj.toString()})"
        }
    }

    func forJNI(): String {
        match (this) {
            case PRIMITIVE(base) => base.forJNI()
            case OBJECT(obj) => obj.forJNI()
        }
    }
}

public class Java_CFFI_MethodSignature {
    public let params: Array<Java_CFFI_ComponentSignature>
    public let ret: Java_CFFI_ComponentSignature

    init(params: Array<Java_CFFI_ComponentSignature>, ret: Java_CFFI_ComponentSignature) {
        this.params = params
        this.ret = ret
    }

    public func toString(): String {
        let buffer = StringBuilder()

        buffer.append("(")
        for (i in 0..params.size) {
            buffer.append(Java_CFFI_className(params[i]))

            if (i != params.size - 1) {
                buffer.append(", ")
            }
        }
        buffer.append("): ")
        buffer.append(Java_CFFI_className(ret))

        buffer.toString()
    }

    func forJNI(): String {
        let buffer = StringBuilder()

        buffer.append("(")
        for (i in 0..params.size) {
            buffer.append(params[i].forJNI())
        }
        buffer.append(")")
        buffer.append(ret.forJNI())

        buffer.toString()
    }
}

public enum Java_CFFI_EntitySignature {
    | FIELD(Java_CFFI_ComponentSignature)
    | METHOD(Java_CFFI_MethodSignature)

    public func toString(): String {
        match (this) {
            case FIELD(component) => "FIELD(${component.toString()})"
            case METHOD(method) => "METHOD(${method.toString()})"
        }
    }

    func forJNI(): String {
        match (this) {
            case FIELD(component) => component.forJNI()
            case METHOD(method) => method.forJNI()
        }
    }
}

private func Java_CFFI_arrayDimensions(obj: Java_CFFI_ComponentSignature): Int64 {
    func arrayDimensionsStep(o: Java_CFFI_ComponentSignature, step: Int64): Int64 {
        match (o) {
            case OBJECT(ARRAY(component)) => arrayDimensionsStep(component, step + 1)
            case _ => step
        }
    }

    arrayDimensionsStep(obj, 0)
}

private func Java_CFFI_innermostClass(obj: Java_CFFI_ComponentSignature): Java_CFFI_ComponentSignature {
    match (obj) {
        case OBJECT(ARRAY(component)) => Java_CFFI_innermostClass(component)
        case _ => obj
    }
}

private func Java_CFFI_className(obj: Java_CFFI_ComponentSignature): String {
    let buffer = StringBuilder()

    let inner = Java_CFFI_innermostClass(obj)
    let innerName = match (inner) {
        case PRIMITIVE(base) => base.toString()
        case OBJECT(obj) => match (obj) {
            case REFERENCE(name) => name
            case ARRAY(_) => assert(false); "ERROR"
        }
    }
    buffer.append(innerName)
    
    for (_ in 0..Java_CFFI_arrayDimensions(obj)) { 
        buffer.append("[]")
    }

    buffer.toString()
}

private func assert(cond: Bool) {
    if (!cond) {
        throw Exception("interoplib inner error")
    }
}

private func signatureBaseCheck(signature: String) {
    if (signature.isEmpty()) {
        throw InteropLibException("Cannot parse empty signature")
    }
    if (!signature.isAscii()) {
        throw InteropLibException("Invalid non-ASCII signature ${signature}")
    }
}

public func Java_CFFI_parseComponentSignature(signature: String): Java_CFFI_ComponentSignature {
    signatureBaseCheck(signature)
    
    let parsed = Java_CFFI_parseComponentSignatureImpl(unsafe { signature.rawData() }, 0)

    // check that parsed all string
    match (parsed) {
        case OBJECT(obj) => checkObjectSignature(signature, parsed, obj)
        case PRIMITIVE(_) => 
            if (signature.size != 1) {
                throw InteropLibException("Cannot parse primitive signature ${signature}")
            }
    }
    parsed
}

private func classNameBorder(signature: Array<Byte>, begin: Int64): Int64 {
    assert(signature[begin] == b'L')

    var i = begin + 1
    while (signature[i] != b';') {
        if (i >= signature.size) {
            throw InteropLibException("Cannot parse object signature ${String.fromUtf8(signature)} (from ${begin})")
        }
        i += 1
    }
    i
}

private func arrayCopy<T>(dst: Array<T>, dstBegIdx: Int64, src: Array<T>, srcBegIdx: Int64, srcEndIdx: Int64) {
    var i = dstBegIdx
    var j = srcBegIdx

    while (j < srcEndIdx) {
        dst[i] = src[j]
        i += 1
        j += 1
    }
}

private func Java_CFFI_parseComponentSignatureImpl(signature: Array<Byte>, begin: Int64): Java_CFFI_ComponentSignature {
    let remainSize = signature.size - begin
    assert(remainSize > 0)
    
    match (signature[begin]) {
        case b'B' => PRIMITIVE(BYTE)
        case b'C' => PRIMITIVE(CHAR)
        case b'D' => PRIMITIVE(DOUBLE)
        case b'F' => PRIMITIVE(FLOAT)
        case b'I' => PRIMITIVE(INT)
        case b'J' => PRIMITIVE(LONG)
        case b'S' => PRIMITIVE(SHORT)
        case b'Z' => PRIMITIVE(BOOLEAN)
        case b'V' => PRIMITIVE(VOID)
        case b'L' => { =>
            let classNameBegin = begin + 1
            let classNameEnd = classNameBorder(signature, begin)
                
            let classNameLength = classNameEnd - classNameBegin
            let classNameBytes = Array<Byte>(classNameLength, repeat: 0)
            arrayCopy(classNameBytes, 0, signature, classNameBegin, classNameEnd)

            OBJECT(REFERENCE(String.fromUtf8(classNameBytes)))
        }()
        case b'[' => OBJECT(ARRAY(Java_CFFI_parseComponentSignatureImpl(signature, begin + 1)))
        case _ => throw InteropLibException("Cannot parse component signature ${String.fromUtf8(signature)} (from ${begin})")
    }
}

private func restoreObjectSignatureSize(signature: Java_CFFI_ComponentSignature, obj: Java_CFFI_ObjectSignature): Int64 {
    match(obj) {
        case ARRAY(_) => { =>
            let dimensions = Java_CFFI_arrayDimensions(signature)
            let innerClass = Java_CFFI_innermostClass(signature)
            
            let innerClassCorrection = match (innerClass) {
                case PRIMITIVE(_) => 1
                case OBJECT(obj) => match(obj) {
                    case ARRAY(_) => assert(false); -123456
                    case REFERENCE(name) => name.toString().size + 2
                }
            }
            dimensions + innerClassCorrection
        }()
        case REFERENCE(name) => name.toString().size + 2
    }
}

private func restoreComponentSignatureSize(signature: Java_CFFI_ComponentSignature): Int64 {
    match (signature) {
        case PRIMITIVE(_) => 1
        case OBJECT(obj) => restoreObjectSignatureSize(signature, obj)
    }
}

public func Java_CFFI_parseMethodSignature(signature: String): Java_CFFI_MethodSignature {
    signatureBaseCheck(signature)
    if (signature.size < 3) {
        throw InteropLibException("Cannot parse method signature ${signature}")
    }

    var pos = 1
    
    let buffer = ArrayQueue<Java_CFFI_ComponentSignature>()
    while (signature[pos] != b')') {
        let argument = Java_CFFI_parseComponentSignatureImpl(unsafe { signature.rawData() }, pos)
        pos += restoreComponentSignatureSize(argument)
        buffer.add(argument)
    }
    
    pos += 1
    if (signature.size - pos <= 0) {
        throw InteropLibException("Cannot parse method return type for signature ${signature}")
    }
    let ret = Java_CFFI_parseComponentSignatureImpl(unsafe { signature.rawData() }, pos)

    let args = Array<Java_CFFI_ComponentSignature>(buffer.size, repeat: PRIMITIVE(VOID))
    let size = buffer.size
    for (i in 0..size) {
        args[i] = buffer.remove().getOrThrow()
    }

    Java_CFFI_MethodSignature(args, ret)
}

public class Java_CFFI_Class <: ToString {
    var obj = jclass()
    public let name: String

    init(env: JNIEnv_ptr, base: jobject) {
        this.name = "<UNNAMED>"
        unsafe {
            withLocalRef(env, base) { localRef =>
                let envCJ = env.read().read()
                this.obj = envCJ.GetObjectClass(env, localRef)
            }
        }
    }

    public init(env: JNIEnv_ptr, name: String) {
        let size = name.size
        this.name = if (size > 2 && name[0] == b'L' && name[size-1] == b';') {
            name[1..(size - 1)]
        } else {
            name
        }

        unsafe {
            let envCJ = env.read().read()
            let cName = LibC.mallocCString(this.name)
            obj = envCJ.FindClass(env, cName)
            LibC.free(cName)
        }

        if (obj.isNull()) {
            throw InteropLibException("Cannot find class ${name}")
        }
    }

    init(obj: jclass, name: String) {
        this.name = name
        this.obj = obj
    }

    public func toString(): String {
        name
    }
}

public class Java_CFFI_InvariantFailed <: Exception {
    public init() {
        super("interoplib invariant failed")
    }

    protected override func getClassName(): String {
        return "Java_CFFI_InvariantFailed"
    }
}

public open class Java_CFFI_EntityID {
    let signature: Java_CFFI_EntitySignature
    let `static`: Bool

    protected init(signature: Java_CFFI_EntitySignature, `static`: Bool) {
        this.signature = signature
        this.`static` = `static`
    }

    static func constr(env: JNIEnv_ptr, clazz: Java_CFFI_Class, entityName: String, signature: Java_CFFI_EntitySignature, `static`: Bool): Java_CFFI_EntityID {
        let envCJ = unsafe { env.read().read() }
        var id = CPointer<Unit>()
        unsafe {
            let cSignature = LibC.mallocCString(signature.forJNI())
            let cEntityName = LibC.mallocCString(entityName)
            match (signature) {
                case FIELD(_) => if (!`static`) {
                    id = envCJ.GetFieldID(env, clazz.obj, cEntityName, cSignature)
                } else {
                    id = envCJ.GetStaticFieldID(env, clazz.obj, cEntityName, cSignature)
                }
                case METHOD(_) => if (!`static`) {
                    id = envCJ.GetMethodID(env, clazz.obj, cEntityName, cSignature)
                } else {
                    id = envCJ.GetStaticMethodID(env, clazz.obj, cEntityName, cSignature)
                }
            }
            LibC.free(cSignature)
            LibC.free(cEntityName)
        }

        if (id.isNull()) {
            if (!`static`) {
                throw InteropLibException("Cannot find entity ${entityName} with signature ${signature.toString()} in class ${clazz.name}")
            } else {
                throw InteropLibException("Cannot find static entity ${entityName} with signature ${signature.toString()} in class ${clazz.name}")
            }
        }

        match (signature) {
            case FIELD(_) => Java_CFFI_FieldID(signature, id, `static`)
            case METHOD(_) => Java_CFFI_MethodID(signature, id, `static`)
        }
    }
}

public class Java_CFFI_FieldID <: Java_CFFI_EntityID {
    let id: jfieldID
    
    init(signature: Java_CFFI_EntitySignature, id: jfieldID, `static`: Bool) {
        super(signature, `static`)
        this.id = id
    }

    public static func constrStatic(env: JNIEnv_ptr, clazz: Java_CFFI_Class, fieldName: String, signature: Java_CFFI_ComponentSignature): Java_CFFI_FieldID {
        match (signature) {
            case PRIMITIVE(base) => match (base) {
                case VOID => throw InteropLibException("Field cannot have void type")
                case _ => ()
            }
            case OBJECT(_) => ()
        }
        match (Java_CFFI_EntityID.constr(env, clazz, fieldName, FIELD(signature), true)) {
            case field: Java_CFFI_FieldID => field
            case _ => throw Java_CFFI_InvariantFailed()
        }
    }

    public static func constr(env: JNIEnv_ptr, clazz: Java_CFFI_Class, fieldName: String, signature: Java_CFFI_ComponentSignature): Java_CFFI_FieldID {
        match (signature) {
            case PRIMITIVE(base) => match (base) {
                case VOID => throw InteropLibException("Field cannot have void type")
                case _ => ()
            }
            case OBJECT(_) => ()
        }
        match (Java_CFFI_EntityID.constr(env, clazz, fieldName, FIELD(signature), false)) {
            case field: Java_CFFI_FieldID => field
            case _ => throw Java_CFFI_InvariantFailed()
        }
    }
}

public class Java_CFFI_MethodID <: Java_CFFI_EntityID {
    let id: jmethodID

    init(signature: Java_CFFI_EntitySignature, id: jmethodID, `static`: Bool) {
        super(signature, `static`)
        this.id = id
    }

    public static func constrStatic(env: JNIEnv_ptr, clazz: Java_CFFI_Class, methodName: String, signature: Java_CFFI_MethodSignature): Java_CFFI_MethodID {
        match (Java_CFFI_EntityID.constr(env, clazz, methodName, METHOD(signature), true)) {
            case method: Java_CFFI_MethodID => method
            case _ => throw Java_CFFI_InvariantFailed()
        }
    }

    public static func constr(env: JNIEnv_ptr, clazz: Java_CFFI_Class, methodName: String, signature: Java_CFFI_MethodSignature): Java_CFFI_MethodID {
        match (Java_CFFI_EntityID.constr(env, clazz, methodName, METHOD(signature), false)) {
            case method: Java_CFFI_MethodID => method
            case _ => throw Java_CFFI_InvariantFailed()
        }
    }
}

private func withLocalRef<R>(env: JNIEnv_ptr, obj: jobject, action: (ref: jobject) -> R): R {
    unsafe {
        let envCJ = env.read().read()
        let localRef = envCJ.NewLocalRef(env, obj)
        try {
            action(localRef)
        } finally {
            envCJ.DeleteLocalRef(env, localRef)
        }
    }
}

private func withPinnedArray<T, R>(array: Array<T>, action: (array: CPointer<T>) -> R): R where T <: CType {
    unsafe {
        let handle = acquireArrayRawData<T>(array)
        try {
            action(handle.pointer)
        } finally {
            releaseArrayRawData<T>(handle)
        }
    }
}

public enum Java_CFFI_JavaEntityKind <: ToString {
    | JVOID
    | JOBJECT
    | JWEAK
    | JSTRONG
    | JLONG
    | JINT
    | JSTRING
    | JBOOLEAN
    | JSHORT
    | JCHAR
    | JBYTE
    | JFLOAT
    | JDOUBLE
    | JVALUE
    // TODO: is JARRAY needed? Use JOBJECT instead.

    public func toString(): String {
        match (this) {
            case JVOID => "void"
            case JOBJECT => "jobject"
            case JWEAK => "jweak"
            case JSTRONG => "jstrong"
            case JLONG => "jlong"
            case JINT => "jint"
            case JSTRING => "jstring"
            case JBOOLEAN => "jboolean"
            case JSHORT => "jshort"
            case JCHAR => "jchar"
            case JBYTE => "jbyte"
            case JFLOAT => "jfloat"
            case JDOUBLE => "jdouble"
            case JVALUE => "jvalue"
        }
    }
}

private unsafe func ptrFromInt(value: UInt64): CPointer<Unit> {
    CPointer<Unit>(CPointer<UInt8>() + Int64(value))
}

// TODO: implement without heap memory allocation (alloca? intrinsic?)
private unsafe func reinterpretCast<A, B>(value: B): A where A <: CType, B <: CType {
    var newValue = zeroValue<A>()

    try (fBox = LibC.malloc<B>(count: 1).asResource()) {
        fBox.value.write(value)

        let iBox = CPointer<A>(fBox.value)
        newValue = iBox.read()
    }

    newValue
}

public struct Java_CFFI_JavaEntity {
    let kind: Java_CFFI_JavaEntityKind
    private let value: jvalue

    public init() {
        this.kind = JVOID
        this.value = 0
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: CPointer<Unit>) {
        this.kind = kind
        this.value = UInt64(value.toUIntNative())
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: UInt64) {
        this.kind = kind
        this.value = value
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: Int64) {
        this.kind = kind
        this.value = UInt64(value)
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: Int32) {
        this.kind = kind
        this.value = UInt64(value)
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: Int16) {
        this.kind = kind
        this.value = UInt64(value)
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: UInt16) {
        this.kind = kind
        this.value = UInt64(value)
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: Int8) {
        this.kind = kind
        this.value = UInt64(value)
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: UInt8) {
        this.kind = kind
        this.value = UInt64(value)
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: Float32) {
        this.kind = kind
        this.value = unsafe { UInt64(reinterpretCast<UInt32, Float32>(value)) }
    }

    public init(kind: Java_CFFI_JavaEntityKind, value: Float64) {
        this.kind = kind
        this.value = unsafe { reinterpretCast<UInt64, Float64>(value) }
    }

    public init(value: Int64) {
        this(JLONG, value)
    }

    public init(value: Int32) {
        this(JINT, value)
    }

    public init(value: Int16) {
        this(JSHORT, value)
    }

    public init(value: UInt16) {
        this(JCHAR, value)
    }

    public init(value: Int8) {
        this(JBYTE, value)
    }

    public init(value: Bool) {
        this(JBOOLEAN, if (value) { 1u8 } else { 0u8 })
    }

    public init(value: Float32) {
        this(JFLOAT, value)
    }

    public init(value: Float64) {
        this(JDOUBLE, value)
    }

    public unsafe func asUnit(): Unit {
        match (kind) {
            case JVOID => ()
            case _ => throw InteropLibException("Cannot get void from ${kind.toString()}")
        }
    }

    public unsafe func asJObject(): jobject {
        match (kind) {
            case JVALUE => jobject(ptrFromInt(value))
            case JOBJECT => jobject(ptrFromInt(value))
            case JSTRING => jobject(ptrFromInt(value))
            case JWEAK => jobject(ptrFromInt(value))
            case JSTRONG => jobject(ptrFromInt(value))
            case _ => throw InteropLibException("Cannot get jobject from ${kind.toString()}")
        }
    }

    public unsafe func asJWeak(): jobject {
        match (kind) {
            case JWEAK => jobject(ptrFromInt(value))
            case _ => throw InteropLibException("Cannot get jweak from ${kind.toString()}")
        }
    }

    public unsafe func asJStrong(): jobject {
        match (kind) {
            case JSTRONG => jobject(ptrFromInt(value))
            case _ => throw InteropLibException("Cannot get jstrong from ${kind.toString()}")
        }
    }

    public unsafe func asJLong(): jlong { 
        match (kind) {
            case JVALUE => Int64(value)
            case JLONG => Int64(value)
            case _ => throw InteropLibException("Cannot get jlong from ${kind.toString()}")
        }
    }

    public unsafe func asJInt(): jint {
        match (kind) {
            case JVALUE => Int32(value)
            case JINT => Int32(value)
            case _ => throw InteropLibException("Cannot get jint from ${kind.toString()}")
        }
    }

    public unsafe func asJString(): jstring {
        match (kind) {
            case JVALUE => jstring(ptrFromInt(value))
            case JOBJECT => jstring(ptrFromInt(value))
            case JSTRING => jstring(ptrFromInt(value))
            case _ => throw InteropLibException("Cannot get jstring from ${kind.toString()}")
        }
    }

    public unsafe func asJBoolean(): jboolean {
        match (kind) {
            case JVALUE => UInt8(value)
            case JBOOLEAN => UInt8(value)
            case _ => throw InteropLibException("Cannot get jboolean from ${kind.toString()}")
        }
    }

    public unsafe func asJShort(): jshort {
        match (kind) {
            case JVALUE => Int16(value)
            case JSHORT => Int16(value)
            case _ => throw InteropLibException("Cannot get jshort from ${kind.toString()}")
        }
    }

    public unsafe func asJChar(): jchar {
        match (kind) {
            case JVALUE => UInt16(value)
            case JCHAR => UInt16(value)
            case _ => throw InteropLibException("Cannot get jchar from ${kind.toString()}")
        }
    }

    public unsafe func asJByte(): jbyte {
        match (kind) {
            case JVALUE => Int8(value)
            case JBYTE => Int8(value)
            case _ => throw InteropLibException("Cannot get jbyte from ${kind.toString()}")
        }
    }

    public unsafe func asJFloat(): jfloat {
        match (kind) {
            case JVALUE => reinterpretCast<Float32, UInt32>(UInt32(value))
            case JFLOAT => reinterpretCast<Float32, UInt32>(UInt32(value))
            case _ => throw InteropLibException("Cannot get jfloat from ${kind.toString()}")
        }
    }

    public unsafe func asJDouble(): jdouble {
        match (kind) {
            case JVALUE => reinterpretCast<Float64, UInt64>(value)
            case JDOUBLE => reinterpretCast<Float64, UInt64>(value)
            case _ => throw InteropLibException("Cannot get jdouble from ${kind.toString()}")
        }
    }

    /* public */ unsafe func asJValue(): jvalue {
        value
    }

    public prop isNull: Bool {
        get() {
            return value == 0u64
        }
    }
}

public class Java_CFFI_JavaCallNest {
    let array: Array<jvalue>
    let size: Int64

    init() {
        array = []
        size = 0
    }

    public init(n: Int64) {
        array = Array<jvalue>(n, repeat: 0)
        size = array.size
    }
}

private unsafe func noParamsCallMethod(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, method: Java_CFFI_MethodID, signature: Java_CFFI_MethodSignature): Java_CFFI_JavaEntity {
    match (signature.ret) {
        case PRIMITIVE(base) => match (base) {
            case BYTE =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualByteMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JBYTE, jResult)
                }
            case CHAR =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualCharMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JCHAR, jResult)
                }
            case DOUBLE =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualDoubleMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JDOUBLE, jResult)
                }
            case FLOAT =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualFloatMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JFLOAT, jResult)
                }
            case INT =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualIntMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JINT, jResult)
                }
            case LONG =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualLongMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JLONG, jResult)
                }
            case SHORT =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualShortMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JSHORT, jResult)
                }
            case BOOLEAN =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualBooleanMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                    Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                }
            case VOID =>
                withLocalRef(env, obj.asJObject()) { localRef =>
                    let envCJ = env.read().read()
                    envCJ.CallNonvirtualVoidMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                }
                Java_CFFI_JavaEntity()
        }
        case OBJECT(object) => 
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.CallNonvirtualObjectMethod(env, localRef, envCJ.GetObjectClass(env, localRef), method.id)
                Java_CFFI_JavaEntity(JOBJECT, jResult)
            }
    }
}

private unsafe func withParamsCallMethod(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, method: Java_CFFI_MethodID, signature: Java_CFFI_MethodSignature, argsCallNest: Java_CFFI_JavaCallNest): Java_CFFI_JavaEntity {
    match (signature.ret) {
        case PRIMITIVE(base) => match (base) {
            case BYTE =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr => 
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualByteMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JBYTE, jResult)
                    }
                }
            case CHAR =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualCharMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JCHAR, jResult)
                    }
                }
            case DOUBLE =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualDoubleMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JDOUBLE, jResult)
                    }
                }
            case FLOAT =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualFloatMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JFLOAT, jResult)
                    }
                }
            case INT =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualIntMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JINT, jResult)
                    }
                }
            case LONG =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualLongMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JLONG, jResult)
                    }
                }
            case SHORT =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualShortMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JSHORT, jResult)
                    }
                }
            case BOOLEAN =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallNonvirtualBooleanMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                        Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                    }
                }
            case VOID =>
                withLocalRef(env, obj.asJObject()) { localRef => 
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        envCJ.CallNonvirtualVoidMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                    }
                }
                Java_CFFI_JavaEntity()
        }
        case OBJECT(object) =>
            withLocalRef(env, obj.asJObject()) { localRef => 
                withPinnedArray(argsCallNest.array) { argsPtr =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallNonvirtualObjectMethodA(env, localRef, envCJ.GetObjectClass(env, localRef), method.id, argsPtr)
                    Java_CFFI_JavaEntity(JOBJECT, jResult)
                }
            }
    }
}

public func Java_CFFI_callMethod(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, method: Java_CFFI_MethodID, args: Array<Java_CFFI_JavaEntity>, argsCallNest: Java_CFFI_JavaCallNest): Java_CFFI_JavaEntity {
    if (args.size > argsCallNest.array.size) {
        throw InteropLibException("Cannot perform call, too small argsCallNest.array (${argsCallNest.size}, required at least ${args.size})")
    }
    if (method.`static`) {
        throw InteropLibException("Cannot perform non-static call to static entity")
    }
    let signature = match(method.signature) {
        case METHOD(methodSignature) => methodSignature
        case FIELD(_) => throw Java_CFFI_InvariantFailed()
    }
    
    if (args.size != signature.params.size) {
        throw InteropLibException("Given ${args.size} arguments for ${signature.params.size} arguments method")
    }

    let res = unsafe {
        if (signature.params.size == 0) {
            noParamsCallMethod(env, obj, method, signature)
        } else {
            for (i in 0..args.size) {
                argsCallNest.array[i] = args[i].asJValue()
            }
            withParamsCallMethod(env, obj, method, signature, argsCallNest)
        }
    }
    handlePendingException(env)
    res
}

public func Java_CFFI_callVirtualMethod(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, method: Java_CFFI_MethodID, args: Array<Java_CFFI_JavaEntity>, argsCallNest: Java_CFFI_JavaCallNest): Java_CFFI_JavaEntity {
    let res = Java_CFFI_callVirtualMethod_no_exception_handling(env, obj, method, args, argsCallNest)
    handlePendingException(env)
    res
}

private func Java_CFFI_callVirtualMethod_no_exception_handling(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, method: Java_CFFI_MethodID, args: Array<Java_CFFI_JavaEntity>, argsCallNest: Java_CFFI_JavaCallNest): Java_CFFI_JavaEntity {
    if (args.size > argsCallNest.array.size) {
        throw InteropLibException("Cannot perform call, too small argsCallNest.array (${argsCallNest.size}, required at least ${args.size})")
    }
    if (method.`static`) {
        throw InteropLibException("Cannot perform non-static call to static entity")
    }
    let signature = match(method.signature) {
        case METHOD(methodSignature) => methodSignature
        case FIELD(_) => throw Java_CFFI_InvariantFailed()
    }
    
    if (args.size != signature.params.size) {
        throw InteropLibException("Given ${args.size} arguments for ${signature.params.size} arguments method")
    }

    unsafe {
        if (signature.params.size == 0) {
            match (signature.ret) {
                case PRIMITIVE(base) => match (base) {
                    case BYTE =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallByteMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JBYTE, jResult)
                        }
                    case CHAR =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallCharMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JCHAR, jResult)
                        }
                    case DOUBLE =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallDoubleMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JDOUBLE, jResult)
                        }
                    case FLOAT =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallFloatMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JFLOAT, jResult)
                        }
                    case INT =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallIntMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JINT, jResult)
                        }
                    case LONG =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallLongMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JLONG, jResult)
                        }
                    case SHORT =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallShortMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JSHORT, jResult)
                        }
                    case BOOLEAN =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallBooleanMethod(env, localRef, method.id)
                            Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                        }
                    case VOID =>
                        withLocalRef(env, obj.asJObject()) { localRef =>
                            let envCJ = env.read().read()
                            envCJ.CallVoidMethod(env, localRef, method.id)
                        }
                        Java_CFFI_JavaEntity()
                }
                case OBJECT(object) => 
                    withLocalRef(env, obj.asJObject()) { localRef =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallObjectMethod(env, localRef, method.id)
                        Java_CFFI_JavaEntity(JOBJECT, jResult)
                    }
            }
        } else {
            for (i in 0..args.size) {
                argsCallNest.array[i] = args[i].asJValue()
            }
            match (signature.ret) {
                case PRIMITIVE(base) => match (base) {
                    case BYTE =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr => 
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallByteMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JBYTE, jResult)
                            }
                        }
                    case CHAR =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallCharMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JCHAR, jResult)
                            }
                        }
                    case DOUBLE =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallDoubleMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JDOUBLE, jResult)
                            }
                        }
                    case FLOAT =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallFloatMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JFLOAT, jResult)
                            }
                        }
                    case INT =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallIntMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JINT, jResult)
                            }
                        }
                    case LONG =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallLongMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JLONG, jResult)
                            }
                        }
                    case SHORT =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallShortMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JSHORT, jResult)
                            }
                        }
                    case BOOLEAN =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                let jResult = envCJ.CallBooleanMethodA(env, localRef, method.id, argsPtr)
                                Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                            }
                        }
                    case VOID =>
                        withLocalRef(env, obj.asJObject()) { localRef => 
                            withPinnedArray(argsCallNest.array) { argsPtr =>
                                let envCJ = env.read().read()
                                envCJ.CallVoidMethodA(env, localRef, method.id, argsPtr)
                            }
                        }
                        Java_CFFI_JavaEntity()
                }
                case OBJECT(object) =>
                    withLocalRef(env, obj.asJObject()) { localRef => 
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallObjectMethodA(env, localRef, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JOBJECT, jResult)
                        }
                    }
            }
        }
    }
}

private func Java_CFFI_callStaticMethod(env: JNIEnv_ptr, clazz: Java_CFFI_Class, methodName: String, methodSignature: String, args: Array<Java_CFFI_JavaEntity>): Java_CFFI_JavaEntity {
    let signature = Java_CFFI_parseMethodSignature(methodSignature)
    let id = Java_CFFI_MethodID.constrStatic(env, clazz, methodName, signature)
    let callNest = Java_CFFI_JavaCallNest(args.size)

    Java_CFFI_callStaticMethod(env, clazz, id, args, callNest)
}

public func Java_CFFI_callStaticMethod(env: JNIEnv_ptr, clazz: Java_CFFI_Class, method: Java_CFFI_MethodID, args: Array<Java_CFFI_JavaEntity>, argsCallNest: Java_CFFI_JavaCallNest): Java_CFFI_JavaEntity {
    if (args.size > argsCallNest.array.size) {
        throw InteropLibException("Cannot perform call, too small argsCallNest.array (${argsCallNest.size}, required at least ${args.size})")
    }
    if (!method.`static`) {
        throw InteropLibException("Cannot perform static call to non-static entity")
    }
    let signature = match(method.signature) {
        case METHOD(methodSignature) => methodSignature
        case FIELD(_) => throw Java_CFFI_InvariantFailed()
    }
    
    if (args.size != signature.params.size) {
        throw InteropLibException("Given ${args.size} arguments for ${signature.params.size} arguments method")
    }

    let res = unsafe {
        if (signature.params.size == 0) {
            match (signature.ret) {
                case PRIMITIVE(base) => match (base) {
                    case BYTE =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticByteMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JBYTE, jResult)
                    case CHAR =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticCharMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JCHAR, jResult)
                    case DOUBLE =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticDoubleMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JDOUBLE, jResult)
                    case FLOAT =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticFloatMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JFLOAT, jResult)
                    case INT =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticIntMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JINT, jResult)
                    case LONG =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticLongMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JLONG, jResult)
                    case SHORT =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticShortMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JSHORT, jResult)
                    case BOOLEAN =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticBooleanMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                    case VOID =>
                        let envCJ = env.read().read()
                        envCJ.CallStaticVoidMethod(env, clazz.obj, method.id)
                        Java_CFFI_JavaEntity()
                }
                case OBJECT(object) =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.CallStaticObjectMethod(env, clazz.obj, method.id)
                    Java_CFFI_JavaEntity(JOBJECT, jResult)
            }
        } else {
            for (i in 0..args.size) {
                argsCallNest.array[i] = args[i].asJValue()
            }
            match (signature.ret) {
                case PRIMITIVE(base) => match (base) {
                    case BYTE =>
                        withPinnedArray(argsCallNest.array) { argsPtr => 
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticByteMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JBYTE, jResult)
                        }
                    case CHAR =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticCharMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JCHAR, jResult)
                        }
                    case DOUBLE =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticDoubleMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JDOUBLE, jResult)
                        }
                    case FLOAT =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticFloatMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JFLOAT, jResult)
                        }
                    case INT =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticIntMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JINT, jResult)
                        }
                    case LONG =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticLongMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JLONG, jResult)
                        }
                    case SHORT =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticShortMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JSHORT, jResult)
                        }
                    case BOOLEAN =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            let jResult = envCJ.CallStaticBooleanMethodA(env, clazz.obj, method.id, argsPtr)
                            Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                        }
                    case VOID =>
                        withPinnedArray(argsCallNest.array) { argsPtr =>
                            let envCJ = env.read().read()
                            envCJ.CallStaticVoidMethodA(env, clazz.obj, method.id, argsPtr)
                        }
                        Java_CFFI_JavaEntity()
                }
                case OBJECT(object) =>
                    withPinnedArray(argsCallNest.array) { argsPtr =>
                        let envCJ = env.read().read()
                        let jResult = envCJ.CallStaticObjectMethodA(env, clazz.obj, method.id, argsPtr)
                        Java_CFFI_JavaEntity(JOBJECT, jResult)
                    }
            }
        }
    }
    handlePendingException(env)
    res
}

private unsafe func getPrimitiveFieldJavaEntity(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity,
    base: Java_CFFI_BaseSignature, id: jfieldID): Java_CFFI_JavaEntity {
    match (base) {
        case BYTE =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetByteField(env, localRef, id)
                Java_CFFI_JavaEntity(JBYTE, jResult)
            }
        case CHAR =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetCharField(env, localRef, id)
                Java_CFFI_JavaEntity(JCHAR, jResult)
            }
        case DOUBLE =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetDoubleField(env, localRef, id)
                Java_CFFI_JavaEntity(JDOUBLE, jResult)
            }
        case FLOAT =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetFloatField(env, localRef, id)
                Java_CFFI_JavaEntity(JFLOAT, jResult)
            }
        case INT =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetIntField(env, localRef, id)
                Java_CFFI_JavaEntity(JINT, jResult)
            }
        case LONG =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetLongField(env, localRef, id)
                Java_CFFI_JavaEntity(JLONG, jResult)
            }
        case SHORT =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetShortField(env, localRef, id)
                Java_CFFI_JavaEntity(JSHORT, jResult)
            }
        case BOOLEAN =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetBooleanField(env, localRef, id)
                Java_CFFI_JavaEntity(JBOOLEAN, jResult)
            }
        case VOID => throw Java_CFFI_InvariantFailed()
    }
}

private unsafe func getFieldJavaEntity(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity,
    signature: Java_CFFI_ComponentSignature, id: jfieldID): Java_CFFI_JavaEntity {
    match (signature) {
        case PRIMITIVE(base) => getPrimitiveFieldJavaEntity(env, obj, base, id)
        case OBJECT(object) =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetObjectField(env, localRef, id)
                Java_CFFI_JavaEntity(JOBJECT, jResult)
            }
    }
}

public func Java_CFFI_getField(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, field: Java_CFFI_FieldID): Java_CFFI_JavaEntity {
    if (field.`static`) {
        throw InteropLibException("Cannot get static entity from non-static obj")
    }
    let signature = match(field.signature) {
        case METHOD(_) => throw Java_CFFI_InvariantFailed()
        case FIELD(fieldSignature) => fieldSignature
    }
    let res = unsafe { getFieldJavaEntity(env, obj, signature, field.id) }
    handlePendingException(env)
    res
}

public func Java_CFFI_getStaticField(env: JNIEnv_ptr, clazz: Java_CFFI_Class, field: Java_CFFI_FieldID): Java_CFFI_JavaEntity {
    if (!field.`static`) {
        throw InteropLibException("Cannot get non-static entity from class")
    }
    let signature = match(field.signature) {
        case METHOD(_) => throw Java_CFFI_InvariantFailed()
        case FIELD(fieldSignature) => fieldSignature
    }

    let res = unsafe {
        match (signature) {
            case PRIMITIVE(base) => match (base) {
                case BYTE =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticByteField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JBYTE, jResult)
                case CHAR =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticCharField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JCHAR, jResult)
                case DOUBLE =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticDoubleField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JDOUBLE, jResult)
                case FLOAT =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticFloatField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JFLOAT, jResult)
                case INT =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticIntField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JINT, jResult)
                case LONG =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticLongField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JLONG, jResult)
                case SHORT =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticShortField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JSHORT, jResult)
                case BOOLEAN =>
                    let envCJ = env.read().read()
                    let jResult = envCJ.GetStaticBooleanField(env, clazz.obj, field.id)
                    Java_CFFI_JavaEntity(JBOOLEAN, jResult)
                case VOID => throw Java_CFFI_InvariantFailed()
            }
            case OBJECT(object) =>
                let envCJ = env.read().read()
                let jResult = envCJ.GetStaticObjectField(env, clazz.obj, field.id)
                Java_CFFI_JavaEntity(JOBJECT, jResult)
        }
    }
    handlePendingException(env)
    res
}

private func Java_CFFI_setField(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, fieldName: String, fieldSignature: String, value: Java_CFFI_JavaEntity): Unit {
    let clazz = Java_CFFI_Class(unsafe { obj.asJObject() }, "<UNNAMED>")
    let signature = Java_CFFI_parseComponentSignature(fieldSignature)
    let id = Java_CFFI_FieldID.constr(env, clazz, fieldName, signature)

    Java_CFFI_setField(env, obj, id, value)
}

private unsafe func setPrimitiveFieldJavaEntity(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity,
    base: Java_CFFI_BaseSignature, id: jfieldID, value: Java_CFFI_JavaEntity): Unit {
    match (base) {
        case BYTE =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetByteField(env, localRef, id, value.asJByte())
            }
        case CHAR =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetCharField(env, localRef, id, value.asJChar())
            }
        case DOUBLE =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetDoubleField(env, localRef, id, value.asJDouble())
            }
        case FLOAT =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetFloatField(env, localRef, id, value.asJFloat())
            }
        case INT =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetIntField(env, localRef, id, value.asJInt())
            }
        case LONG =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetLongField(env, localRef, id, value.asJLong())
            }
        case SHORT =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetShortField(env, localRef, id, value.asJShort())
            }
        case BOOLEAN =>
            withLocalRef(env, obj.asJObject()) { localRef =>
                let envCJ = env.read().read()
                envCJ.SetBooleanField(env, localRef, id, value.asJBoolean())
            }
        case VOID => throw Java_CFFI_InvariantFailed()
    }
}

private unsafe func setFieldJavaEntity(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity,
    signature: Java_CFFI_ComponentSignature, id: jfieldID, value: Java_CFFI_JavaEntity): Unit {
    match (signature) {
        case PRIMITIVE(base) => setPrimitiveFieldJavaEntity(env, obj, base, id, value)
        case OBJECT(object) =>
            withLocalRef(env, obj.asJObject()) { localRef => 
                let envCJ = env.read().read()
                envCJ.SetObjectField(env, localRef, id, value.asJObject())
            }
    }
}

public func Java_CFFI_setField(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, field: Java_CFFI_FieldID, value: Java_CFFI_JavaEntity): Unit {
    if (field.`static`) {
        throw InteropLibException("Cannot set static entity to non-static obj")
    }
    let signature = match(field.signature) {
        case METHOD(_) => throw Java_CFFI_InvariantFailed()
        case FIELD(fieldSignature) => fieldSignature
    }
    unsafe { setFieldJavaEntity(env, obj, signature, field.id, value)}
    handlePendingException(env)
}

public func Java_CFFI_setStaticField(env: JNIEnv_ptr, clazz: Java_CFFI_Class, field: Java_CFFI_FieldID, value: Java_CFFI_JavaEntity): Unit {
    if (!field.`static`) {
        throw InteropLibException("Cannot set non-static entity to class")
    }
    let signature = match(field.signature) {
        case METHOD(_) => throw Java_CFFI_InvariantFailed()
        case FIELD(fieldSignature) => fieldSignature
    }

    unsafe {
        match (signature) {
            case PRIMITIVE(base) => match (base) {
                case BYTE =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticByteField(env, clazz.obj, field.id, value.asJByte())
                case CHAR =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticCharField(env, clazz.obj, field.id, value.asJChar())
                case DOUBLE =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticDoubleField(env, clazz.obj, field.id, value.asJDouble())
                case FLOAT =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticFloatField(env, clazz.obj, field.id, value.asJFloat())
                case INT =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticIntField(env, clazz.obj, field.id, value.asJInt())
                case LONG =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticLongField(env, clazz.obj, field.id, value.asJLong())
                case SHORT =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticShortField(env, clazz.obj, field.id, value.asJShort())
                case BOOLEAN =>
                    let envCJ = env.read().read()
                    envCJ.SetStaticBooleanField(env, clazz.obj, field.id, value.asJBoolean())
                case VOID => throw Java_CFFI_InvariantFailed()
            }
            case OBJECT(object) =>
                let envCJ = env.read().read()
                envCJ.SetStaticObjectField(env, clazz.obj, field.id, value.asJObject())
        }
    }
    handlePendingException(env)
}

public abstract class Java_CFFI_SafeInteropPreparationMixin {
    private static var JavaInteropPreparationLock = AtomicBool(false)
    private static var JavaInteropPrepared = AtomicBool(false)

    public static func prepareForJavaInterop(env: JNIEnv_ptr, prepareForJavaInteropImpl: (JNIEnv_ptr) -> Unit) {
        if (!JavaInteropPrepared.load()) {
            if (JavaInteropPreparationLock.compareAndSwap(false, true)) {
                prepareForJavaInteropImpl(env)

                if (JavaInteropPrepared.swap(true)) {
                    throw Java_CFFI_InvariantFailed()
                }

                if (!JavaInteropPreparationLock.swap(false)) {
                    throw Java_CFFI_InvariantFailed()
                }
            } else {
                while (!JavaInteropPrepared.load()) { }
            }
        }
    }
}

public func Java_CFFI_JavaStringToCangjie(env: JNIEnv_ptr, str: Java_CFFI_JavaEntity): String {
    unsafe {
        let jStr = str.asJString()
        let envCJ = env.read().read()
        let length = Int64(envCJ.GetStringUTFLength(env, jStr))

        if (length == 0) {
            return ""
        }

        let chars = envCJ.GetStringUTFChars(env, jStr, CPointer<jboolean>())
        try {
            if (chars.isNull()) {
                throw InteropLibException("Cannot retrieve Java String chars")
            }

            String.fromUtf8(Array<UInt8>(length, { i: Int64 => (chars.getChars() + i).read() }))
        } finally {
            if (!chars.isNull()) {
                envCJ.ReleaseStringUTFChars(env, jStr, chars)
            }
        }
    }
}

public func Java_CFFI_CangjieStringToJava(env: JNIEnv_ptr, str: String): Java_CFFI_JavaEntity {
    Java_CFFI_JavaEntity(JSTRING, to_jstring(env, str))
}

public func Java_CFFI_newJavaObject(env: JNIEnv_ptr, clazz: Java_CFFI_Class, constructor: Java_CFFI_MethodID, args: Array<Java_CFFI_JavaEntity>, argsCallNest: Java_CFFI_JavaCallNest): Java_CFFI_JavaEntity {
    if (args.size > argsCallNest.size) {
        throw InteropLibException("Cannot perform call, too small argsCallNest (${argsCallNest.size}, required at least ${args.size})")
    }
    if (constructor.`static`) {
        throw InteropLibException("Constructor cannot be static")
    }
    let signature = match(constructor.signature) {
        case METHOD(constructorSignature) => constructorSignature
        case FIELD(_) => throw Java_CFFI_InvariantFailed()
    }

    match (signature.ret) {
        case PRIMITIVE(base) => match (base) {
            case VOID => ()
            case _ => throw InteropLibException("Java constructor return type should be void")
        }
        case OBJECT(_) => throw InteropLibException("Java constructor return type should be void")
    }
    
    if (args.size != signature.params.size) {
        throw InteropLibException("Given ${args.size} arguments for ${signature.params.size} arguments constructor")
    }

    let res = if (signature.params.size == 0) {
        unsafe {
            let envCJ = env.read().read()
            let jResult = envCJ.NewObject(env, clazz.obj, constructor.id)
            Java_CFFI_JavaEntity(JOBJECT, jResult)
        }
    } else {
        for (i in 0..args.size) {
            argsCallNest.array[i] = unsafe { args[i].asJValue() }
        }
        let result = Box(Java_CFFI_JavaEntity())
        withPinnedArray(argsCallNest.array) { argsPtr: CPointer<jvalue> => 
            unsafe {
                let envCJ = env.read().read()
                let jResult = envCJ.NewObjectA(env, clazz.obj, constructor.id, argsPtr)
                result.value = Java_CFFI_JavaEntity(JOBJECT, jResult)
            }
        }
        result.value
    }
    handlePendingException(env)
    res
}

// The constructor of the proxy class for structTy contains a special parameter, whereas EnumTy
// does not require it. 
public func Java_CFFI_newJavaProxyObjectForCJMapping(env: JNIEnv_ptr, entity: Object, name: String,
    withMarkerParam: Bool): jobject {
    let id = Java_CFFI_put_to_registry_1(entity)
    let clazz = Java_CFFI_Class(env, name)
    let args = ArrayQueue<Java_CFFI_JavaEntity>()
    var signStr: String = "(J"
    args.add((Java_CFFI_JavaEntity(JLONG, id)))
    if (withMarkerParam) {
        signStr += "Lcangjie/lang/internal/$$NativeConstructorMarker;"
        args.add(Java_CFFI_JavaEntityJobjectNull())
    }
    signStr += ")V"
    let signature = Java_CFFI_parseMethodSignature(signStr)
    let methodId = Java_CFFI_MethodID.constr(env, clazz, "<init>", signature)
    let result = Java_CFFI_newJavaObject(env, clazz, methodId, args.toArray(), Java_CFFI_JavaCallNest(args.size))
    unsafe { result.asJObject() }
}

public func Java_CFFI_newGlobalReference(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, weak: Bool): Java_CFFI_JavaEntity {
    unsafe {
        let envCJ = env.read().read()
        if (weak) {
            Java_CFFI_JavaEntity(JWEAK, envCJ.NewWeakGlobalRef(env, obj.asJObject()))
        } else {
            Java_CFFI_JavaEntity(JSTRONG, envCJ.NewGlobalRef(env, obj.asJObject()))
        }
    }
}

public func Java_CFFI_newJavaArray(env: JNIEnv_ptr, elementType: String, size: jsize): Java_CFFI_JavaEntity {
    let elementTypeSignature = Java_CFFI_parseComponentSignature(elementType)
    Java_CFFI_newJavaArray(env, elementTypeSignature, size)
}

public func Java_CFFI_newJavaArray(env: JNIEnv_ptr, elementType: Java_CFFI_ComponentSignature, size: jsize): Java_CFFI_JavaEntity {
    let res = unsafe {
        let envCJ = env.read().read()
        match (elementType) {
            case PRIMITIVE(base) => match (base) {
                case BYTE => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewByteArray(env, size))
                case CHAR => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewCharArray(env, size))
                case DOUBLE => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewDoubleArray(env, size))
                case FLOAT => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewFloatArray(env, size))
                case INT => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewIntArray(env, size))
                case LONG => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewLongArray(env, size))
                case SHORT => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewShortArray(env, size))
                case BOOLEAN => Java_CFFI_JavaEntity(JOBJECT, envCJ.NewBooleanArray(env, size))
                case VOID => throw Java_CFFI_InvariantFailed()
            }
            case OBJECT(object) =>
                Java_CFFI_JavaEntity(JOBJECT, envCJ.NewObjectArray(env, size, Java_CFFI_Class(env, object.forJNI()).obj, jobject()))
        }
    }
    handlePendingException(env)
    res
}

public func Java_CFFI_arrayGetLength(env: JNIEnv_ptr, array: Java_CFFI_JavaEntity): jsize {
    unsafe {
        let envCJ = env.read().read()
        envCJ.GetArrayLength(env, array.asJObject())
    }
}

public func Java_CFFI_deleteGlobalReference(env: JNIEnv_ptr, ref: Java_CFFI_JavaEntity): Unit {
    unsafe {
        let envCJ = env.read().read()
        match (ref.kind) {
            case JWEAK => envCJ.DeleteWeakGlobalRef(env, ref.asJWeak())
            case JSTRONG => envCJ.DeleteGlobalRef(env, ref.asJStrong())
            case _ => throw InteropLibException("Cannot delete not global reference entity")
        }
    }
}

public func Java_CFFI_arrayGet(env: JNIEnv_ptr, elementType: Java_CFFI_JavaEntityKind, array: Java_CFFI_JavaEntity, index: jint): Java_CFFI_JavaEntity {
    unsafe {
        let envCJ = env.read().read()
        let res = match (elementType) {
            case JBYTE =>
                var value: jbyte = 0
                envCJ.GetByteArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JCHAR =>
                var value: jchar = 0
                envCJ.GetCharArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JDOUBLE =>
                var value: jdouble = 0.0
                envCJ.GetDoubleArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JFLOAT =>
                var value: jfloat = 0.0
                envCJ.GetFloatArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JINT =>
                var value: jint = 0
                envCJ.GetIntArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JLONG =>
                var value: jlong = 0
                envCJ.GetLongArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JSHORT =>
                var value: jshort = 0
                envCJ.GetShortArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JBOOLEAN =>
                var value: jboolean = 0
                envCJ.GetBooleanArrayRegion(env, array.asJObject(), index, 1, inout value)
                Java_CFFI_JavaEntity(elementType, value)
            case JOBJECT =>
                Java_CFFI_JavaEntity(elementType, envCJ.GetObjectArrayElement(env, array.asJObject(), index))
            case _ => throw Java_CFFI_InvariantFailed()
        }
        handlePendingException(env)
        res
    }
}

private let cstrSelf = unsafe { LibC.mallocCString("self") }
private let cstrJ = unsafe { LibC.mallocCString("J") }

public func Java_CFFI_putToRegistry(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, a: Any): Unit {
    let id = registry.put(a)
    unsafe {
        let envCJ : JNINativeInterface_ = env.read().read()
        let objClass = envCJ.GetObjectClass(env, obj.asJObject())
        let fieldId = envCJ.GetFieldID(env, objClass, cstrSelf, cstrJ)
        envCJ.SetLongField(env, obj.asJObject(), fieldId, id)
    }
}

public func Java_CFFI_arraySet(env: JNIEnv_ptr, elementType: Java_CFFI_JavaEntityKind, array: Java_CFFI_JavaEntity, index: jint, valueEntity: Java_CFFI_JavaEntity): Unit {
    unsafe {
        let envCJ = env.read().read()
        let res = match (elementType) {
            case JBYTE =>
                var value: jbyte = valueEntity.asJByte()
                envCJ.SetByteArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JCHAR =>
                var value: jchar = valueEntity.asJChar()
                envCJ.SetCharArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JDOUBLE =>
                var value: jdouble = valueEntity.asJDouble()
                envCJ.SetDoubleArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JFLOAT =>
                var value: jfloat = valueEntity.asJFloat()
                envCJ.SetFloatArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JINT =>
                var value: jint = valueEntity.asJInt()
                envCJ.SetIntArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JLONG =>
                var value: jlong = valueEntity.asJLong()
                envCJ.SetLongArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JSHORT =>
                var value: jshort = valueEntity.asJShort()
                envCJ.SetShortArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JBOOLEAN =>
                var value: jboolean = valueEntity.asJBoolean()
                envCJ.SetBooleanArrayRegion(env, array.asJObject(), index, 1, inout value)
            case JOBJECT =>
                envCJ.SetObjectArrayElement(env, array.asJObject(), index, valueEntity.asJObject())
            case _ => throw Java_CFFI_InvariantFailed()
        }
        handlePendingException(env)
        res
    }
}

///////////////////////////////////////////////////////////////////////////////
//// Region OLD

private func to_jstring(env: JNIEnv_ptr, str: String): jstring {
    var result = jstring()
    unsafe {
        let envCJ : JNINativeInterface_ = env.read().read()
        try (cstr = LibC.mallocCString(str).asResource()) {
            result = envCJ.NewStringUTF(env, cstr.value)
        }
    }
    result
}

public func Java_CFFI_get_env(): JNIEnv_ptr {
    var envPtr = JNIEnv_ptr()

    unsafe {
        let javaVM: JNIInvokeInterface_ = jvm.read().read()
        /*
            Thread should be attached as daemon to prevent waiting for Cangjie threads.
            Because Cangjie runtime threads are registered (for example, during objects finalization),
            JVM is blocked from terminating.

            A better solution is to detach Cangjie thread from JVM.
        */
        javaVM.AttachCurrentThreadAsDaemon(jvm, inout envPtr, CPointer<Unit>())
    }

    envPtr
}

public func Java_CFFI_JavaEntityJobject(jobject: CPointer<Unit>): Java_CFFI_JavaEntity {
    Java_CFFI_JavaEntity(JOBJECT, jobject)
}

public func Java_CFFI_JavaEntityJobjectNull(): Java_CFFI_JavaEntity {
    Java_CFFI_JavaEntity(JOBJECT, CPointer<Unit>())
}

public func Java_CFFI_put_to_registry_1(any: Any): Int64 {
    registry.put(any)
}

public func Java_CFFI_getFromRegistryByObjOption<T>(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity): ?T {
    unsafe {
        let envCJ : JNINativeInterface_ = env.read().read()
        let jobj = obj.asJObject()
        if (jobj.isNull()) {
            return None
        }
 
        let objClass = envCJ.GetObjectClass(env, jobj)
        let fieldId = envCJ.GetFieldID(env, objClass, cstrSelf, cstrJ)
        if (fieldId.isNull()) {
            return None
        }
 
        let id = envCJ.GetLongField(env, obj.asJObject(), fieldId)
        Java_CFFI_getFromRegistryOption<T>(id)
    }
}

public func Java_CFFI_getFromRegistryByObj<T>(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity): T {
    return Java_CFFI_getFromRegistryByObjOption<T>(env, obj) ?? throw InteropLibException("CJ object does not exist")
}

public func Java_CFFI_getFromRegistry<T>(env: JNIEnv_ptr, self: jlong): T {
    if (let Some(v) <- registry.get<T>(self)) {
        v
    } else {
        throw InteropLibException("CJ object ${self} does not exist")
    }
}

public func Java_CFFI_getFromRegistryOption<T>(self: jlong): Option<T> {
    registry.get<T>(self)
}

public func Java_CFFI_removeFromRegistry(self: jlong): Unit {
    registry.remove(self)
}

public func Java_CFFI_deleteCJObject<T>(env: JNIEnv_ptr, self: jlong, getEntity: (T) -> Java_CFFI_JavaEntity): Unit {
    let v = Java_CFFI_getFromRegistry<T>(env, self)
    Java_CFFI_deleteGlobalReference(env, getEntity(v))
    registry.remove(self)
}

public func Java_CFFI_deleteCJObjectOneWay<T>(env: JNIEnv_ptr, self: jlong): Unit {
    let v = Java_CFFI_getFromRegistry<T>(env, self)
    registry.remove(self)
}

public func Java_CFFI_unwrapJavaEntityAsValue<T>(entity: Java_CFFI_JavaEntity): T {
    unsafe {
        match (entity.kind) {
            case JVOID where entity.asUnit() is T => (entity.asUnit() as T).getOrThrow()
            case JVALUE | JOBJECT | JWEAK | JSTRONG where entity.asJObject() is T => (entity.asJObject() as T).getOrThrow()
            case JVALUE | JLONG where entity.asJLong() is T => (entity.asJLong() as T).getOrThrow()
            case JVALUE | JINT where entity.asJInt() is T => (entity.asJInt() as T).getOrThrow()
            case JVALUE | JSTRING where entity.asJString() is T => (entity.asJString() as T).getOrThrow()
            case JVALUE | JBOOLEAN where entity.asJBoolean().asBool() is T => (entity.asJBoolean().asBool() as T).getOrThrow()
            case JVALUE | JSHORT where entity.asJShort() is T => (entity.asJShort() as T).getOrThrow()
            case JVALUE | JCHAR where entity.asJChar() is T => (entity.asJChar() as T).getOrThrow()
            case JVALUE | JBYTE where entity.asJByte() is T => (entity.asJByte() as T).getOrThrow()
            case JVALUE | JFLOAT where entity.asJFloat() is T => (entity.asJFloat() as T).getOrThrow()
            case JVALUE | JDOUBLE where entity.asJDouble() is T => (entity.asJDouble() as T).getOrThrow()
            case _ => throw InteropLibException("Cannot get value of expected type from ${entity.kind.toString()}")
        }
    }
}

public func Java_CFFI_ensure_not_null(entity: Java_CFFI_JavaEntity): Java_CFFI_JavaEntity {
    match (entity) {
        case e where e.isNull => throw NoneValueException("Unexpected `null` has come from java")
        case e => e
    }
}

public func Java_CFFI_getJavaEntityOrNull(entity: Java_CFFI_JavaEntity): ?Java_CFFI_JavaEntity {
    match (entity) {
        case e where e.isNull => None
        case e => e
    }
}

public func Java_CFFI_isInstanceOf(env: JNIEnv_ptr, obj: Java_CFFI_JavaEntity, className: String): Bool {
    let clazz = Java_CFFI_Class(env, className)
    unsafe {
        withLocalRef(env, obj.asJObject()) { localRef =>
            let envCJ = env.read().read()
            envCJ.IsInstanceOf(env, localRef, clazz.obj).asBool()
        }
    }
}

extend jboolean {
    func asBool() {
        this != 0
    }
}

private func Java_CFFI_create_java_exception_object(env: JNIEnv_ptr, msg: String): jobject {
    unsafe {
        let msgObj = [Java_CFFI_CangjieStringToJava(env, msg)]
        let clazz = Java_CFFI_Class(env, "java/lang/Exception")
        let signature = Java_CFFI_parseMethodSignature("(Ljava/lang/String;)V")
        let id = Java_CFFI_MethodID.constr(env, clazz, "<init>", signature)
        let callNest = Java_CFFI_JavaCallNest(msgObj.size)
        let throwObj = Java_CFFI_newJavaObject(env, clazz, id, msgObj, callNest)
        throwObj.asJObject()
    }
}

/*
 * The function is called to process jump from CJ world backward to Java.
 * It does not throw JNIException because it is called in handler of JNIException.
 */
private func Java_CFFI_throw_exception(env: JNIEnv_ptr, exception: jthrowable) {
    unsafe {
        let envCJ : JNINativeInterface_ = env.read().read()
        envCJ.Throw(env, exception)
    }
}

private func handlePendingException(env: JNIEnv_ptr) {
    unsafe {
        let envCJ : JNINativeInterface_ = env.read().read()
        let pendingException = envCJ.ExceptionOccurred(env)
        if (!pendingException.isNull()) {
            envCJ.ExceptionClear(env)

            let clazz = Java_CFFI_Class(env, "java/lang/Throwable")
            let signature = Java_CFFI_parseMethodSignature("()Ljava/lang/String;")
            let id = Java_CFFI_MethodID.constr(env, clazz, "toString", signature)
            let jMessage = Java_CFFI_callVirtualMethod_no_exception_handling(env, Java_CFFI_JavaEntity(JOBJECT, pendingException),
                id, [], Java_CFFI_JavaCallNest())

            let getMessageException = envCJ.ExceptionOccurred(env)
            var cjMessage: String
            if (getMessageException.isNull()) {
                cjMessage = Java_CFFI_JavaStringToCangjie(env, jMessage)
            } else {
                cjMessage = "<Java Throwable.getMessage() has thrown an exception>"
                envCJ.ExceptionClear(env)
            }

            throw JNIException(cjMessage, pendingException)
        }
    }
}

private func Java_CFFI_wrap_cj_exception(env: JNIEnv_ptr, ex: Exception): JNIException {
    let throwObj = Java_CFFI_create_java_exception_object(env, ex.message)
    JNIException(ex.message, throwObj)
}

public func withExceptionHandling<R>(env: JNIEnv_ptr, action: () -> R): R where R <: CType {
    try {
        action()
    } catch (ex: JNIException) {
        Java_CFFI_throw_exception(env, ex.jException)
        unsafe { zeroValue<R>() }
    } catch (ex: Exception) { // TODO: handle Cangjie Error type?
        let wrapped = Java_CFFI_wrap_cj_exception(env, ex)
        Java_CFFI_throw_exception(env, wrapped.jException)
        unsafe { zeroValue<R>() }
    }
}

public func Java_CFFI_MethodIDConstr(env: JNIEnv_ptr, clazz: Java_CFFI_Class, methodName: String, signature: Java_CFFI_MethodSignature): Java_CFFI_MethodID {
    Java_CFFI_MethodID.constr(env, clazz, methodName, signature)
}

public func Java_CFFI_MethodIDConstrStatic(env: JNIEnv_ptr, clazz: Java_CFFI_Class, methodName: String, signature: Java_CFFI_MethodSignature): Java_CFFI_MethodID {
    Java_CFFI_MethodID.constrStatic(env, clazz, methodName, signature)
}

public func Java_CFFI_FieldIDConstr(env: JNIEnv_ptr, clazz: Java_CFFI_Class, fieldName: String, signature: Java_CFFI_ComponentSignature): Java_CFFI_FieldID {
    Java_CFFI_FieldID.constr(env, clazz, fieldName, signature)
}

public func Java_CFFI_FieldIDConstrStatic(env: JNIEnv_ptr, clazz: Java_CFFI_Class, fieldName: String, signature: Java_CFFI_ComponentSignature): Java_CFFI_FieldID {
    Java_CFFI_FieldID.constrStatic(env, clazz, fieldName, signature)
}

public func Java_CFFI_ClassInit(env: JNIEnv_ptr, name: String) : Java_CFFI_Class {
    Java_CFFI_Class(env, name)
}

public func Java_CFFI_JavaCallNestInit(n: Int64) : Java_CFFI_JavaCallNest {
    Java_CFFI_JavaCallNest(n)
}
