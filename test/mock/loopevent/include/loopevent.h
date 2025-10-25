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
#ifndef TEST_LOOPEVENT_H
#define TEST_LOOPEVENT_H
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <errno.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

LE_STATUS LE_AddIdle(const LoopHandle loopHandle, IdleHandle *idle,
    LE_ProcessIdle processIdle, void *context, int repeat);
void LE_DelIdle(IdleHandle idle);
int IdleListTraversalProc(ListNode *node, void *data);
void LE_RunIdle(const LoopHandle loopHandle);
int IsValid(const EventEpoll *loop);
void GetEpollEvent(int fd, int op, struct epoll_event *event);
LE_STATUS Close_(const EventLoop *loop);
LE_STATUS AddEvent(const EventLoop *loop, const BaseTask *task, int op);
LE_STATUS ModEvent(const EventLoop *loop, const BaseTask *task, int op);
LE_STATUS DelEvent(const EventLoop *loop, int fd, int op);
LE_STATUS RunLoop(const EventLoop *loop);
int TaskNodeCompare(const HashNode *node1, const HashNode *node2);
int TaskKeyCompare(const HashNode *node, const void *key);
int TaskGetNodeHasCode(const HashNode *node);
int TaskGetKeyHasCode(const void *key);
void TaskNodeFree(const HashNode *node, void *context);
LE_STATUS CreateLoop(EventLoop **loop, uint32_t maxevents, uint32_t timeout);
LE_STATUS CloseLoop(EventLoop *loop);
LE_STATUS ProcessEvent(const EventLoop *loop, int fd, uint32_t oper);
LE_STATUS AddTask(EventLoop *loop, BaseTask *task);
BaseTask *GetTaskByFd(EventLoop *loop, int fd);
void DelTask(EventLoop *loop, BaseTask *task);
LoopHandle LE_GetDefaultLoop(void);
LE_STATUS LE_CreateLoop(LoopHandle *handle);
void LE_RunLoop(const LoopHandle handle);
void LE_CloseLoop(const LoopHandle loopHandle);
void LE_StopLoop(const LoopHandle handle);
LE_STATUS HandleSignalEvent(const LoopHandle loop, const TaskHandle task, uint32_t oper);
void HandleSignalTaskClose(const LoopHandle loopHandle, const TaskHandle signalHandle);
void PrintSigset(sigset_t mask);
void DumpSignalTaskInfo(const TaskHandle task);
LE_STATUS LE_CreateSignalTask(const LoopHandle loopHandle, SignalHandle *signalHandle, LE_ProcessSignal processSignal);
LE_STATUS LE_AddSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal);
LE_STATUS LE_RemoveSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal);
int SetSocketTimeout(int fd);
int CreatePipeSocket(const char *server);
LE_STATUS GetSockaddrFromServer_(const char *server, struct sockaddr_in *addr);
int CreateTcpServerSocket(const char *server, int maxClient);
int CreateTcpSocket(const char *server);
int AcceptPipeSocket(int serverFd);
int AcceptTcpSocket(int serverFd);
int CreateSocket(int flags, const char *server);
int AcceptSocket(int fd, int flags);
void DoAsyncEvent(const LoopHandle loopHandle, AsyncEventTask *asyncTask);
void LE_DoAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle);
LE_STATUS HandleAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle, uint32_t oper);
void HandleAsyncTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle);
void DumpEventTaskInfo_(const TaskHandle task);
LE_STATUS LE_CreateAsyncTask(const LoopHandle loopHandle,
    TaskHandle *taskHandle, LE_ProcessAsyncEvent processAsyncEvent);
LE_STATUS LE_StartAsyncEvent(const LoopHandle loopHandle,
    const TaskHandle taskHandle, uint64_t eventId, const uint8_t *data, uint32_t buffLen);
