/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "loopevent.h"
#include "securec.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

LE_STATUS LE_AddIdle(const LoopHandle loopHandle, IdleHandle *idle,
    LE_ProcessIdle processIdle, void *context, int repeat)
{
    return 0;
}

void LE_DelIdle(IdleHandle idle)
{
    printf("test LE_DelIdle");
}

int IdleListTraversalProc(ListNode *node, void *data)
{
    return 0;
}

void LE_RunIdle(const LoopHandle loopHandle)
{
    printf("test LE_RunIdle");
}

int IsValid(const EventEpoll *loop)
{
    return 0;
}

void GetEpollEvent(int fd, int op, struct epoll_event *event)
{
    printf("test GetEpollEvent_");
}

LE_STATUS Close(const EventLoop *loop)
{
    return 0;
}

LE_STATUS AddEvent(const EventLoop *loop, const BaseTask *task, int op)
{
    return 0;
}

LE_STATUS ModEvent(const EventLoop *loop, const BaseTask *task, int op)
{
    return 0;
}

LE_STATUS DelEvent(const EventLoop *loop, int fd, int op)
{
    return 0;
}

LE_STATUS RunLoop(const EventLoop *loop)
{
    return 0;
}

LE_STATUS CreateEpollLoop(EventLoop **loop, uint32_t maxevents, uint32_t timeout)
{
    return 0;
}

int TaskNodeCompare(const HashNode *node1, const HashNode *node2)
{
    return 0;
}

int TaskKeyCompare(const HashNode *node, const void *key)
{
    return 0;
}

int TaskGetNodeHasCode(const HashNode *node)
{
    return 0;
}

int TaskGetKeyHasCode(const void *key)
{
    return 0;
}

void TaskNodeFree(const HashNode *node, void *context)
{
    printf("test TaskNodeFree");
}

LE_STATUS CreateLoop(EventLoop **loop, uint32_t maxevents, uint32_t timeout)
{
    return 0;
}

LE_STATUS CloseLoop(EventLoop *loop)
{
    return 0;
}

LE_STATUS ProcessEvent(const EventLoop *loop, int fd, uint32_t oper)
{
    return 0;
}

LE_STATUS AddTask(EventLoop *loop, BaseTask *task)
{
    return 0;
}

BaseTask *GetTaskByFd(EventLoop *loop, int fd)
{
    return NULL;
}

void DelTask(EventLoop *loop, BaseTask *task)
{
    printf("test DelTask");
}

LoopHandle LE_GetDefaultLoop(void)
{
    return 0;
}

LE_STATUS LE_CreateLoop(LoopHandle *handle)
{
    return 0;
}

void LE_RunLoop(const LoopHandle handle)
{
    printf("test LE_RunLoop");
}

void LE_CloseLoop(const LoopHandle loopHandle)
{
    printf("test LE_CloseLoop");
}

void LE_StopLoop(const LoopHandle handle)
{
    printf("test LE_CloseLoop");
}

LE_STATUS HandleSignalEvent(const LoopHandle loop, const TaskHandle task, uint32_t oper)
{
    return 0;
}

void HandleSignalTaskClose(const LoopHandle loopHandle, const TaskHandle signalHandle)
{
    printf("test HandleSignalTaskClose_");
}

void PrintSigset(sigset_t mask)
{
    printf("test PrintSigset");
}

void DumpSignalTaskInfo(const TaskHandle task)
{
    printf("test DumpSignalTaskInfo_");
}

LE_STATUS LE_CreateSignalTask(const LoopHandle loopHandle, SignalHandle *signalHandle, LE_ProcessSignal processSignal)
{
    return 0;
}

LE_STATUS LE_AddSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal)
{
    return 0;
}

LE_STATUS LE_RemoveSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal)
{
    return 0;
}

void LE_CloseSignalTask(const LoopHandle loopHandle, const SignalHandle signalHandle)
{
    printf("test LE_CloseSignalTask");
}

