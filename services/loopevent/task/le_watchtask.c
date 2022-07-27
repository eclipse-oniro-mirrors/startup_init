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

static LE_STATUS HandleWatcherEvent_(const LoopHandle loopHandle, const TaskHandle taskHandle, uint32_t oper)
{
    LE_LOGV("HandleWatcherEvent_ fd: %d oper 0x%x", GetSocketFd(taskHandle), oper);
    EventLoop *loop = (EventLoop *)loopHandle;
    WatcherTask *watcher = (WatcherTask *)taskHandle;
    int fd = GetSocketFd(taskHandle);
    uint32_t events = oper;
    uint64_t userData = *(uint64_t *)LE_GetUserData(taskHandle);
    if (watcher->processEvent != NULL) {
        watcher->processEvent(taskHandle, fd, &events, (void *)userData);
    }
    watcher = (WatcherTask *)GetTaskByFd((EventLoop *)loopHandle, fd);
    LE_ONLY_CHECK(watcher != NULL, return 0);
    if (watcher->base.flags & WATCHER_ONCE) {
        loop->delEvent(loop, fd, watcher->events);
        return 0;
    }
    if (events == 0) {
        loop->delEvent(loop, fd, watcher->events);
        return 0;
    }
    if (events != watcher->events) {
        watcher->events = events;
        loop->modEvent(loop, (const BaseTask *)taskHandle, watcher->events);
    }
    return LE_SUCCESS;
}

static void HandleWatcherTaskClose_(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    CloseTask(loopHandle, (BaseTask *)taskHandle);
}

LE_STATUS LE_StartWatcher(const LoopHandle loopHandle,
    WatcherHandle *watcherHandle, const LE_WatchInfo *info, const void *context)
{
    LE_CHECK(loopHandle != NULL && watcherHandle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    LE_CHECK(info != NULL && info->processEvent != NULL, return LE_INVALID_PARAM, "Invuint64_talid processEvent");

    LE_BaseInfo baseInfo = {TASK_WATCHER | (info->flags & WATCHER_ONCE), info->close, sizeof(uint64_t)};
    WatcherTask *task = (WatcherTask *)CreateTask(loopHandle, info->fd, &baseInfo, sizeof(WatcherTask));
    LE_CHECK(task != NULL, return LE_NO_MEMORY, "Failed to create task");
    task->base.handleEvent = HandleWatcherEvent_;
    task->base.innerClose = HandleWatcherTaskClose_;
    task->processEvent = info->processEvent;
    task->events = info->events;
    *(uint64_t *)(task + 1) = (uint64_t)context;

    EventLoop *loop = (EventLoop *)loopHandle;
    loop->addEvent(loop, (const BaseTask *)task, info->events);
    *watcherHandle = (WatcherHandle)task;
    return LE_SUCCESS;
}

void LE_RemoveWatcher(const LoopHandle loopHandle, const WatcherHandle watcherHandle)
{
    LE_CHECK(loopHandle != NULL && watcherHandle != NULL, return, "Invalid parameters");
    LE_CloseTask(loopHandle, watcherHandle);
}