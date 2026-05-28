// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <stddef.h>
#include <jni.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__ANDROID__)
#include <signal.h>
#endif
#include <ucontext.h>
#include <stdbool.h>
#include <ctype.h>

struct HeapParam {
    /*
     * The reference value of region size, measured in KB, default to 64 KB, must be in range [4KB, 64KB].
     * It will be set to default value if assigned with 0.
     */
    size_t regionSize;

    /*
     * The maximum size of cangjie heap, measured in KB, default to 256 * 1024 KB, must >= 4MB.
     * It will be set to default value if assigned with 0.
     */
    size_t heapSize;

    /*
     * Threshold used to determine whether a region is exempted (i.e., will not be forwarded).
     * If the percentage of live objects in a region is greater than this value, this region will not be exempted.
     * Default to 0.8, must be in range (0, 1].
     * It will be set to default value if assigned with 0.
     */
    double exemptionThreshold;

    /*
     * A hint to guide collector to release physical memory to OS.
     * heap utilization = heap-used-memory / total-heap-memory.
     * During each gc, collector determines how much memory should be cached,
     * and let the heap utilization be close to this value.
     * Default to 0.80, must be in range (0, 1].
     * It will be set to default value if assigned with 0.
     */
    double heapUtilization;

    /*
     * The ratio to expand heap after each GC.
     * GC is probably triggered more often if this value is set to an improperly small number.
     * Default to 0.15, must > 0.
     * It will be set to default value if assigned with 0.
     */
    double heapGrowth;

    /*
     * The rate of allocating memory from heap.
     * this value is the lower bound of the real allocation rate.
     * allocator maybe wait some time if this value is set with an improperly small number.
     * Mesured in MB/s, default to 10240 MB/s, must be > 0 MB/s.
     * It will be set to default value if assigned with 0.
     */
    double allocationRate;

    /*
     * The maximum wait time when allocating memory from heap.
     * The latter alloction will wait a number of time if the two alloction interval is less than the wait time.
     * The real wait time is the minimum of allocationWaitTime and the wait time calculated from real alloction rate.
     * Measured in ns, default to 1000 ns, must > 0 ns.
     * It will be set to default value if assigned with 0.
     */
    size_t allocationWaitTime;
};

/*
 * @struct GCParam
 * @brief Data structure for Cangjie garbage collection configuration parameters,\n
 * including the garbage ratio, garbage collection interval and etc.
 */
struct GCParam {
    /*
     * GC will be triggered when heap allocated size is greater than this threshold.
     * Measured in KB, must be > 0.
     */
    size_t gcThreshold;

    /*
     * The threshold used to determine whether to collect from-space during GC.
     * The from-space will be collected if the percentage of the garbage in from space is greater than this threshold.
     * default to 0.5, must be in range [0.1, 1.0].
     */
    double garbageThreshold;

    /*
     * Minimum interval each GC request will be responded. If two adjacent GC requests with
     * interval less than this value, the latter one is ignored.
     * Measured in ns, default to 150 ms, must be > 0 ms.
     * It will be set default value if the value is 0.
     */
    uint64_t gcInterval;

    /*
     * Minimum interval each backup GC request will be responded.
     * Backup GC will be triggered if there is no GC during this interval.
     * Measured in ns, default to 240 s, must be > 0 s.
     * It will be set default value if the value is 0.
     */
    uint64_t backupGCInterval;

    /*
     * Parameters for adjusting the number of GC threads.
     * The number of gc threads is ((the hardware concurrency / this value) - 1).
     * default to 8, must be > 0.
     * It will be set default value if the value is 0.
     */
    int32_t gcThreads;
};

enum RTLogLevel {
    RTLOG_VERBOSE,
    RTLOG_DEBUGY,
    RTLOG_INFO,
    RTLOG_WARNING,
    RTLOG_ERROR,
    RTLOG_FATAL_WITHOUT_ABORT,
    RTLOG_FATAL,
    RTLOG_OFF
};

/*
 * @struct LogParam
 * @brief Data structure for Cangjie Log configuration parameters,\n
 * including the log size, log file level and etc.
 */
