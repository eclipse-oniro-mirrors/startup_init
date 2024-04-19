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

#include "le_loop.h"
#include "le_epoll.h"


static int TaskNodeCompare(const HashNode *node1, const HashNode *node2)
{
    BaseTask *task1 = HASHMAP_ENTRY(node1, BaseTask, hashNode);
    BaseTask *task2 = HASHMAP_ENTRY(node2, BaseTask, hashNode);
    return (int)task1->taskId.fd - (int)task2->taskId.fd;
}

static int TaskKeyCompare(const HashNode *node, const void *key)
{
    BaseTask *task = HASHMAP_ENTRY(node, BaseTask, hashNode);
    TaskId *taskId = (TaskId *)key;
    return (int)task->taskId.fd - taskId->taskId.fd;
}

static int TaskGetNodeHasCode(const HashNode *node)
{
    BaseTask *task = HASHMAP_ENTRY(node, BaseTask, hashNode);
    return task->taskId.fd;
}

static int TaskGetKeyHasCode(const void *key)
{
    TaskId *taskId = (TaskId *)key;
    return taskId->taskId.fd;
}

static void TaskNodeFree(const HashNode *node, void *context)
{
    BaseTask *task = HASHMAP_ENTRY(node, BaseTask, hashNode);
    CloseTask((const LoopHandle)context, task);
    free(task);
}

static LE_STATUS CreateLoop_(EventLoop **loop, uint32_t maxevents, uint32_t timeout)
{
#ifdef LOOP_EVENT_USE_EPOLL
    LE_STATUS ret = CreateEpollLoop(loop, maxevents, timeout);
    LE_CHECK(ret == LE_SUCCESS, return ret, "Failed to create epoll loop");
#endif
    (*loop)->maxevents = maxevents;
    (*loop)->timeout = timeout;
    (*loop)->stop = 0;
    LoopMutexInit(&(*loop)->mutex);

    OH_ListInit(&((*loop)->idleList));

    HashInfo info = {
        TaskNodeCompare,
        TaskKeyCompare,
        TaskGetNodeHasCode,
        TaskGetKeyHasCode,
        TaskNodeFree,
        128
    };
    ret = OH_HashMapCreate(&(*loop)->taskMap, &info);
    LE_CHECK(ret == LE_SUCCESS, return ret, "failed to create hash map loop");
    OH_ListInit(&((*loop)->timerList));
    return ret;
}

LE_STATUS CloseLoop(EventLoop *loop)
{
    if (!loop->stop) {
        return LE_SUCCESS;
    }
    OH_HashMapDestory(loop->taskMap, loop);
    if (loop->close) {
        loop->close(loop);
    }
    return LE_SUCCESS;
}

LE_STATUS ProcessEvent(const EventLoop *loop, int fd, uint32_t oper)
{
    BaseTask *task = GetTaskByFd((EventLoop *)loop, fd);
    if (task != NULL) {
        if (oper & EVENT_ERROR) {
            task->flags |= TASK_FLAGS_INVALID;
            loop->delEvent(loop, task->taskId.fd, EVENT_READ | EVENT_WRITE);
        }
        task->handleEvent((LoopHandle)loop, (TaskHandle)task, oper);
    } else {
        loop->delEvent(loop, fd, EVENT_READ | EVENT_WRITE);
    }
    return LE_SUCCESS;
}

LE_STATUS AddTask(EventLoop *loop, BaseTask *task)
{
    LoopMutexLock(&loop->mutex);
    int ret = OH_HashMapAdd(loop->taskMap, &task->hashNode);
    LoopMutexUnlock(&loop->mutex);
#ifndef STARTUP_INIT_TEST
    return ret;
#else
    ret = 0;
    return ret;
#endif
}

BaseTask *GetTaskByFd(EventLoop *loop, int fd)
{
    BaseTask *task = NULL;
    LoopMutexLock(&loop->mutex);
    TaskId id = {0, {fd}};
    HashNode *node = OH_HashMapGet(loop->taskMap, &id);
    if (node != NULL) {
        task = HASHMAP_ENTRY(node, BaseTask, hashNode);
    }
    LoopMutexUnlock(&loop->mutex);
    return task;
}

void DelTask(EventLoop *loop, BaseTask *task)
{
    loop->delEvent(loop, task->taskId.fd,
        EVENT_READ | EVENT_WRITE | EVENT_ERROR | EVENT_FREE | EVENT_TIMEOUT | EVENT_SIGNAL);
    LoopMutexLock(&loop->mutex);
    OH_HashMapRemove(loop->taskMap, (TaskId *)task);
    LoopMutexUnlock(&loop->mutex);
    return;
}

static EventLoop *g_defaultLoop = NULL;
LoopHandle LE_GetDefaultLoop(void)
{
    if (g_defaultLoop == NULL) {
        LE_CreateLoop((LoopHandle *)&g_defaultLoop);
    }
    return (LoopHandle)g_defaultLoop;
}

LE_STATUS LE_CreateLoop(LoopHandle *handle)
{
    EventLoop *loop = NULL;
    LE_STATUS ret = CreateLoop_(&loop, LOOP_MAX_SOCKET, DEFAULT_TIMEOUT);
    *handle = (LoopHandle)loop;
    return ret;
}

void LE_RunLoop(const LoopHandle handle)
{
    LE_CHECK(handle != NULL, return, "Invalid handle");
    EventLoop *loop = (EventLoop *)handle;
    loop->runLoop(loop);
}

void LE_CloseLoop(const LoopHandle loopHandle)
{
    LE_CHECK(loopHandle != NULL, return, "Invalid handle");
    CloseLoop((EventLoop *)loopHandle);
    if ((LoopHandle)g_defaultLoop == loopHandle) {
        g_defaultLoop = NULL;
    }
}

void LE_StopLoop(const LoopHandle handle)
{
    LE_CHECK(handle != NULL, return, "Invalid handle");
    EventLoop *loop = (EventLoop *)handle;
    loop->stop = 1;
}