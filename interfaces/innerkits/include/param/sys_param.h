/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE_STARTUP_INIT_SYS_PARAM_H
#define BASE_STARTUP_INIT_SYS_PARAM_H

#include <stdint.h>
#ifndef __LITEOS_M__
#include <pthread.h>
#endif

#if (defined(PARAM_SUPPORT_STDATOMIC) || defined(__LITEOS_A__))
#include <stdatomic.h>
#endif

#ifndef __LITEOS_A__
#if defined FUTEX_WAIT || defined FUTEX_WAKE
#include <linux/futex.h>
#endif
#endif

#define MEMORY_ORDER_ACQUIRE 2

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef __LITEOS_M__
#define ATOMIC_UINT32 uint32_t
#define ATOMIC_LLONG  long long
#define ATOMIC_UINT64_LOAD_EXPLICIT(commitId, order) *(commitId)

#else
#if (defined(PARAM_SUPPORT_STDATOMIC) || defined(__LITEOS_A__))
#define ATOMIC_UINT32 atomic_uint
#define ATOMIC_LLONG atomic_llong
#define ATOMIC_UINT64_LOAD_EXPLICIT(commitId, order) atomic_load_explicit((commitId), order)

#else
#ifndef STARTUP_INIT_TEST
#define ATOMIC_UINT32 uint32_t
#define ATOMIC_LLONG int64_t
static inline ATOMIC_LLONG param_atomic_uint64_load(ATOMIC_LLONG *ptr, int order)
{
    return *((volatile ATOMIC_LLONG *)ptr);
}
#define ATOMIC_UINT64_LOAD_EXPLICIT(commitId, order) param_atomic_uint64_load((commitId), order)
#endif
#endif
#endif

#ifdef __LITEOS_M__
typedef struct {
    uint32_t mutex;
} ParamRWMutex;

typedef struct {
    uint32_t mutex;
} ParamMutex;
#endif

// support mutex
#ifndef STARTUP_INIT_TEST
typedef struct {
    pthread_rwlock_t rwlock;
} ParamRWMutex;

typedef struct {
    pthread_mutex_t mutex;
} ParamMutex;
#endif

#ifndef STARTUP_INIT_TEST
typedef struct {
    int shmid;
} MemHandle;

typedef struct {
    ATOMIC_LLONG commitId;
    ATOMIC_LLONG commitPersistId;
    uint32_t trieNodeCount;
    uint32_t paramNodeCount;
    uint32_t securityNodeCount;
    uint32_t currOffset;
    uint32_t spaceSizeOffset;
    uint32_t firstNode;
    uint32_t dataSize;
    char data[0];
} ParamTrieHeader;

typedef struct WorkSpace_ {
    unsigned int flags;
    MemHandle memHandle;
    ParamTrieHeader *area;
    ATOMIC_UINT32 rwSpaceLock;
    uint32_t spaceSize;
    uint32_t spaceIndex;
    ParamRWMutex rwlock;
    char fileName[0];
} WorkSpace;

typedef struct CachedParameter_ {
    struct WorkSpace_ *workspace;
    const char *(*cachedParameterCheck)(struct CachedParameter_ *param, int *changed);
    long long spaceCommitId;
    uint32_t dataCommitId;
    uint32_t dataIndex;
    uint32_t bufferLen;
    uint32_t nameLen;
    char *paramValue;
    char data[0];
} CachedParameter;

typedef void *CachedHandle;
#endif
/**
 * parameter client init
 */
void InitParameterClient(void);

/**
 * by name and default value，save parameter info in handle。
 *
 */
CachedHandle CachedParameterCreate(const char *name, const char *defValue);

/**
 * destroy handle
 *
 */
void CachedParameterDestroy(CachedHandle handle);

/**
 * if name exist，return value else return default value
 *
 */
static inline const char *CachedParameterGet(CachedHandle handle)
{
    struct CachedParameter_ *param = (struct CachedParameter_ *)handle;
    if (param == NULL) {
        return NULL;
    }

    // no change, do not to find
    long long spaceCommitId = ATOMIC_UINT64_LOAD_EXPLICIT(&param->workspace->area->commitId, MEMORY_ORDER_ACQUIRE);
    if (param->spaceCommitId == spaceCommitId) {
        return param->paramValue;
    }
    param->spaceCommitId = spaceCommitId;
    int changed = 0;
    if (param->cachedParameterCheck == NULL) {
        return param->paramValue;
    }
    return param->cachedParameterCheck(param, &changed);
}

static inline const char *CachedParameterGetChanged(CachedHandle handle, int *changed)
{
    struct CachedParameter_ *param = (struct CachedParameter_ *)handle;
    if (param == NULL) {
        return NULL;
    }
    // no change, do not to find
    long long spaceCommitId = ATOMIC_UINT64_LOAD_EXPLICIT(&param->workspace->area->commitId, MEMORY_ORDER_ACQUIRE);
    if (param->spaceCommitId == spaceCommitId) {
        return param->paramValue;
    }
    param->spaceCommitId = spaceCommitId;
    if ((changed == NULL) || (param->cachedParameterCheck == NULL)) {
        return param->paramValue;
    }
    return param->cachedParameterCheck(param, changed);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif