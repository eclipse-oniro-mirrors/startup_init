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

#include "le_timer.h"

#include <stdio.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "le_loop.h"
#include "le_task.h"
#include "loop_event.h"

static LE_STATUS SetTimer_(int tfd, uint64_t timeout)
{
    struct itimerspec timeValue;
    time_t sec = timeout / TIMEOUT_BASE;
    timeValue.it_interval.tv_sec = sec;
    long nsec = (timeout % TIMEOUT_BASE) * TIMEOUT_BASE * TIMEOUT_BASE;
    timeValue.it_interval.tv_nsec = nsec;
    timeValue.it_value.tv_sec = sec;
    timeValue.it_value.tv_nsec = nsec;
    LE_LOGV("SetTimer_ sec %llu tv_nsec %lu", sec, nsec);
    int ret = timerfd_settime(tfd, 0, &timeValue, NULL);
    LE_CHECK(ret == 0, return LE_FAILURE, "Failed to set timer %d", errno);
    return 0;
}

static LE_STATUS HandleTimerEvent_(const LoopHandle loop, const TaskHandle task, uint32_t oper)
{
    if (!LE_TEST_FLAGS(oper, Event_Read)) {
        return LE_FAILURE;
    }
    uint64_t repeat = 0;
    (void)read(GetSocketFd(task), &repeat, sizeof(repeat));
    TimerTask *timer = (TimerTask *)task;
    int fd = GetSocketFd(task);
    if (timer->processTimer) {
        uint64_t userData = *(uint64_t *)LE_GetUserData(task);
        timer->processTimer(task, (void *)userData);
    }
    timer = (TimerTask *)GetTaskByFd((EventLoop *)loop, fd);
    if (timer == NULL) {
        return LE_SUCCESS;
    }
    if (timer->repeat <= repeat) {
        SetTimer_(fd, 0);
        return LE_SUCCESS;
    }
    timer->repeat -= repeat;
    return LE_SUCCESS;
}

static void HandleTimerClose_(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    BaseTask *task = (BaseTask *)taskHandle;
    CloseTask(loopHandle, task);
    close(task->taskId.fd);
}

LE_STATUS LE_CreateTimer(const LoopHandle loopHandle,
    TimerHandle *timer, LE_ProcessTimer processTimer, void *context)
{
    LE_CHECK(loopHandle != NULL && timer != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    LE_CHECK(processTimer != NULL, return LE_FAILURE, "Invalid parameters processTimer");
    LE_BaseInfo baseInfo = {};
    baseInfo.flags = TASK_TIME;
    baseInfo.userDataSize = sizeof(uint64_t);
    baseInfo.close = NULL;
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    LE_CHECK(fd >= 0, return LE_FAILURE, "Failed to create timer");
    SetNoBlock(fd);
    TimerTask *task = (TimerTask *)CreateTask(loopHandle, fd, &baseInfo, sizeof(TimerTask));
    LE_CHECK(task != NULL, close(fd);
        return LE_NO_MEMORY, "Failed to create task");
    task->base.handleEvent = HandleTimerEvent_;
    task->base.innerClose = HandleTimerClose_;
    task->processTimer = processTimer;
    *(uint64_t *)(task + 1) = (uint64_t)context;
    *timer = (TimerHandle)task;
    return LE_SUCCESS;
}

LE_STATUS LE_StartTimer(const LoopHandle loopHandle,
    const TimerHandle timer, uint64_t timeout, uint64_t repeat)
{
    LE_CHECK(loopHandle != NULL && timer != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    EventLoop *loop = (EventLoop *)loopHandle;
    TimerTask *task = (TimerTask *)timer;
    task->timeout = timeout;
    task->repeat = repeat;
    int ret = SetTimer_(GetSocketFd(timer), task->timeout);
    LE_CHECK(ret == 0, return LE_FAILURE, "Failed to set timer");
    ret = loop->addEvent(loop, (const BaseTask *)task, Event_Read);
    LE_CHECK(ret == 0, return LE_FAILURE, "Failed to add event");
    return LE_SUCCESS;
}

void LE_StopTimer(const LoopHandle loopHandle, const TimerHandle timer)
{
    LE_CHECK(loopHandle != NULL && timer != NULL, return, "Invalid parameters");
    LE_CloseTask(loopHandle, timer);
}