/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "le_idle.h"

#include <stdio.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "le_loop.h"
#include "le_task.h"
#include "loop_event.h"

/**
 * @brief Add a new idle handler
 *
 * @param loopHandle the running loop this idle will be attached
 * @param idle optional output parameter for the created idle handler
 * @param processIdle the idle handler function
 * @param context optional idle handler context
 * @param repeat if the idle function will be repeated forevent (non zero) or once (zero)
 * @return status code, 0 means succeed
 */
LE_STATUS LE_AddIdle(const LoopHandle loopHandle, IdleHandle *idle,
    LE_ProcessIdle processIdle, void *context, int repeat)
{
    LE_CHECK(loopHandle != NULL && processIdle != NULL, return LE_INVALID_PARAM, "Invalid parameters");
    IdleTask *task = (IdleTask *)calloc(1, sizeof(IdleTask));
    LE_CHECK(task != NULL,
        return LE_NO_MEMORY, "Failed to create task");

    task->loop = (EventLoop *)loopHandle;
    task->processIdle = processIdle;
    task->context = context;
    task->repeat = repeat;
    if (idle != NULL) {
        *idle = (IdleHandle)task;
    }

    // Add to list
    OH_ListAddTail(&(task->loop->idleList), &(task->node));
    return LE_SUCCESS;
}

/**
 * @brief Delete an idle handler
 *
 * @param idle idle handler
 * @return None
 */
void LE_DelIdle(IdleHandle idle)
{
    LE_CHECK(idle != NULL, return, "Invalid parameters");
    IdleTask *task = (IdleTask *)idle;
    OH_ListRemove(&(task->node));
    free((void *)task);
}

/**
 * @brief Execute an function once in the next loop
 *
 * @param loopHandle the running loop this idle will be attached
 * @param idle the function to be executed
 * @param context optional idle handler context
 * @return status code, 0 means succeed
 */
int LE_DelayProc(const LoopHandle loopHandle, LE_ProcessIdle idle, void *context)
{
    return LE_AddIdle(loopHandle, NULL, idle, context, 0);
}

static int IdleListTraversalProc(ListNode *node, void *data)
{
    IdleTask *task = (IdleTask *)node;

    // Do idle proc
    task->processIdle(task, task->context);

    if (task->repeat) {
        return 0;
    }

    // Remove if no need to repeat
    LE_DelIdle((IdleHandle)task);
    return 0;
}

/**
 * @brief Execute all idle functions
 *
 * @param loopHandle the running loop
 * @return None
 */
void LE_RunIdle(const LoopHandle loopHandle)
{
    if (loopHandle == NULL) {
        return;
    }
    EventLoop *loop = (EventLoop *)loopHandle;

    OH_ListTraversal(&(loop->idleList), NULL, IdleListTraversalProc, 0);
}