int SetSocketTimeout(int fd)
{
    return 0;
}

int CreatePipeServerSocket(const char *server, int maxClient, int public)
{
    return 0;
}

int CreatePipeSocket(const char *server)
{
    return 0;
}

LE_STATUS GetSockaddrFromServer(const char *server, struct sockaddr_in *addr)
{
    return 0;
}

int CreateTcpServerSocket(const char *server, int maxClient)
{
    return 0;
}

int CreateTcpSocket(const char *server)
{
    return 0;
}

int AcceptPipeSocket(int serverFd)
{
    return 0;
}

int AcceptTcpSocket(int serverFd)
{
    return 0;
}

int CreateSocket(int flags, const char *server)
{
    return 0;
}

int AcceptSocket(int fd, int flags)
{
    return 0;
}

int listenSocket(int fd, uint32_t flags, const char *server)
{
    return 0;
}

void DoAsyncEvent(const LoopHandle loopHandle, AsyncEventTask *asyncTask)
{
    printf("test DoAsyncEvent_");
}

void LE_DoAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    printf("test LE_DoAsyncEvent");
}

LE_STATUS HandleAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle, uint32_t oper)
{
    return 0;
}

void HandleAsyncTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    printf("test HandleAsyncTaskClose_");
}

void DumpEventTaskInfo(const TaskHandle task)
{
    printf("test DumpEventTaskInfo_");
}

LE_STATUS LE_CreateAsyncTask(const LoopHandle loopHandle,
    TaskHandle *taskHandle, LE_ProcessAsyncEvent processAsyncEvent)
{
    return 0;
}

LE_STATUS LE_StartAsyncEvent(const LoopHandle loopHandle,
    const TaskHandle taskHandle, uint64_t eventId, const uint8_t *data, uint32_t buffLen)
{
    return 0;
}

void LE_StopAsyncTask(LoopHandle loopHandle, TaskHandle taskHandle)
{
    printf("test DumpEventTaskInfo_");
}

LE_STATUS HandleSendMsg(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const LE_SendMessageComplete complete)
{
    return 0;
}

LE_STATUS HandleRecvMsg(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const LE_RecvMessage recvMessage, const LE_HandleRecvMsg handleRecvMsg)
{
    return 0;
}

LE_STATUS HandleStreamEvent(const LoopHandle loopHandle, const TaskHandle handle, uint32_t oper)
{
    return 0;
}

LE_STATUS HandleClientEvent(const LoopHandle loopHandle, const TaskHandle handle, uint32_t oper)
{
    return 0;
}

void HandleStreamTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    printf("test HandleStreamTaskClose_");
}

void DumpStreamServerTaskInfo(const TaskHandle task)
{
    printf("test HandleStreamTaskClose_");
}

void DumpStreamConnectTaskInfo(const TaskHandle task)
{
    printf("test HandleStreamTaskClose_");
}

LE_STATUS HandleServerEvent(const LoopHandle loopHandle, const TaskHandle serverTask, uint32_t oper)
{
    return 0;
}

LE_STATUS LE_CreateStreamServer(const LoopHandle loopHandle,
    TaskHandle *taskHandle, const LE_StreamServerInfo *info)
{
    return 0;
}

LE_STATUS LE_CreateStreamClient(const LoopHandle loopHandle,
    TaskHandle *taskHandle, const LE_StreamInfo *info)
{
    return 0;
}

LE_STATUS LE_AcceptStreamClient(const LoopHandle loopHandle, const TaskHandle server,
    TaskHandle *taskHandle, const LE_StreamInfo *info)
{
    return 0;
}

void LE_CloseStreamTask(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    printf("test LE_CloseStreamTask");
}

int LE_GetSocketFd(const TaskHandle taskHandle)
{
    return 0;
}

int CheckTaskFlags(const BaseTask *task, uint32_t flags)
{
    return 0;
}

int GetSocketFd(const TaskHandle task)
{
    return 0;
}

