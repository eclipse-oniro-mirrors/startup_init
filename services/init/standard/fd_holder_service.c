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
#include <sys/socket.h>
#include <sys/un.h>

#include "fd_holder_internal.h"
#include "init_log.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "loop_event.h"
#include "securec.h"

#define MSG_ARRAY_INDEX 2

static void FreeFds(int *fds)
{
    if (fds != NULL) {
        free(fds);
    }
}

static int HandlerHoldFds(Service *service, int *fds, size_t fdCount, const char *pollStr)
{
    if (fds == NULL || fdCount == 0 || fdCount > MAX_HOLD_FDS) {
        INIT_LOGE("Service %s request hold fds with invalid fds", service->name);
        return -1;
    }

    INIT_LOGI("Hold service \' %s \' fds:", service->name);
    for (size_t i = 0; i < fdCount; i++) {
        INIT_LOGI("fd = %d", fds[i]);
    }
    if (UpdaterServiceFds(service, fds, fdCount) < 0) {
        INIT_LOGE("Failed to update service \' %s \' fds", service->name);
        return -1;
    }

    if (strcmp(pollStr, WITHPOLL) == 0) {
        // poll service fds if service asked for
        INIT_LOGI("Service \' %s \' asked init to poll fds, not implement yet.", service->name);
    }
    return 0;
}

static void SendErrorInfo(int sock, const char *errInfo, const char *serviceName)
{
    int ret = 0;
    char errBuffer[MAX_FD_HOLDER_BUFFER] = {};
    if (UNLIKELY(errInfo == NULL)) { // Should not happen.
        char *defaultError = "Unknonw error";
        ret = strncpy_s(errBuffer, MAX_FD_HOLDER_BUFFER, defaultError, strlen(defaultError));
    } else {
        ret = strncpy_s(errBuffer, MAX_FD_HOLDER_BUFFER, errInfo, strlen(errInfo));
    }
    if (ret != 0) {
        INIT_LOGE("Failed to copy, err = %d", errno);
        return;
    }

    struct iovec iovec = {
        .iov_base = errBuffer,
        .iov_len = strlen(errBuffer),
    };

    struct msghdr msghdr = {
        .msg_iov = &iovec,
        .msg_iovlen = 1,
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
    };

    if (TEMP_FAILURE_RETRY(sendmsg(sock, &msghdr, MSG_NOSIGNAL)) < 0) {
        INIT_LOGE("Failed to send err info to service \' %s \', err = %d", serviceName, errno);
    }
}

static int CheckFdHolderPermission(Service *service, pid_t requestPid)
{
    if (service == NULL) {
        INIT_LOGE("Invalid service");
        return -1;
    }

    INIT_LOGI("received service pid = %d", requestPid);
    if (service->pid < 0 || requestPid != service->pid) {
        INIT_LOGE("Service \' %s \'(pid = %d) is not valid or request with unexpected process(pid = %d)",
            service->name, service->pid, requestPid);
        return -1;
    }
    INIT_LOGI("CheckFdHolderPermission done");
    return 0;
}

static inline void CloseFds(int *fds, size_t fdCount)
{
    for (size_t i = 0; i < fdCount; i++) {
        close(fds[i]);
    }
}

static void HandlerFdHolder(int sock)
{
    char buffer[MAX_FD_HOLDER_BUFFER + 1] = {};
    size_t fdCount = 0;
    pid_t requestPid = -1;
    struct iovec iovec = {
        .iov_base = buffer,
        .iov_len = MAX_FD_HOLDER_BUFFER,
    };
    int *fds = ReceiveFds(sock, iovec, &fdCount, true, &requestPid);
    // Check what client want, is it want init to hold fds or return fds.
    INIT_LOGI("Received buffer: [%s]", buffer);
    int msgCount = 0;
    int maxMsgCount = 3;
    char **msg = SplitStringExt(buffer, "|", &msgCount, maxMsgCount);
    if (msg == NULL || msgCount != maxMsgCount) {
        INIT_LOGE("Invalid message received: %s", buffer);
        CloseFds(fds, fdCount);
        FreeFds(fds);
        return;
    }
    char *serviceName = msg[0];
    char *action = msg[1];
    char *pollStr = msg[MSG_ARRAY_INDEX];

    Service *service = GetServiceByName(serviceName);
    if (CheckFdHolderPermission(service, requestPid) < 0) {
        SendErrorInfo(sock, "Invalid service", serviceName);
        // Permission check failed.
        // But fds may already dup to init, so close them.
        CloseFds(fds, fdCount);
        FreeFds(fds);
        FreeStringVector(msg, msgCount);
        return;
    }
    if (strcmp(action, ACTION_HOLD) == 0) {
        INIT_LOGI("Service \' %s \' request init to %s fds", serviceName, action);
        if (HandlerHoldFds(service, fds, fdCount, pollStr) < 0) {
            CloseFds(fds, fdCount);
        }
    } else {
        INIT_LOGE("Unexpected action: %s", action);
        CloseFds(fds, fdCount);
    }
    FreeFds(fds);
    FreeStringVector(msg, msgCount);
}

void ProcessFdHoldEvent(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    HandlerFdHolder(fd);
    *events = EVENT_READ;
}

void RegisterFdHoldWatcher(int sock)
{
    if (sock < 0) {
        INIT_LOGE("Invalid fd holder socket");
        return;
    }

    WatcherHandle watcher = NULL;
    LE_WatchInfo info = {};
    info.fd = sock;
    info.flags = 0; // WATCHER_ONCE;
    info.events = EVENT_READ;
    info.processEvent = ProcessFdHoldEvent;
    LE_StartWatcher(LE_GetDefaultLoop(), &watcher, &info, NULL);
}