struct LogParam {
    /* Logging statements with level less than this value are ignored. Default to ERROR. */
    enum RTLogLevel logLevel;
};

/*
 * @struct ConcurrencyParam
 * @brief Data structure for thread and cjthread configuration parameters,\n
 * including the default stack size for threads and cjthread, numbers of processors,\n
 * and the maximum number of cjthreads.
 */
struct ConcurrencyParam {
    /*
     * Thread stack size. Measured in KB, recommended value is 1 MB, must be > 0.
     * It will be set default value if the value is 0.
     */
    size_t thStackSize;

    /*
     * CJThread stack size. Measured in KB, recommended value is 64KB, must be in range [64KB, 1GB].
     * It will be set default value if the value is 0.
     */
    size_t coStackSize;

    /*
     * Number of processors, recommended value is consistent with the number of CPU cores, must be > 0.
     * It will be set default value if the value is 0.
     */
    uint32_t processorNum;
};

struct RuntimeParam {
    struct HeapParam heapParam;
    struct GCParam gcParam;
    struct LogParam logParam;
    struct ConcurrencyParam coParam;
};

static enum RTLogLevel InitLogLevel()
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
            return RTLOG_DEBUGY;
        case 'i':
            return RTLOG_INFO;
        case 'w':
            return RTLOG_WARNING;
        case 'e':
            return RTLOG_ERROR;
        case 'f':
            return RTLOG_FATAL;
        case 's':
            return RTLOG_FATAL_WITHOUT_ABORT;
        default:
            printf("Unsupported MRT_LOG_LEVEL value. Valid MRT_LOG_LEVEL must be in"
                   "['v', 'd', 'i', 'w', 'e', 'f', 's'].\n");
    }

    return RTLOG_ERROR;
}

extern int InitCJRuntime(const struct RuntimeParam* param);
extern int LoadCJLibraryWithInit(const char* libName);
extern void setJavaVmAndInitClassLoading(JavaVM *vm, JNIEnv *env, jobject classLoader, jmethodID methodID, jclass javaLangClass);

#if !defined(__ANDROID__)
struct SignalAction {
    union {
        void (*saHandler)(int);
        bool (*saSignalAction)(int, siginfo_t*, void*);
    };
    sigset_t scMask;
    uint64_t scFlags;
};

void AddHandlerToSignalStack(int signal, struct SignalAction* sa);

static void setEmptyDefaultSIGSEGVHandler() {
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

JNIEXPORT void JNICALL Java_cangjie_lang_LibraryLoader_nativeLoadCJLibrary(JNIEnv *env, jclass unused, jstring libName, jobject classLoader) {
    JavaVM *vm = NULL;
    jboolean isCopy = false;
    const char *cjLibraryName = (*env)->GetStringUTFChars(env, libName, &isCopy);
    // This string will be in Modified UTF-8, which is almost certainly not the filesystem encoding (for dlopen).
    // The filesystem encoding is very likely to be UTF-8 on Linux, this will cause problems with surrogate pairs, if present in the library name.
    // TODO: string encoding conversion from MUTF-8 to UTF-8, at least on Linux.
    LoadCJLibraryWithInit(cjLibraryName);
    (*env)->ReleaseStringUTFChars(env, libName, cjLibraryName);

    (*env)->GetJavaVM(env, &vm);

    if (javaLangClass == NULL) {
        jclass localRef = (*env)->FindClass(env, "java/lang/Class");
        // Since interop operations can happen even during process shutdown,
        // there is no appropriate moment in time to delete this JNI GlobalRef.
        javaLangClass = (jclass)(*env)->NewGlobalRef(env, localRef);
    }
    jmethodID methodID = (*env)->GetStaticMethodID(
        env, javaLangClass, "forName",
        "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
    setJavaVmAndInitClassLoading(vm, env, classLoader, methodID, javaLangClass);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    struct RuntimeParam param = {0};
    param.logParam.logLevel = InitLogLevel();
    InitCJRuntime(&param);
#if !defined(__ANDROID__)
    setEmptyDefaultSIGSEGVHandler();
#endif
    return JNI_VERSION_1_6;
}
