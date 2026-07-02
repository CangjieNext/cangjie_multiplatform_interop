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

#if defined(__ANDROID__)
#include <android/log.h>
#define LOG_TAG "Cangjie-Interop"
#endif

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

static JavaVM* jvm = NULL;
static jclass javaLangClass = NULL;
static jmethodID forNameMethodID = NULL;
static volatile jobject classLoaderRef = NULL;

JNIEXPORT void JNICALL Java_cangjie_lang_LibraryLoader_nativeLoadCJLibrary(
    JNIEnv* env, jclass unused, jstring libName, jobject classLoader)
{
    jobject oldClassLoaderRef = classLoaderRef;
    if ((*env)->IsSameObject(env, oldClassLoaderRef, classLoader) == JNI_FALSE) {
        classLoaderRef = (*env)->NewGlobalRef(env, classLoader);
        if (oldClassLoaderRef != NULL) {
            (*env)->DeleteGlobalRef(env, oldClassLoaderRef);
        }
    }

    jboolean isCopy = JNI_FALSE;
    const char* cjLibraryName = (*env)->GetStringUTFChars(env, libName, &isCopy);
    // This string will be in Modified UTF-8, which is almost certainly not the filesystem encoding (for dlopen).
    // The filesystem encoding is very likely to be UTF-8 on Linux, this will cause problems with surrogate pairs,
    // if present in the library name.
    // Consider string encoding conversion from MUTF-8 to UTF-8, at least on Linux.

    if (LoadCJLibraryWithInit(cjLibraryName)) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to load Cangjie library '%s' in LibraryLoader", cjLibraryName);
#else
        printf("Failed to load Cangjie library '%s' in LibraryLoader\n", cjLibraryName);
#endif
    }
        
    (*env)->ReleaseStringUTFChars(env, libName, cjLibraryName);
}

JNIEXPORT JNIEnv* GetJNIEnv(void)
{
    // Thread should be attached as daemon to prevent waiting for Cangjie threads.
    // Because Cangjie runtime threads are registered (for example, during objects finalization),
    // they may call back into JVM, and if the thread is not attached as daemon, the JVM may wait for them to finish,
    // which may cause deadlock if the thread is waiting for the JVM to do something.
    //
    // A better solution is to detach Cangjie thread from JVM.

    if (jvm == NULL) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "JavaVM instance is not set");
#else
        printf("JavaVM instance is not set\n");
#endif
        return NULL;
    }

    JNIEnv *env = NULL;
#if defined(__ANDROID__)
    if ((*jvm)->AttachCurrentThreadAsDaemon(jvm, &env, NULL) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to attach current thread to JVM");
#else
    if ((*jvm)->AttachCurrentThreadAsDaemon(jvm, (void**)&env, NULL) != JNI_OK) {
        printf("Failed to attach current thread to JVM\n");
#endif
        return NULL;
    }

    return env;
}

JNIEXPORT jclass LoadJavaClass(JNIEnv* env, char* searchName)
{
    jstring searchNameStr = (*env)->NewStringUTF(env, searchName);
    jclass result = (*env)->CallStaticObjectMethod(env, javaLangClass, forNameMethodID, searchNameStr, JNI_FALSE, classLoaderRef);
    (*env)->DeleteLocalRef(env, searchNameStr);
    return result;
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
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to get JNIEnv in JNI_OnLoad");
#else
        printf("Failed to get JNIEnv in JNI_OnLoad\n");
#endif
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
