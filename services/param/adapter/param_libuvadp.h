/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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
#ifndef BASE_STARTUP_PARAM_LIBUVADP_H
#define BASE_STARTUP_PARAM_LIBUVADP_H
#include <string.h>
#include <unistd.h>

#include "param_message.h"
#include "param_utils.h"
#include "uv.h"

typedef struct {
    ProcessPidDelete pidDeleteProcess;
} LibuvWorkSpace;

typedef struct {
    ParamTask worker;
    TaskClose close;
    uint16_t userDataSize;
    uint16_t userDataOffset;
} LibuvBaseTask;

typedef struct {
    LibuvBaseTask base;
    RecvMessage recvMessage;
    union {
        uv_pipe_t pipe;
    } stream;
    uv_write_t writer;
} LibuvStreamTask;

typedef struct {
    LibuvBaseTask base;
    IncomingConnect incomingConnect;
    union {
        uv_pipe_t pipe;
    } server;
} LibuvServerTask;

typedef struct {
    LibuvBaseTask base;
    EventProcess process;
    EventProcess beforeProcess;
} LibuvEventTask;

typedef struct {
    uv_async_t async;
    LibuvEventTask *task;
    uint64_t eventId;
    uint32_t contentSize;
    char content[0];
} LibuvAsyncEvent;

typedef struct {
    LibuvBaseTask base;
    uv_timer_t timer;
    TimerProcess timerProcess;
    void *context;
} LibuvTimerTask;

#endif // BASE_STARTUP_PARAM_LIBUVADP_H