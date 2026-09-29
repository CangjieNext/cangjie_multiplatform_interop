// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__ANDROID__) && !defined(_WIN64)
#include <signal.h>
#endif
#include <ctype.h>
#include <jni.h>
#include "Cangjie.h"

static enum RTLogLevel InitLogLevel(void)
{
    const char* env = getenv("MRT_LOG_LEVEL");
    if (!env) {
        return RTLOG_ERROR;
    }

    while ((*env) != '\0' && isspace(*env)) {
        env++;
    }

    if (strlen(env) != 1) {
        printf("Unsupported MRT_LOG_LEVEL value: length must be 1."
               " Valid MRT_LOG_LEVEL must be in ['v', 'd', 'i', 'w', 'e', 'f', 's'].\n");
        return RTLOG_ERROR;
    }

    switch (env[0]) {
        case 'v':
            return RTLOG_VERBOSE;
        case 'd':
            return RTLOG_DEBUG;
        case 'i':
            return RTLOG_INFO;
        case 'w':
            return RTLOG_WARNING;
        case 'e':
            return RTLOG_ERROR;
        case 'f':
            return RTLOG_FATAL;
        case 's':
            return RTLOG_FAIL;
        default:
            printf("Unsupported MRT_LOG_LEVEL value. Valid MRT_LOG_LEVEL must be in"
                   "['v', 'd', 'i', 'w', 'e', 'f', 's'].\n");
    }

    return RTLOG_ERROR;
}

extern void setJavaVmAndInitClassLoading(
    JavaVM* vm, JNIEnv* env, jobject classLoader, jmethodID methodID, jclass javaLangClass);

#if !defined(__ANDROID__) && !defined(_WIN64)
static void setEmptyDefaultSIGSEGVHandler(void)
{
    sigset_t mask;
    sigemptyset(&mask);

    struct SignalAction sa;
    memset(&sa, 0, sizeof(struct SignalAction));
    // Undocumented behavior: NULL handler stops SignalStack iteration,
    // so `SignalManager::HandleUnexpectedSigsegv` is not called.
    // Unlike true-returning handler, it calls the previously-defined signal handler (JVM).
    sa.saSignalAction = NULL;
    // Undocumented behavior: signal mask of the current thread for the duration of handler call,
    // it's not relevant for NULL handler.
    sa.scMask = mask;
    // Undocumented behavior: only 1 flag is defined for this field (SIGNAL_STACK_ALLOW_NORETURN),
    // it's not relevant for NULL handler.
    sa.scFlags = 0;
    AddHandlerToSignalStack(SIGSEGV, &sa);
}
#endif

static jclass javaLangClass = NULL;
static jmethodID forNameMethodID = NULL;
static JavaVM* jvm = NULL;

JNIEXPORT void JNICALL Java_cangjie_lang_LibraryLoader_nativeLoadCJLibrary(
    JNIEnv* env, jclass unused, jbyteArray libNameBytes, jobject classLoader)
{
    jsize len = (*env)->GetArrayLength(env, libNameBytes);
    char* cjLibraryName = malloc((size_t)len + 1);
    (*env)->GetByteArrayRegion(env, libNameBytes, 0, len, (jbyte*)cjLibraryName);
    cjLibraryName[len] = 0;
    LoadCJLibraryWithInit(cjLibraryName);
    free(cjLibraryName);

    setJavaVmAndInitClassLoading(jvm, env, classLoader, forNameMethodID, javaLangClass);
}

jobject Java_JNI_newGlobalRef(JNIEnv* env, jobject ref)
{
    jobject globalRef = (*env)->NewGlobalRef(env, ref);
    return globalRef;
}

/**
 * Creates global java reference from local java reference `localRef`.
 * Returns fresh global reference.
 * `localRef` is cleaned up as local java reference.
 *
 * If `localRef` is `nullptr`, then `nullptr` is returned.
 */
jobject Java_JNI_swapLocalWithGlobalRef(JNIEnv* env, jobject localRef)
{
    // It is safe to pass nullptr to NewGlobalRef. It just returns nullptr.
    jobject globalRef = (*env)->NewGlobalRef(env, localRef);
    // It should be safe to call DeleteLocalRef on nullptr.
    (*env)->DeleteLocalRef(env, localRef);
    return globalRef;
}

/* wrapper around DeleteGlobalRef, for @FastNative */
void Java_JNI_deleteGlobalRef(JNIEnv* env, jobject ref)
{
    (*env)->DeleteGlobalRef(env, ref);
}

/* wrapper around AttachCurrentThreadAsDaemon, for @FastNative */
JNIEnv* Java_JNI_getEnv()
{
    JNIEnv* env;
    /*
    due to incompatibility between NDK and JNI standard, we have to
    do an explicit adaptation of this API
    in NDK, the second argument of AttachCurrentThreadAsDaemon
    has type JNIEnv**, while in "normal" JNI
    the argument type is void**
    */
#ifdef __ANDROID__
// non-JNI compatible NDK-specific type
typedef JNIEnv** ppEnvType;
#else
// standard JNI type
typedef void** ppEnvType;
#endif
    jint res = (*jvm)->AttachCurrentThreadAsDaemon(jvm, (ppEnvType) &env, NULL);
    if (res != 0) {
        return NULL;
    }
    return env;
}

/* wrapper around ExceptionOccured, for @FastNative */
jthrowable Java_JNI_exceptionOccured(JNIEnv* env)
{
    return (*env)->ExceptionOccurred(env);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    jvm = vm;
    struct RuntimeParam param = {0};
    param.logParam.logLevel = InitLogLevel();
    InitCJRuntime(&param);
#if !defined(__ANDROID__) && !defined(_WIN64)
    setEmptyDefaultSIGSEGVHandler();
#endif

    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass localRef = (*env)->FindClass(env, "java/lang/Class");
    // Since interop operations can happen even during process shutdown,
    // there is no appropriate moment in time to delete this JNI GlobalRef.
    javaLangClass = (jclass)(*env)->NewGlobalRef(env, localRef);

    forNameMethodID = (*env)->GetStaticMethodID(
        env, javaLangClass, "forName",
        "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");

    return JNI_VERSION_1_6;
}
