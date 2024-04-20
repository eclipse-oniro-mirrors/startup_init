/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef LOOP_TIMER_H
#define LOOP_TIMER_H
#include <stdint.h>
#include <stdlib.h>
#include "le_task.h"
#include "loop_event.h"
#include "le_loop.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LE_MSEC_TO_NSEC 1000000
#define LE_SEC_TO_MSEC 1000

typedef struct TimeNode {
    uint32_t flags;
    ListNode node;
#ifdef LOOP_EVENT_USE_MUTEX
    LoopMutex mutex;
#else
    uint8_t mutex;
#endif
    uint64_t timeout;
    uint64_t repeat;
    uint64_t endTime;
    LE_ProcessTimer process;
    void *context;
} TimerNode;

uint64_t GetCurrentTimespec(uint64_t timeout);
void CheckTimeoutOfTimer(EventLoop *loop, uint64_t currTime);
void DestroyTimerList(EventLoop *loop);
uint64_t GetMinTimeoutPeriod(const EventLoop *loop);
void CancelTimer(TimerHandle timerHandle);
#ifdef __cplusplus
}
#endif
#endif