void LE_StopAsyncTask(LoopHandle loopHandle, TaskHandle taskHandle);
LE_STATUS HandleSendMsg(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const LE_SendMessageComplete complete);
LE_STATUS HandleRecvMsg(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const LE_RecvMessage recvMessage, const LE_HandleRecvMsg handleRecvMsg);
LE_STATUS HandleStreamEvent(const LoopHandle loopHandle, const TaskHandle handle, uint32_t oper);
LE_STATUS HandleClientEvent(const LoopHandle loopHandle, const TaskHandle handle, uint32_t oper);
void HandleStreamTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle);
void DumpStreamServerTaskInfo(const TaskHandle task);
LE_STATUS HandleServerEvent_(const LoopHandle loopHandle, const TaskHandle serverTask, uint32_t oper);
LE_STATUS LE_CreateStreamServer(const LoopHandle loopHandle,
    TaskHandle *taskHandle, const LE_StreamServerInfo *info);
LE_STATUS LE_CreateStreamClient(const LoopHandle loopHandle,
    TaskHandle *taskHandle, const LE_StreamInfo *info);
LE_STATUS LE_AcceptStreamClient(const LoopHandle loopHandle, const TaskHandle server,
    TaskHandle *taskHandle, const LE_StreamInfo *info);
void LE_CloseStreamTask(const LoopHandle loopHandle, const TaskHandle taskHandle);
int LE_GetSocketFd(const TaskHandle taskHandle);
int CheckTaskFlags(const BaseTask *task, uint32_t flags);
int GetSocketFd(const TaskHandle task);
BaseTask *CreateTask(const LoopHandle loopHandle, int fd, const LE_BaseInfo *info, uint32_t size);
void CloseTask(const LoopHandle loopHandle, BaseTask *task);
LE_Buffer *CreateBuffer(uint32_t bufferSize);
int IsBufferEmpty(StreamTask *task);
LE_Buffer *GetFirstBuffer(StreamTask *task);
void AddBuffer(StreamTask *task, LE_Buffer *buffer);
LE_Buffer *GetNextBuffer(StreamTask *task, const LE_Buffer *next);
void FreeBuffer(const LoopHandle loop, StreamTask *task, LE_Buffer *buffer);
BufferHandle LE_CreateBuffer(const LoopHandle loop, uint32_t bufferSize);
void LE_FreeBuffer(const LoopHandle loop, const TaskHandle taskHandle, const BufferHandle handle);
uint8_t *LE_GetBufferInfo(const BufferHandle handle, uint32_t *dataSize, uint32_t *buffSize);
LE_STATUS LE_Send(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const BufferHandle buffHandle, uint32_t buffLen);
void LE_CloseTask(const LoopHandle loopHandle, const TaskHandle taskHandle);
void *LE_GetUserData(TaskHandle handle);
int32_t LE_GetSendResult(const BufferHandle handle);
void HandleWatcherTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle);
LE_STATUS HandleWatcherEvent(const LoopHandle loopHandle, const TaskHandle taskHandle, uint32_t oper);
void DumpWatcherTaskInfo(const TaskHandle task);
LE_STATUS LE_StartWatcher(const LoopHandle loopHandle,
    WatcherHandle *watcherHandle, const LE_WatchInfo *info, const void *context);
void LE_RemoveWatcher(const LoopHandle loopHandle, const WatcherHandle watcherHandle);
uint64_t GetCurrentTimespec(uint64_t timeout);
int TimerNodeCompareProc(ListNode *node, ListNode *newNode);
void InsertTimerNode(EventLoop *loop, TimerNode *timer);
void CheckTimeoutOfTimer(EventLoop *loop, uint64_t currTime);
TimerNode *CreateTimer(void);
LE_STATUS LE_CreateTimer(const LoopHandle loopHandle,
    TimerHandle *timer, LE_ProcessTimer processTimer, void *context);
LE_STATUS LE_StartTimer(const LoopHandle loopHandle,
    const TimerHandle timer, uint64_t timeout, uint64_t repeat);
uint64_t GetMinTimeoutPeriod(const EventLoop *loop);
void TimerNodeDestroyProc(ListNode *node);
void DestroyTimerList(EventLoop *loop);
void CancelTimer(TimerHandle timerHandle);
void LE_StopTimer(const LoopHandle loopHandle, const TimerHandle timer);
void SetNoBlock(int fd);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // TEST_WRAPPER_H