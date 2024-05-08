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

#include "le_timer.h"

#include <errno.h>
#include <time.h>

#include "le_loop.h"
#include "le_task.h"
#include "le_utils.h"
#include "list.h"
#include "loop_event.h"

#define TIMER_CANCELED 0x1000
#define TIMER_PROCESSING 0x2000

uint64_t GetCurrentTimespec(uint64_t timeout)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    uint64_t ms = timeout;
    ms += start.tv_sec * LE_SEC_TO_MSEC + start.tv_nsec / LE_MSEC_TO_NSEC;
    return ms;
}

static int TimerNodeCompareProc(ListNode *node, ListNode *newNode)
{
    TimerNode *timer1 = ListEntry(node, TimerNode, node);
    TimerNode *timer2 = ListEntry(newNode, TimerNode, node);
    if (timer1->endTime > timer2->endTime) {
        return 1;
    } else if (timer1->endTime == timer2->endTime) {
        return 0;
    }

    return -1;
}

static void InsertTimerNode(EventLoop *loop, TimerNode *timer)
{
    timer->endTime = GetCurrentTimespec(timer->timeout);
    LoopMutexLock(&timer->mutex);
    timer->flags &= ~TIMER_PROCESSING;
    timer->repeat--;
    OH_ListAddWithOrder(&loop->timerList, &timer->node, TimerNodeCompareProc);

    LoopMutexUnlock(&timer->mutex);
}

void CheckTimeoutOfTimer(EventLoop *loop, uint64_t currTime)
{
    const uint64_t faultTime = 10; // 10ms
    ListNode timeoutList;
    OH_ListInit(&timeoutList);
    ListNode *node = loop->timerList.next;
    while (node != &loop->timerList) {
        TimerNode *timer = ListEntry(node, TimerNode, node);
        if (timer->endTime > (currTime + faultTime)) {
            break;
        }

        LoopMutexLock(&timer->mutex);
        OH_ListRemove(&timer->node);
        OH_ListInit(&timer->node);
        LoopMutexUnlock(&timer->mutex);

        OH_ListAddTail(&timeoutList, &timer->node);
        timer->flags |= TIMER_PROCESSING;

        node = loop->timerList.next;;
    }

    node = timeoutList.next;
    while (node != &timeoutList) {
        TimerNode *timer = ListEntry(node, TimerNode, node);

        OH_ListRemove(&timer->node);
        OH_ListInit(&timer->node);
        timer->process((TimerHandle)timer, timer->context);
        if ((timer->repeat == 0) || ((timer->flags & TIMER_CANCELED) == TIMER_CANCELED)) {
            free(timer);
            node = timeoutList.next;
            continue;
        }

        InsertTimerNode(loop, timer);
        node = timeoutList.next;
    }
}

static TimerNode *CreateTimer(void)
{
    TimerNode *timer = (TimerNode *)malloc(sizeof(TimerNode));
    LE_CHECK(timer != NULL, return NULL, "Failed to create timer");
    OH_ListInit(&timer->node);
    LoopMutexInit(&timer->mutex);
    timer->timeout = 0;
    timer->repeat = 1;
    timer->flags = TASK_TIME;

    return timer;
}

LE_STATUS LE_CreateTimer(const LoopHandle loopHandle,
    TimerHandle *timer, LE_ProcessTimer processTimer, void *context)
{
    LE_CHECK(loopHandle != NULL, return LE_INVALID_PARAM, "loopHandle iS NULL");
    LE_CHECK(timer != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    LE_CHECK(processTimer != NULL, return LE_FAILURE, "Invalid parameters processTimer");

    TimerNode *timerNode = CreateTimer();
    LE_CHECK(timerNode != NULL, return LE_FAILURE, "Failed to create timer");
    timerNode->process = processTimer;
    timerNode->context = context;
    *timer = (TimerHandle)timerNode;

    return LE_SUCCESS;
}

LE_STATUS LE_StartTimer(const LoopHandle loopHandle,
    const TimerHandle timer, uint64_t timeout, uint64_t repeat)
{
    LE_CHECK(loopHandle != NULL && timer != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    EventLoop *loop = (EventLoop *)loopHandle;

    TimerNode *timerNode = (TimerNode *)timer;
    timerNode->timeout = timeout;
    timerNode->repeat = repeat > 0 ? repeat : 1;

    InsertTimerNode(loop, timerNode);
    return LE_SUCCESS;
}

uint64_t GetMinTimeoutPeriod(const EventLoop *loop)
{
    LE_CHECK(loop != NULL , return 0, "Invalid loop");
    LE_ONLY_CHECK(loop->timerList.next != &(loop->timerList), return 0);
    TimerNode *timerNode = ListEntry(loop->timerList.next, TimerNode, node);
    LE_CHECK(timerNode != NULL , return 0, "Invalid timeNode");

    return timerNode->endTime;
}

static void TimerNodeDestroyProc(ListNode *node)
{
    TimerNode *timer = ListEntry(node, TimerNode, node);
    OH_ListRemove(&timer->node);
    OH_ListInit(&timer->node);
    LoopMutexDestroy(timer->mutex);
    free(timer);
}

void DestroyTimerList(EventLoop *loop)
{
    OH_ListRemoveAll(&loop->timerList, TimerNodeDestroyProc);
}

void CancelTimer(TimerHandle timerHandle)
{
    TimerNode *timer = (TimerNode *)timerHandle;
    LE_CHECK(timer != NULL, return, "Invalid timer");

    if ((timer->flags & TIMER_PROCESSING) == TIMER_PROCESSING) {
        timer->flags |= TIMER_CANCELED;
        return;
    }
    LoopMutexLock(&timer->mutex);
    OH_ListRemove(&timer->node);
    OH_ListInit(&timer->node);
    LoopMutexUnlock(&timer->mutex);
    free(timer);
}

void LE_StopTimer(const LoopHandle loopHandle, const TimerHandle timer)
{
    CancelTimer(timer);
}