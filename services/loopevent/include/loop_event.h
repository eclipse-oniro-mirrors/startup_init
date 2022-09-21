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

#ifndef LOOP_EVENT_H
#define LOOP_EVENT_H
#include <stdint.h>
#include <stdlib.h>
#include <sys/signalfd.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

// 配置
#define LOOP_EVENT_USE_EPOLL 1
#define LOOP_DEFAULT_BUFFER (1024 * 5)
#define LOOP_MAX_BUFFER (1024 * 64)

#define LOOP_MAX_CLIENT 1024
#define LOOP_MAX_SOCKET 1024
#define DEFAULT_TIMEOUT 1000

typedef enum {
    Event_Read = (0x0001),
    Event_Write = (0x0002),
    Event_Error = (0x0004),
    Event_Free = (0x0008),
    Event_Timeout = (0x0010),
    Event_Signal = (0x0020),
} EventOper;

typedef enum {
    LE_SUCCESS = 0,
    LE_FAILURE = 10000,
    LE_INVALID_PARAM,
    LE_NO_MEMORY,
    LE_DIS_CONNECTED,
    LE_INVALID_TASK
} LE_STATUS;

typedef struct {
    uint32_t flags;
} LoopBase;

typedef LoopBase *LoopHandle;
typedef LoopBase *TaskHandle;
typedef LoopBase *TimerHandle;
typedef LoopBase *SignalHandle;
typedef LoopBase *WatcherHandle;
typedef void *BufferHandle;

LoopHandle LE_GetDefaultLoop(void);
LE_STATUS LE_CreateLoop(LoopHandle *loopHandle);
void LE_RunLoop(const LoopHandle loopHandle);
void LE_CloseLoop(const LoopHandle loopHandle);
void LE_StopLoop(const LoopHandle loopHandle);
void LE_CloseTask(const LoopHandle loopHandle, const TaskHandle taskHandle);

/**
 * 申请一个buffer，用于消息的发送和接收
 */
BufferHandle LE_CreateBuffer(const LoopHandle loopHandle, uint32_t bufferSize);
void LE_FreeBuffer(const LoopHandle loopHandle, const TaskHandle taskHandle, const BufferHandle handle);
uint8_t *LE_GetBufferInfo(const BufferHandle handle, uint32_t *dataSize, uint32_t *buffSize);
void *LE_GetUserData(const TaskHandle handle);
int32_t LE_GetSendResult(const BufferHandle handle);

typedef void (*LE_Close)(const TaskHandle taskHandle);
typedef struct {
    uint32_t flags;
    LE_Close close;
    uint16_t userDataSize;
} LE_BaseInfo;

/**
 * 数据流服务，可以指定使用pipe或者普通的tcp
 */
#define TASK_STREAM 0x01
#define TASK_TCP (0x01 << 8)
#define TASK_PIPE (0x02 << 8)
#define TASK_SERVER (0x01 << 16)
#define TASK_CONNECT (0x02 << 16)
#define TASK_TEST (0x01 << 24)
typedef void (*LE_DisConnectComplete)(const TaskHandle client);
typedef void (*LE_ConnectComplete)(const TaskHandle client);
typedef void (*LE_SendMessageComplete)(const TaskHandle taskHandle, BufferHandle handle);
typedef void (*LE_RecvMessage)(const TaskHandle taskHandle, const uint8_t *buffer, uint32_t buffLen);
typedef int (*LE_IncommingConnect)(const LoopHandle loopHandle, const TaskHandle serverTask);
typedef struct {
    LE_BaseInfo baseInfo;
    char *server;
    int socketId;
    LE_DisConnectComplete disConnectComplete;
    LE_IncommingConnect incommingConnect;
    LE_SendMessageComplete sendMessageComplete;
    LE_RecvMessage recvMessage;
} LE_StreamServerInfo;

typedef struct {
    LE_BaseInfo baseInfo;
    char *server;
    LE_DisConnectComplete disConnectComplete;
    LE_ConnectComplete connectComplete;
    LE_SendMessageComplete sendMessageComplete;
    LE_RecvMessage recvMessage;
} LE_StreamInfo;

LE_STATUS LE_CreateStreamServer(const LoopHandle loopHandle,
    TaskHandle *taskHandle, const LE_StreamServerInfo *info);
LE_STATUS LE_CreateStreamClient(const LoopHandle loopHandle,
    TaskHandle *taskHandle, const LE_StreamInfo *info);
LE_STATUS LE_AcceptStreamClient(const LoopHandle loopHandle,
    const TaskHandle serverTask, TaskHandle *taskHandle, const LE_StreamInfo *info);
LE_STATUS LE_Send(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const BufferHandle handle, uint32_t buffLen);
void LE_CloseStreamTask(const LoopHandle loopHandle, const TaskHandle taskHandle);
int LE_GetSocketFd(const TaskHandle taskHandle);

/**
 * 异步事件服务
 */
typedef void (*LE_ProcessAsyncEvent)(const TaskHandle taskHandle,
    uint64_t eventId, const uint8_t *buffer, uint32_t buffLen);
#define TASK_EVENT 0x02
#define TASK_ASYNC_EVENT (0x01 << 8)
LE_STATUS LE_CreateAsyncTask(const LoopHandle loopHandle,
    TaskHandle *taskHandle, LE_ProcessAsyncEvent processAsyncEvent);
LE_STATUS LE_StartAsyncEvent(const LoopHandle loopHandle,
    const TaskHandle taskHandle, uint64_t eventId, const uint8_t *data, uint32_t buffLen);
void LE_StopAsyncTask(const LoopHandle loopHandle, const TaskHandle taskHandle);
#ifdef STARTUP_INIT_TEST
void LE_DoAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle);
#endif

/**
 * 定时器处理
 */
#define TASK_TIME 0x04
typedef void (*LE_ProcessTimer)(const TimerHandle taskHandle, void *context);

LE_STATUS LE_CreateTimer(const LoopHandle loopHandle, TimerHandle *timer,
    LE_ProcessTimer processTimer, void *context);
LE_STATUS LE_StartTimer(const LoopHandle loopHandle,
    const TimerHandle timer, uint64_t timeout, uint64_t repeat);
void LE_StopTimer(const LoopHandle loopHandle, const TimerHandle timer);

#define TASK_SIGNAL 0x08
typedef void (*LE_ProcessSignal)(const struct signalfd_siginfo *siginfo);
LE_STATUS LE_CreateSignalTask(const LoopHandle loopHandle,
    SignalHandle *signalHandle, LE_ProcessSignal processSignal);
LE_STATUS LE_AddSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal);
LE_STATUS LE_RemoveSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal);
void LE_CloseSignalTask(const LoopHandle loopHandle, const SignalHandle signalHandle);

/**
 * 监控句柄变化
 */
#define TASK_WATCHER 0x10
#define WATCHER_ONCE  0x0100
typedef void (*ProcessWatchEvent)(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context);
typedef struct {
    int fd;
    uint32_t flags;
    uint32_t events;
    LE_Close close;
    ProcessWatchEvent processEvent;
} LE_WatchInfo;
LE_STATUS LE_StartWatcher(const LoopHandle loopHandle,
    WatcherHandle *watcherHandle, const LE_WatchInfo *info, const void *context);
void LE_RemoveWatcher(const LoopHandle loopHandle, const WatcherHandle watcherHandle);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif