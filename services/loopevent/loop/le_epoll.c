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

#include "le_epoll.h"
#include "le_utils.h"

static int IsValid_(const EventEpoll *loop)
{
    return loop->epollFd > 0;
}

static void GetEpollEvent_(int fd, int op, struct epoll_event *event)
{
    event->data.fd = fd;
    if (LE_TEST_FLAGS(op, Event_Read)) {
        event->events = EPOLLIN;
    }
    if (LE_TEST_FLAGS(op, Event_Write)) {
        event->events = EPOLLOUT;
    }
}

static LE_STATUS Close_(const EventLoop *loop)
{
    LE_CHECK(loop != NULL, return LE_FAILURE, "Invalid loop");
    EventEpoll *epoll = (EventEpoll *)loop;
    close(epoll->epollFd);
    free(epoll);
    return LE_SUCCESS;
}

static LE_STATUS AddEvent_(const EventLoop *loop, const BaseTask *task, int op)
{
    int ret = LE_FAILURE;
    LE_CHECK(loop != NULL, return LE_FAILURE, "Invalid loop");
    EventEpoll *epoll = (EventEpoll *)loop;

    struct epoll_event event = {};
    int fd = GetSocketFd((const TaskHandle)task);
    GetEpollEvent_(fd, op, &event);
    if (IsValid_(epoll) && fd >= 0) {
        ret = epoll_ctl(epoll->epollFd, EPOLL_CTL_ADD, fd, &event);
    }
    LE_CHECK(ret == 0, return LE_FAILURE, "Failed to add epoll_ctl %d ret %d", fd, errno);
    return LE_SUCCESS;
}

static LE_STATUS ModEvent_(const EventLoop *loop, const BaseTask *task, int op)
{
    int ret = LE_FAILURE;
    LE_CHECK(loop != NULL, return LE_FAILURE, "Invalid loop");
    EventEpoll *epoll = (EventEpoll *)loop;

    struct epoll_event event = {};
    int fd = GetSocketFd((const TaskHandle)task);
    GetEpollEvent_(fd, op, &event);
    if (IsValid_(epoll) && fd >= 0) {
        ret = epoll_ctl(epoll->epollFd, EPOLL_CTL_MOD, fd, &event);
    }
    LE_CHECK(ret == 0, return LE_FAILURE, "Failed to mod epoll_ctl %d ret %d", fd, errno);
    return LE_SUCCESS;
}

static LE_STATUS DelEvent_(const EventLoop *loop, int fd, int op)
{
    LE_CHECK(loop != NULL, return LE_FAILURE, "Invalid loop");
    EventEpoll *epoll = (EventEpoll *)loop;

    struct epoll_event event = {};
    GetEpollEvent_(fd, op, &event);
    if (IsValid_(epoll) && fd >= 0) {
        (void)epoll_ctl(epoll->epollFd, EPOLL_CTL_DEL, fd, &event);
    }
    return LE_SUCCESS;
}

static LE_STATUS RunLoop_(const EventLoop *loop)
{
    LE_CHECK(loop != NULL, return LE_FAILURE, "Invalid loop");
    EventEpoll *epoll = (EventEpoll *)loop;
    if (!IsValid_(epoll)) {
        return LE_FAILURE;
    }
    while (1) {
        int number = epoll_wait(epoll->epollFd, epoll->waitEvents, loop->maxevents, -1);
        for (int index = 0; index < number; index++) {
            if ((epoll->waitEvents[index].events & EPOLLIN) == EPOLLIN) {
                ProcessEvent(loop, epoll->waitEvents[index].data.fd, Event_Read);
            }
            if ((epoll->waitEvents[index].events & EPOLLOUT) == EPOLLOUT) {
                ProcessEvent(loop, epoll->waitEvents[index].data.fd, Event_Write);
            }
            if ((epoll->waitEvents[index].events & EPOLLERR) == EPOLLERR) {
                LE_LOGV("RunLoop_ error %d", epoll->waitEvents[index].data.fd);
                ProcessEvent(loop, epoll->waitEvents[index].data.fd, Event_Error);
            }
            if ((epoll->waitEvents[index].events & EPOLLHUP) == EPOLLHUP) {
                LE_LOGV("RunLoop_ error %d", epoll->waitEvents[index].data.fd);
                ProcessEvent(loop, epoll->waitEvents[index].data.fd, Event_Error);
            }
        }
        if (loop->stop) {
            break;
        }
    }
    return LE_SUCCESS;
}

LE_STATUS CreateEpollLoop(EventLoop **loop, uint32_t maxevents, uint32_t timeout)
{
    LE_CHECK(loop != NULL, return LE_FAILURE, "Invalid loop");
    EventEpoll *epoll = (EventEpoll *)malloc(sizeof(EventEpoll) + sizeof(struct epoll_event) * (maxevents));
    LE_CHECK(epoll != NULL, return LE_FAILURE, "Failed to alloc memory for epoll");
    epoll->epollFd = epoll_create(maxevents);
    LE_CHECK(epoll->epollFd > 0, free(epoll);
        return LE_FAILURE, "Failed to create epoll");

    *loop = (EventLoop *)epoll;
    epoll->loop.maxevents = maxevents;
    epoll->loop.timeout = timeout;
    epoll->loop.close = Close_;
    epoll->loop.runLoop = RunLoop_;
    epoll->loop.delEvent = DelEvent_;
    epoll->loop.addEvent = AddEvent_;
    epoll->loop.modEvent = ModEvent_;
    return LE_SUCCESS;
}