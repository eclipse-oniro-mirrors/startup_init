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

#ifndef BASE_STARTUP_PARAM_COMMON_H
#define BASE_STARTUP_PARAM_COMMON_H
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#ifndef __LITEOS_M__
#include <pthread.h>
#endif
#include "param_atomic.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

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

typedef struct {
    int shmid;
} MemHandle;

typedef struct {
    uint32_t left;
    uint32_t right;
    uint32_t child;
    uint32_t labelIndex;
    uint32_t dataIndex;
    uint16_t selinuxLabel;
    uint16_t length;
    char key[0];
} ParamTrieNode;

#define PARAM_FLAGS_MODIFY 0x80000000
#define PARAM_FLAGS_TRIGGED 0x40000000
#define PARAM_FLAGS_WAITED 0x20000000
#define PARAM_FLAGS_COMMITID 0x0000ffff

#define PARAM_TYPE_MASK   0x0f
#define PARAM_TYPE_STRING 0x00
#define PARAM_TYPE_INT    0x01
#define PARAM_TYPE_BOOL   0x02

typedef struct {
    ATOMIC_UINT32 commitId;
    uint8_t type;
    uint8_t keyLength;
    uint16_t valueLength;
    char data[0];
} ParamNode;

typedef struct {
    uid_t uid;
    gid_t gid;
    uint32_t selinuxIndex;
    uint16_t mode;
    uint8_t type;
    uint8_t length;
    uint32_t memberNum;
    uid_t members[0];
} ParamSecurityNode;

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

typedef struct {
    uint8_t updaterMode;
    void (*logFunc)(int logLevel, uint32_t domain, const char *tag, const char *fmt, va_list vargs);
    int (*setfilecon)(const char *name, const char *content);
    int (*getServiceGroupIdByPid)(pid_t pid, gid_t *gids, uint32_t gidSize);
} PARAM_WORKSPACE_OPS;

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

typedef struct _SpaceSize {
    uint32_t maxLabelIndex;
    uint32_t spaceSize[0];
} WorkSpaceSize;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_PARAM_COMMON_H