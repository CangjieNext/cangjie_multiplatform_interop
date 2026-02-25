// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Cangjie runtime initialization based on Cangjie.h (copied from cangjie_runtime repository) is a temporary solution.
// Later it should be rewritten using C Invoke Cangjie API when it gets available in Cangjie SDK.
#import "Cangjie.h"

#import "pthread.h"
#import "stdio.h"
#import "cjinterop.h"

volatile bool runtime_inited = false;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// ObjC runtime functions used to calculate override-mask
typedef struct objc_method* Method;
extern IMP method_getImplementation(Method m);
extern Method class_getInstanceMethod(Class cls, SEL name);
extern id objc_getClass(const char * name);

static struct RuntimeParam defaultCJRuntimeParams = {0};

bool initCJRuntime(const char* cj_gluecode_lib_name) {
    if (!runtime_inited) {
        pthread_mutex_lock(&mutex);
        if (!runtime_inited) {
            defaultCJRuntimeParams.logParam.logLevel = RTLOG_ERROR;
            if (InitCJRuntime(&defaultCJRuntimeParams) != E_OK) {
                printf("ERROR: Failed to initialize Cangjie runtime\n");
                pthread_mutex_unlock(&mutex);
                return false;
            }
            runtime_inited = true;
        }
        pthread_mutex_unlock(&mutex);
    }

    if (LoadCJLibraryWithInit(cj_gluecode_lib_name)) {
        printf("ERROR: Failed to init cjworld library\n");
        return false;
    }

    return true;
}

uint64_t calcOverrideMask(Class baseCls, Class selfCls, SEL* methods, int len) {
    int max = len;
    if (max > 64) {
        max = 64;
    }

    uint64_t mask = 0;
    for (int i = 0; i < max; i++) {
        SEL m = methods[i];
        bool override = method_getImplementation(class_getInstanceMethod(baseCls, m)) != method_getImplementation(class_getInstanceMethod(selfCls, m));
        if (override) {
            mask |= (UINT64_C(1) << i);
        }
    }

    return mask;
}
