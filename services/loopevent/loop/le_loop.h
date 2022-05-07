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

#ifndef LE_EVENT_LOOP_H
#define LE_EVENT_LOOP_H
#include <unistd.h>

#include "le_task.h"
#include "le_utils.h"
#include "init_hashmap.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct EventLoop_ {
    LE_STATUS (*close)(const struct EventLoop_ *loop);
    LE_STATUS (*runLoop)(const struct EventLoop_ *loop);
    LE_STATUS (*addEvent)(const struct EventLoop_ *loop, const BaseTask *task, int op);
    LE_STATUS (*modEvent)(const struct EventLoop_ *loop, const BaseTask *task, int op);
    LE_STATUS (*delEvent)(const struct EventLoop_ *loop, int fd, int op);

    uint32_t maxevents;
    uint32_t timeout;
    uint32_t stop;
#ifdef LOOP_EVENT_USE_MUTEX
    LoopMutex mutex;
#else
    char mutex;
#endif
    HashMapHandle taskMap;
} EventLoop;

LE_STATUS CloseLoop(EventLoop *loop);
LE_STATUS AddTask(EventLoop *loop, BaseTask *task);
BaseTask *GetTaskByFd(EventLoop *loop, int fd);
void DelTask(EventLoop *loop, BaseTask *task);
LE_STATUS ProcessEvent(const EventLoop *loop, int fd, uint32_t oper);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif