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
#include <stdatomic.h>
#endif

#if defined FUTEX_WAIT || defined FUTEX_WAKE
#include <linux/futex.h>
#endif

#include "param_utils.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
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
*/
#if (defined __LITEOS_A__ || defined __LITEOS_M__)
#define DAC_DEFAULT_MODE 0777
#ifdef STARTUP_INIT_TEST
#define PARAM_WORKSPACE_MAX (1024 * 50)
#else
#ifndef PARAM_WORKSPACE_MAX
#define PARAM_WORKSPACE_MAX (1024 * 5)
#endif
#endif
#define PARAM_WORKSPACE_SMALL PARAM_WORKSPACE_MAX
#define PARAM_WORKSPACE_DEF PARAM_WORKSPACE_MAX
#define DAC_DEFAULT_GROUP 0
#define DAC_DEFAULT_USER 0
#else
#define PARAM_WORKSPACE_MAX (80 * 1024)
#define PARAM_WORKSPACE_SMALL (1024 * 20)
#ifdef STARTUP_INIT_TEST
#define DAC_DEFAULT_MODE 0777
#define PARAM_WORKSPACE_DEF (1024 * 50)
#else
#define DAC_DEFAULT_MODE 0774
#define PARAM_WORKSPACE_DEF (1024 * 30)
#endif
#define DAC_DEFAULT_GROUP 0
#define DAC_DEFAULT_USER 0
#endif

// support futex
#ifndef __NR_futex
#define PARAM_NR_FUTEX 202 /* syscall number */
#else
#define PARAM_NR_FUTEX __NR_futex
#endif

#if !(defined FUTEX_WAIT || defined FUTEX_WAKE)
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#ifndef __LITEOS_M__
#define PARAM_FUTEX(ftx, op, value, timeout, bitset)                         \
    do {                                                                   \
        struct timespec d_timeout = { 0, 1000 * 1000 * (timeout) };        \
        syscall(PARAM_NR_FUTEX, ftx, op, value, &d_timeout, NULL, bitset); \
    } while (0)

#define futex_wake(ftx, count) PARAM_FUTEX(ftx, FUTEX_WAKE, count, 0, 0)
#define futex_wait(ftx, value) PARAM_FUTEX(ftx, FUTEX_WAIT, value, 100, 0)
#else
#define futex_wake(ftx, count) (void)(ftx)
#define futex_wait(ftx, value) (void)(ftx)
#endif
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

// support mutex
#ifndef __LITEOS_M__
typedef struct {
    pthread_rwlock_t rwlock;
} ParamRWMutex;

typedef struct {
    pthread_mutex_t mutex;
} ParamMutex;
#else
typedef struct {
    uint32_t mutex;
} ParamRWMutex;

typedef struct {
    uint32_t mutex;
} ParamMutex;
#endif

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

#ifdef PARAMWORKSPACE_NEED_MUTEX
#define WORKSPACE_INIT_LOCK(workspace) ParamRWMutexCreate(&(workspace).rwlock)
#define WORKSPACE_RW_LOCK(workspace) ParamRWMutexWRLock(&(workspace).rwlock)
#define WORKSPACE_RD_LOCK(workspace) ParamRWMutexRDLock(&(workspace).rwlock)
#define WORKSPACE_RW_UNLOCK(workspace) ParamRWMutexUnlock(&(workspace).rwlock)
#else
#define WORKSPACE_INIT_LOCK(workspace) (void)(workspace)
#define WORKSPACE_RW_LOCK(workspace) (void)(workspace)
#define WORKSPACE_RD_LOCK(workspace) (void)(workspace)
#define WORKSPACE_RW_UNLOCK(workspace) (void)(workspace)
#endif

typedef struct {
    int shmid;
} MemHandle;
INIT_LOCAL_API void *GetSharedMem(const char *fileName, MemHandle *handle, uint32_t spaceSize, int readOnly);
INIT_LOCAL_API void FreeSharedMem(const MemHandle *handle, void *mem, uint32_t dataSize);

// for atomic
#ifdef __LITEOS_M__
#define ATOMIC_UINT32 uint32_t
#define ATOMIC_LLONG  long long
#define ATOMIC_INIT(commitId, value) *(commitId) = (value)
#define ATOMIC_LOAD_EXPLICIT(commitId, order) *(commitId)
#define ATOMIC_STORE_EXPLICIT(commitId, value, order) *(commitId) = (value)
#else
#define ATOMIC_UINT32 atomic_uint
#define ATOMIC_LLONG atomic_llong
#define ATOMIC_INIT(commitId, value) atomic_init((commitId), (value))
#define ATOMIC_LOAD_EXPLICIT(commitId, order) atomic_load_explicit((commitId), (order))
#define ATOMIC_STORE_EXPLICIT(commitId, value, order) atomic_store_explicit((commitId), (value), (order))
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

INIT_LOCAL_API uint32_t Difftime(time_t curr, time_t base);
#endif // BASE_STARTUP_PARAM_MESSAGE_H