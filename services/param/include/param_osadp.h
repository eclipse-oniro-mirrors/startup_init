/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE_STARTUP_PARAM_OS_ADAPTER_H
#define BASE_STARTUP_PARAM_OS_ADAPTER_H
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
#include <sys/syscall.h>
#include "loop_event.h"
#else
#include <time.h>
#endif

#ifndef __LITEOS_M__
#include <pthread.h>
#endif
#include "param_utils.h"
#include "param_common.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef STATIC_INLINE
#ifdef STARTUP_INIT_TEST
#define STATIC_INLINE
#else
#define STATIC_INLINE static inline
#endif
#endif

#define PARAM_WORKSPACE_INVALID ((uint32_t)-1)
#define PARAM_WORKSPACE_MIN (1024)
/*
    length for parameter = node size + data size
    xxxx.xxxx.xxxx.xxxx
    node size:
    24 * (count(.) + 1) + strlen(xxxx.xxxx.xxxx.xxxx)
    data size
    strlen(xxxx.xxxx.xxxx.xxxx) + 96

    dac size
     24 * (count(.) + 1)  + sizeof(ParamSecurityNode)
*/
#define DAC_DEFAULT_GROUP 0
#define DAC_DEFAULT_USER 0

#ifdef STARTUP_INIT_TEST
#define DAC_DEFAULT_MODE 0777
#define PARAM_WORKSPACE_DEF (1024 * 80)
#define PARAM_WORKSPACE_MAX (1024 * 80)
#define PARAM_WORKSPACE_SMALL PARAM_WORKSPACE_DEF
#else

#ifdef __LITEOS_M__
#define DAC_DEFAULT_MODE 0777
#ifndef PARAM_WORKSPACE_MAX
#define PARAM_WORKSPACE_MAX (1024 * 5)
#endif
#define PARAM_WORKSPACE_SMALL PARAM_WORKSPACE_MAX
#define PARAM_WORKSPACE_DEF PARAM_WORKSPACE_MAX
#else // __LITEOS_M__

#ifdef __LITEOS_A__
#define DAC_DEFAULT_MODE 0777
#define PARAM_WORKSPACE_MAX (1024 * 10)
#define PARAM_WORKSPACE_SMALL PARAM_WORKSPACE_MAX
#define PARAM_WORKSPACE_DEF PARAM_WORKSPACE_MAX
#else // __LITEOS_A__
#define DAC_DEFAULT_MODE 0774
#ifdef PARAM_TEST_PERFORMANCE
#define PARAM_WORKSPACE_MAX (1024 * 1024 * 10)
#else
#define PARAM_WORKSPACE_MAX (80 * 1024)
#endif
#define PARAM_WORKSPACE_SMALL (1024 * 10)
#define PARAM_WORKSPACE_DEF (1024 * 30)
#define PARAM_WORKSPACE_DAC (1024 * 60)
#endif // __LITEOS_A__
#endif // __LITEOS_M__
#endif // STARTUP_INIT_TEST

#ifndef PARAM_WORKSPACE_DAC
#define PARAM_WORKSPACE_DAC PARAM_WORKSPACE_SMALL
#endif

// support timer
#if defined __LITEOS_A__ || defined __LITEOS_M__
struct ParamTimer_;
typedef void (*ProcessTimer)(const struct ParamTimer_ *taskHandle, void *context);
typedef struct ParamTimer_ {
    timer_t timerId;
    uint64_t repeat;
    ProcessTimer timeProcessor;
    void *context;
} ParamTimer;

typedef ParamTimer *ParamTaskPtr;
#else
typedef LoopBase *ParamTaskPtr;
typedef void (*ProcessTimer)(const ParamTaskPtr taskHandle, void *context);
#endif

int ParamTimerCreate(ParamTaskPtr *timer, ProcessTimer process, void *context);
int ParamTimerStart(const ParamTaskPtr timer, uint64_t timeout, uint64_t repeat);
void ParamTimerClose(ParamTaskPtr timer);

INIT_LOCAL_API void  paramMutexEnvInit(void);
INIT_LOCAL_API int ParamRWMutexCreate(ParamRWMutex *lock);
INIT_LOCAL_API int ParamRWMutexWRLock(ParamRWMutex *lock);
INIT_LOCAL_API int ParamRWMutexRDLock(ParamRWMutex *lock);
INIT_LOCAL_API int ParamRWMutexUnlock(ParamRWMutex *lock);
INIT_LOCAL_API int ParamRWMutexDelete(ParamRWMutex *lock);

INIT_LOCAL_API int ParamMutexCreate(ParamMutex *mutex);
INIT_LOCAL_API int ParamMutexPend(ParamMutex *mutex);
INIT_LOCAL_API int ParamMutexPost(ParamMutex *mutex);
INIT_LOCAL_API int ParamMutexDelete(ParamMutex *mutex);

#ifdef WORKSPACE_AREA_NEED_MUTEX
#define PARAMSPACE_AREA_INIT_LOCK(workspace) ParamRWMutexCreate(&workspace->rwlock)
#define PARAMSPACE_AREA_RW_LOCK(workspace) ParamRWMutexWRLock(&workspace->rwlock)
#define PARAMSPACE_AREA_RD_LOCK(workspace) ParamRWMutexRDLock(&workspace->rwlock)
#define PARAMSPACE_AREA_RW_UNLOCK(workspace) ParamRWMutexUnlock(&workspace->rwlock)
#else
#define PARAMSPACE_AREA_INIT_LOCK(rwlock) (void)(rwlock)
#define PARAMSPACE_AREA_RW_LOCK(rwlock) (void)(rwlock)
#define PARAMSPACE_AREA_RD_LOCK(rwlock) (void)(rwlock)
#define PARAMSPACE_AREA_RW_UNLOCK(rwlock) (void)(rwlock)
#endif

INIT_LOCAL_API void *GetSharedMem(const char *fileName, MemHandle *handle, uint32_t spaceSize, int readOnly);
INIT_LOCAL_API void FreeSharedMem(const MemHandle *handle, void *mem, uint32_t dataSize);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // BASE_STARTUP_PARAM_OS_ADAPTER_H