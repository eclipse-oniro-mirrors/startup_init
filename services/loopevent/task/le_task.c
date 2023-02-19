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


#include "le_loop.h"
#include "le_utils.h"

int CheckTaskFlags(const BaseTask *task, uint32_t flags)
{
    if (task == NULL) {
        return 0;
    }
    return ((task->flags & flags) == flags);
}

int GetSocketFd(const TaskHandle task)
{
    BaseTask *stream = (BaseTask *)task;
    return stream->taskId.fd;
}

BaseTask *CreateTask(const LoopHandle loopHandle, int fd, const LE_BaseInfo *info, uint32_t size)
{
    if ((size >= LOOP_MAX_BUFFER) || ((size + info->userDataSize) >= LOOP_MAX_BUFFER)) {
        return NULL;
    }
    BaseTask *task = (BaseTask *)malloc(size + info->userDataSize);
    LE_CHECK(task != NULL, return NULL, "Failed to alloc for task");
    HASHMAPInitNode(&task->hashNode);
    // key id
    task->flags = info->flags;
    task->taskId.fd = fd;
    LE_STATUS ret = AddTask((EventLoop *)loopHandle, task);
    LE_CHECK(ret == LE_SUCCESS, free(task);
        return NULL, "Failed to alloc for task");
    task->userDataSize = info->userDataSize;
    task->userDataOffset = size;
    task->close = info->close;
    return task;
}

void CloseTask(const LoopHandle loopHandle, BaseTask *task)
{
    LE_LOGV("CloseTask");
    LE_CHECK(loopHandle != NULL && task != NULL, return, "Invalid parameters");
    if (CheckTaskFlags(task, TASK_STREAM | TASK_CONNECT) ||
        CheckTaskFlags(task, TASK_EVENT | TASK_ASYNC_EVENT)) {
        StreamTask *stream = (StreamTask *)task;
        LE_Buffer *buffer = GetFirstBuffer(stream);
        while (buffer) {
            FreeBuffer(loopHandle, stream, (BufferHandle)buffer);
            buffer = GetFirstBuffer(stream);
        }
    }
    if (task->close != NULL) {
        task->close((TaskHandle)task);
    }
    DelTask((EventLoop *)loopHandle, task);
}

LE_Buffer *CreateBuffer(uint32_t bufferSize)
{
    LE_ONLY_CHECK(bufferSize < LOOP_MAX_BUFFER, return NULL);
    LE_Buffer *buffer = NULL;
    LE_CHECK((buffer = (LE_Buffer *)malloc(sizeof(LE_Buffer) + bufferSize)) != NULL,
        return NULL, "Failed to alloc memory for buffer");
    OH_ListInit(&buffer->node);
    buffer->buffSize = bufferSize;
    buffer->dataSize = 0;
    return buffer;
}

int IsBufferEmpty(StreamTask *task)
{
    LoopMutexLock(&task->mutex);
    int ret = ListEmpty(task->buffHead);
    LoopMutexUnlock(&task->mutex);
    return ret;
}

LE_Buffer *GetFirstBuffer(StreamTask *task)
{
    LoopMutexLock(&task->mutex);
    ListNode *node = task->buffHead.next;
    LE_Buffer *buffer = NULL;
    if (node != &task->buffHead) {
        buffer = ListEntry(node, LE_Buffer, node);
    }
    LoopMutexUnlock(&task->mutex);
    return buffer;
}

void AddBuffer(StreamTask *task, LE_Buffer *buffer)
{
    LoopMutexLock(&task->mutex);
    OH_ListAddTail(&task->buffHead, &buffer->node);
    LoopMutexUnlock(&task->mutex);
}

LE_Buffer *GetNextBuffer(StreamTask *task, const LE_Buffer *next)
{
    LoopMutexLock(&task->mutex);
    LE_Buffer *buffer = NULL;
    ListNode *node = NULL;
    if (next == NULL) {
        node = task->buffHead.next;
    } else {
        node = next->node.next;
    }
    if (node != &task->buffHead) {
        buffer = ListEntry(node, LE_Buffer, node);
    }
    LoopMutexUnlock(&task->mutex);
    return buffer;
}

void FreeBuffer(const LoopHandle loop, StreamTask *task, LE_Buffer *buffer)
{
    LE_CHECK(buffer != NULL, return, "Invalid buffer");
    if (task == NULL) {
        free(buffer);
        return;
    }
    if (CheckTaskFlags((BaseTask *)task, TASK_STREAM | TASK_CONNECT) ||
        CheckTaskFlags((BaseTask *)task, TASK_EVENT | TASK_ASYNC_EVENT)) {
        LoopMutexLock(&task->mutex);
        OH_ListRemove(&buffer->node);
        LoopMutexUnlock(&task->mutex);
    }
    free(buffer);
}

BufferHandle LE_CreateBuffer(const LoopHandle loop, uint32_t bufferSize)
{
    return (BufferHandle)CreateBuffer(bufferSize);
}

void LE_FreeBuffer(const LoopHandle loop, const TaskHandle taskHandle, const BufferHandle handle)
{
    FreeBuffer(loop, (StreamTask *)taskHandle, (LE_Buffer *)handle);
}

uint8_t *LE_GetBufferInfo(const BufferHandle handle, uint32_t *dataSize, uint32_t *buffSize)
{
    LE_Buffer *buffer = (LE_Buffer *)handle;
    LE_CHECK(buffer != NULL, return NULL, "Invalid buffer");
    if (dataSize) {
        *dataSize = (uint32_t)buffer->dataSize;
    }
    if (buffSize) {
        *buffSize = (uint32_t)buffer->buffSize;
    }
    return buffer->data;
}

LE_STATUS LE_Send(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const BufferHandle buffHandle, uint32_t buffLen)
{
    EventLoop *loop = (EventLoop *)loopHandle;
    if (((BaseTask *)taskHandle)->flags & TASK_FLAGS_INVALID) {
        LE_FreeBuffer(loopHandle, taskHandle, buffHandle);
        return LE_INVALID_TASK;
    }
    LE_Buffer *buffer = (LE_Buffer *)buffHandle;
    buffer->dataSize = buffLen;
    if (CheckTaskFlags((BaseTask *)taskHandle, TASK_STREAM | TASK_CONNECT)) {
        AddBuffer((StreamTask *)taskHandle, buffer);
    } else if (CheckTaskFlags((BaseTask *)taskHandle, TASK_EVENT | TASK_ASYNC_EVENT)) {
        AddBuffer((StreamTask *)taskHandle, buffer);
    }
    loop->modEvent(loop, (BaseTask *)taskHandle, Event_Write);
    return LE_SUCCESS;
}

void LE_CloseTask(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    LE_CHECK(loopHandle != NULL && taskHandle != NULL, return, "Invalid parameters");
    LE_LOGV("LE_CloseTask %d", GetSocketFd(taskHandle));
    BaseTask *task = (BaseTask *)taskHandle;
    if (task->innerClose != NULL) {
        task->innerClose(loopHandle, taskHandle);
    }
    free(task);
}

void *LE_GetUserData(TaskHandle handle)
{
    LE_CHECK(handle != NULL, return NULL, "Invalid handle");
    BaseTask *stream = (BaseTask *)handle;
    return (void *)(((char *)stream) + stream->userDataOffset);
}

int32_t LE_GetSendResult(const BufferHandle handle)
{
    LE_CHECK(handle != NULL, return 0, "Invalid handle");
    return ((LE_Buffer *)handle)->result;
}