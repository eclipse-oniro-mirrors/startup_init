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

#ifndef LOOP_IDLE_H
#define LOOP_IDLE_H
#include "le_task.h"
#include "le_loop.h"
#include "loop_event.h"

/**
 * @brief Idle Task Structure
 */
typedef struct {
    /* List Node */
    ListNode node;

    /* The loop handler this idle task belongs to */
    EventLoop *loop;

    /* The actual function to be executed */
    LE_ProcessIdle processIdle;

    /* The function context pointer */
    void *context;

    /* This task will be repeat forever or just once */
    int repeat;
} IdleTask;

/**
 * @brief Execute all idle functions
 *
 * @param loopHandle the running loop
 * @return None
 */
void LE_RunIdle(const LoopHandle loopHandle);

#endif