BaseTask *CreateTask(const LoopHandle loopHandle, int fd, const LE_BaseInfo *info, uint32_t size)
{
    return NULL;
}

void CloseTask(const LoopHandle loopHandle, BaseTask *task)
{
    printf("test CloseTask");
}

LE_Buffer *CreateBuffer(uint32_t bufferSize)
{
    return NULLr;
}

int IsBufferEmpty(StreamTask *task)
{
    return 0;
}

LE_Buffer *GetFirstBuffer(StreamTask *task)
{
    return NULL;
}

void AddBuffer(StreamTask *task, LE_Buffer *buffer)
{
    printf("test AddBuffer");
}

LE_Buffer *GetNextBuffer(StreamTask *task, const LE_Buffer *next)
{
    return NULL;
}

void FreeBuffer(const LoopHandle loop, StreamTask *task, LE_Buffer *buffer)
{
    printf("test FreeBuffer");
}

BufferHandle LE_CreateBuffer(const LoopHandle loop, uint32_t bufferSize)
{
    return NULL;
}

void LE_FreeBuffer(const LoopHandle loop, const TaskHandle taskHandle, const BufferHandle handle)
{
    printf("test LE_FreeBuffer");
}

uint8_t *LE_GetBufferInfo(const BufferHandle handle, uint32_t *dataSize, uint32_t *buffSize)
{
    return NULL;
}

LE_STATUS LE_Send(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const BufferHandle buffHandle, uint32_t buffLen)
{
    return 0;
}

void LE_CloseTask(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    printf("test LE_CloseTask");
}

void *LE_GetUserData(TaskHandle handle)
{
    return NULL;
}

int32_t LE_GetSendResult(const BufferHandle handle)
{
    return 0;
}

void HandleWatcherTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    printf("test HandleWatcherTaskClose_");
}

LE_STATUS HandleWatcherEvent(const LoopHandle loopHandle, const TaskHandle taskHandle, uint32_t oper)
{
    return 0;
}

void DumpWatcherTaskInfo(const TaskHandle task)
{
    printf("test DumpWatcherTaskInfo_");
}

LE_STATUS LE_StartWatcher(const LoopHandle loopHandle,
    WatcherHandle *watcherHandle, const LE_WatchInfo *info, const void *context)
{
    return 0;
}

void LE_RemoveWatcher(const LoopHandle loopHandle, const WatcherHandle watcherHandle)
{
    printf("test LE_RemoveWatcher");
}

uint64_t GetCurrentTimespec(uint64_t timeout)
{
    return 0;
}

int TimerNodeCompareProc(ListNode *node, ListNode *newNode)
{
    return 0;
}

void InsertTimerNode(EventLoop *loop, TimerNode *timer)
{
    printf("test InsertTimerNode");
}

void CheckTimeoutOfTimer(EventLoop *loop, uint64_t currTime)
{
    printf("test InsertTimerNode");
}

TimerNode *CreateTimer(void)
{
    return NULL;
}

LE_STATUS LE_CreateTimer(const LoopHandle loopHandle,
    TimerHandle *timer, LE_ProcessTimer processTimer, void *context)
{
    return 0;
}

LE_STATUS LE_StartTimer(const LoopHandle loopHandle,
    const TimerHandle timer, uint64_t timeout, uint64_t repeat)
{
    return 0;
}

uint64_t GetMinTimeoutPeriod(const EventLoop *loop)
{
    return 0;
}

void TimerNodeDestroyProc(ListNode *node)
{
    printf("test TimerNodeDestroyProc");
}

void DestroyTimerList(EventLoop *loop)
{
    printf("test DestroyTimerList");
}

void CancelTimer(TimerHandle timerHandle)
{
    printf("test CancelTimer");
}

void LE_StopTimer(const LoopHandle loopHandle, const TimerHandle timer)
{
    printf("test LE_StopTimer");
}

void SetNoBlock(int fd)
{
    printf("test SetNoBlock");
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
