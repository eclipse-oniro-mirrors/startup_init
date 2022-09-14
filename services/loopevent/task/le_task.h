/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef LOOP_TASK_H
#define LOOP_TASK_H
#include <stdlib.h>

#include "init_hashmap.h"
#include "le_utils.h"
#include "list.h"
#include "loop_event.h"

#ifndef LOOP_EVENT_USE_MUTEX
#define LoopMutexInit(x) (void)(x)
#define LoopMutexLock(x) (void)(x)
#define LoopMutexUnlock(x) (void)(x)
#else
#include <pthread.h>
#define LoopMutex pthread_mutex_t
#define LoopMutexInit(x) pthread_mutex_init(x, NULL)
#define LoopMutexLock(x) pthread_mutex_lock(x)
#define LoopMutexUnlock(x) pthread_mutex_unlock(x)
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct {
    ListNode node;
    uint32_t buffSize;
    uint32_t dataSize;
    int32_t result;
    uint8_t data[0];
} LE_Buffer;

#define TASK_FLAGS_INVALID 0x80000000
typedef LE_STATUS (*HandleTaskEvent)(const LoopHandle loop, const TaskHandle task, uint32_t oper);
typedef void (*HandleTaskClose)(const LoopHandle loop, const TaskHandle task);
#define TASKINFO \
    uint32_t flags; \
    union { \
        int fd; \
    } taskId

typedef struct {
    TASKINFO;
} TaskId;

typedef struct LiteTask_ {
    TASKINFO;
    HashNode hashNode;
    LE_Close close;
    HandleTaskEvent handleEvent;
    HandleTaskClose innerClose;
    uint16_t userDataOffset;
    uint16_t userDataSize;
} BaseTask;

typedef struct {
    BaseTask base;
    LE_IncommingConnect incommingConnect;
    char server[0];
} StreamServerTask;

typedef struct {
    BaseTask base;
#ifdef LOOP_EVENT_USE_MUTEX
    LoopMutex mutex;
#else
    char mutex;
#endif
    ListHead buffHead;
} StreamTask;

typedef struct {
    StreamTask stream;
    StreamServerTask *serverTask;
    LE_SendMessageComplete sendMessageComplete;
    LE_RecvMessage recvMessage;
    LE_DisConnectComplete disConnectComplete;
} StreamConnectTask;

typedef struct {
    StreamTask stream;
    LE_DisConnectComplete disConnectComplete;
    LE_ConnectComplete connectComplete;
    LE_SendMessageComplete sendMessageComplete;
    LE_RecvMessage recvMessage;
    uint32_t connected : 1;
    char server[0];
} StreamClientTask;

typedef struct {
    StreamTask stream;
    LE_ProcessAsyncEvent processAsyncEvent;
} AsyncEventTask;

typedef struct {
    BaseTask base;
    uint32_t events;
    ProcessWatchEvent processEvent;
} WatcherTask;

LE_Buffer *CreateBuffer(uint32_t bufferSize);
LE_Buffer *GetNextBuffer(StreamTask *task, const LE_Buffer *next);
LE_Buffer *GetFirstBuffer(StreamTask *task);
int IsBufferEmpty(StreamTask *task);

void FreeBuffer(const LoopHandle loop, StreamTask *task, LE_Buffer *buffer);
void AddBuffer(StreamTask *task, LE_Buffer *buffer);

BaseTask *CreateTask(const LoopHandle loopHandle, int fd, const LE_BaseInfo *info, uint32_t size);
void CloseTask(const LoopHandle loopHandle, BaseTask *task);
int GetSocketFd(const TaskHandle task);
int CheckTaskFlags(const BaseTask *task, uint32_t flags);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
