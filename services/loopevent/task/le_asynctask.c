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
#include "le_task.h"
#include <time.h>
#include <sys/eventfd.h>

#include "le_loop.h"

#define MILLION_MICROSECOND 1000000
#define THOUSAND_MILLISECOND 1000

static void DoAsyncEvent_(const LoopHandle loopHandle, AsyncEventTask *asyncTask)
{
    LE_CHECK(loopHandle != NULL && asyncTask != NULL, return, "Invalid parameters");
#ifdef LOOP_DEBUG
    struct timespec startTime;
    struct timespec endTime;
    long long diff;
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
#endif
    StreamTask *task = &asyncTask->stream;
    ListNode *node = task->buffHead.next;
    if (node != &task->buffHead) {
        LE_Buffer *buffer = ListEntry(node, LE_Buffer, node);
        uint64_t eventId = *(uint64_t*)(buffer->data);
        if (asyncTask->processAsyncEvent) {
            asyncTask->processAsyncEvent((TaskHandle)asyncTask, eventId,
                (uint8_t *)(buffer->data + sizeof(uint64_t)), buffer->dataSize);
        }
        OH_ListRemove(&buffer->node);
        free(buffer);
#ifdef LOOP_DEBUG
        clock_gettime(CLOCK_MONOTONIC, &(endTime));
        diff = (long long)((endTime.tv_sec - startTime.tv_sec) * MILLION_MICROSECOND);
        if (endTime.tv_nsec > startTime.tv_nsec) {
            diff += (endTime.tv_nsec - startTime.tv_nsec) / THOUSAND_MILLISECOND; // 1000 ms
        } else {
            diff -= (endTime.tv_nsec - startTime.tv_nsec) / THOUSAND_MILLISECOND; // 1000 ms
        }
        LE_LOGI("DoAsyncEvent_ diff %ld",  diff);
#endif
    }
}

#ifdef STARTUP_INIT_TEST
void LE_DoAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    AsyncEventTask *asyncTask = (AsyncEventTask *)taskHandle;
    while (!IsBufferEmpty(&asyncTask->stream)) {
        DoAsyncEvent_(loopHandle, (AsyncEventTask *)taskHandle);
    }
}
#endif
static LE_STATUS HandleAsyncEvent_(const LoopHandle loopHandle, const TaskHandle taskHandle, uint32_t oper)
{
    LE_LOGV("HandleAsyncEvent_ fd: %d oper 0x%x", GetSocketFd(taskHandle), oper);
    EventLoop *loop = (EventLoop *)loopHandle;
    AsyncEventTask *asyncTask = (AsyncEventTask *)taskHandle;
    if (LE_TEST_FLAGS(oper, Event_Read)) {
        uint64_t eventId = 0;
        int ret = read(GetSocketFd(taskHandle), &eventId, sizeof(eventId));
        LE_LOGV("HandleAsyncEvent_ read fd:%d ret: %d eventId %llu", GetSocketFd(taskHandle), ret, eventId);
        DoAsyncEvent_(loopHandle, asyncTask);
        if (!IsBufferEmpty(&asyncTask->stream)) {
            loop->modEvent(loop, (const BaseTask *)taskHandle, Event_Write);
            return LE_SUCCESS;
        }
    } else {
        static uint64_t eventId = 0;
        (void)write(GetSocketFd(taskHandle), &eventId, sizeof(eventId));
        loop->modEvent(loop, (const BaseTask *)taskHandle, Event_Read);
        eventId++;
    }
    return LE_SUCCESS;
}

static void HandleAsyncTaskClose_(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    BaseTask *task = (BaseTask *)taskHandle;
    CloseTask(loopHandle, task);
    close(task->taskId.fd);
}

LE_STATUS LE_CreateAsyncTask(const LoopHandle loopHandle,
    TaskHandle *taskHandle, LE_ProcessAsyncEvent processAsyncEvent)
{
    LE_CHECK(loopHandle != NULL && taskHandle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    LE_CHECK(processAsyncEvent != NULL, return LE_INVALID_PARAM, "Invalid parameters processAsyncEvent ");

    int fd = eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC);
    LE_CHECK(fd > 0, return LE_FAILURE, "Failed to event fd ");
    LE_BaseInfo baseInfo = {TASK_EVENT | TASK_ASYNC_EVENT, NULL};
    AsyncEventTask *task = (AsyncEventTask *)CreateTask(loopHandle, fd, &baseInfo, sizeof(AsyncEventTask));
    LE_CHECK(task != NULL, close(fd);
        return LE_NO_MEMORY, "Failed to create task");
    task->stream.base.handleEvent = HandleAsyncEvent_;
    task->stream.base.innerClose = HandleAsyncTaskClose_;

    OH_ListInit(&task->stream.buffHead);
    LoopMutexInit(&task->stream.mutex);
    task->processAsyncEvent = processAsyncEvent;
    EventLoop *loop = (EventLoop *)loopHandle;
    loop->addEvent(loop, (const BaseTask *)task, Event_Read);
    *taskHandle = (TaskHandle)task;
    return LE_SUCCESS;
}

LE_STATUS LE_StartAsyncEvent(const LoopHandle loopHandle,
    const TaskHandle taskHandle, uint64_t eventId, const uint8_t *data, uint32_t buffLen)
{
    LE_CHECK(loopHandle != NULL && taskHandle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    BufferHandle handle = LE_CreateBuffer(loopHandle, buffLen + 1 + sizeof(eventId));
    char *buff = (char *)LE_GetBufferInfo(handle, NULL, NULL);
    int ret = memcpy_s(buff, sizeof(eventId), &eventId, sizeof(eventId));
    LE_CHECK(ret == 0, return -1, "Failed to copy data");
    if (data != NULL || buffLen == 0) {
        ret = memcpy_s(buff + sizeof(eventId), buffLen, data, buffLen);
        LE_CHECK(ret == 0, return -1, "Failed to copy data");
        buff[sizeof(eventId) + buffLen] = '\0';
    }
    return LE_Send(loopHandle, taskHandle, handle, buffLen);
}

void LE_StopAsyncTask(LoopHandle loopHandle, TaskHandle taskHandle)
{
    LE_CHECK(loopHandle != NULL && taskHandle != NULL, return, "Invalid parameters");
    LE_CloseTask(loopHandle, taskHandle);
}