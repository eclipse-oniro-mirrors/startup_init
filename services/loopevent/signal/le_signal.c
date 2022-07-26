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

#include "le_signal.h"

#include <signal.h>
#include <stdio.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "le_loop.h"
#include "le_task.h"
#include "loop_event.h"

static LE_STATUS HandleSignalEvent_(const LoopHandle loop, const TaskHandle task, uint32_t oper)
{
    if (!LE_TEST_FLAGS(oper, Event_Read)) {
        return LE_FAILURE;
    }
    struct signalfd_siginfo fdsi;
    ssize_t s = read(GetSocketFd(task), &fdsi, sizeof(fdsi));
    LE_CHECK(s == sizeof(fdsi), return LE_FAILURE, "Failed to read sign %d %d", s, errno);
    SignalTask *sigTask = (SignalTask *)task;
    if (sigTask->processSignal) {
        sigTask->processSignal(&fdsi);
    }
    return LE_SUCCESS;
}

static void HandleSignalTaskClose_(const LoopHandle loopHandle, const TaskHandle signalHandle)
{
    BaseTask *task = (BaseTask *)signalHandle;
    CloseTask(loopHandle, task);
    close(task->taskId.fd);
}

LE_STATUS LE_CreateSignalTask(const LoopHandle loopHandle, SignalHandle *signalHandle, LE_ProcessSignal processSignal)
{
    LE_CHECK(loopHandle != NULL && signalHandle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    LE_CHECK(processSignal != NULL, return LE_FAILURE, "Invalid parameters processSignal");
    sigset_t mask;
    sigemptyset(&mask);
    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    LE_CHECK(sfd > 0, return -1, "Failed to create signal fd");
    LE_BaseInfo info = {TASK_SIGNAL, NULL};
    SignalTask *task = (SignalTask *)CreateTask(loopHandle, sfd, &info, sizeof(SignalTask));
    LE_CHECK(task != NULL, return LE_NO_MEMORY, "Failed to create task");
    task->base.handleEvent = HandleSignalEvent_;
    task->base.innerClose = HandleSignalTaskClose_;
    task->sigNumber = 0;
    sigemptyset(&task->mask);
    task->processSignal = processSignal;
    *signalHandle = (SignalHandle)task;
    return LE_SUCCESS;
}

LE_STATUS LE_AddSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal)
{
    LE_CHECK(loopHandle != NULL && signalHandle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    EventLoop *loop = (EventLoop *)loopHandle;
    SignalTask *task = (SignalTask *)signalHandle;
    LE_LOGI("LE_AddSignal %d %d", signal, task->sigNumber);
    if (sigismember(&task->mask, signal)) {
        return LE_SUCCESS;
    }
    sigaddset(&task->mask, signal);
    sigprocmask(SIG_BLOCK, &task->mask, NULL);
    int sfd = signalfd(GetSocketFd(signalHandle), &task->mask, SFD_NONBLOCK | SFD_CLOEXEC);
    LE_CHECK(sfd > 0, return -1, "Failed to create signal fd");
    if (task->sigNumber == 0) {
        loop->addEvent(loop, (const BaseTask *)task, Event_Read);
    } else {
        loop->modEvent(loop, (const BaseTask *)task, Event_Read);
    }
    task->sigNumber++;
    return LE_SUCCESS;
}

LE_STATUS LE_RemoveSignal(const LoopHandle loopHandle, const SignalHandle signalHandle, int signal)
{
    LE_CHECK(loopHandle != NULL && signalHandle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    EventLoop *loop = (EventLoop *)loopHandle;
    SignalTask *task = (SignalTask *)signalHandle;
    LE_LOGI("LE_RemoveSignal %d %d", signal, task->sigNumber);
    if (!sigismember(&task->mask, signal)) {
        return LE_SUCCESS;
    }
    sigdelset(&task->mask, signal);
    task->sigNumber--;
    int sfd = signalfd(GetSocketFd(signalHandle), &task->mask, SFD_NONBLOCK | SFD_CLOEXEC);
    LE_CHECK(sfd > 0, return -1, "Failed to create signal fd");
    if (task->sigNumber <= 0) {
        loop->delEvent(loop, GetSocketFd(signalHandle), Event_Read);
    }
    return LE_SUCCESS;
}

void LE_CloseSignalTask(const LoopHandle loopHandle, const SignalHandle signalHandle)
{
    LE_CHECK(loopHandle != NULL && signalHandle != NULL, return, "Invalid parameters");
    LE_CloseTask(loopHandle, signalHandle);
}
