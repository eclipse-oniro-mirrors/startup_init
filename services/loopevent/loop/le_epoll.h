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
#ifndef EVENT_EPOLL_H
#define EVENT_EPOLL_H
#include <sys/epoll.h>
#include "le_utils.h"

#include "le_loop.h"

typedef struct {
    EventLoop loop;
    int epollFd;
    struct epoll_event waitEvents[0];
} EventEpoll;

LE_STATUS CreateEpollLoop(EventLoop **loop, uint32_t maxevents, uint32_t timeout);

#endif