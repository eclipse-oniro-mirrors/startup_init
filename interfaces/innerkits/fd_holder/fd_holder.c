/*
* Copyright (c) 2021 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "fd_holder.h"
#include <stdio.h>
#include <errno.h>

#include "beget_ext.h"
#include "fd_holder_internal.h"
#include "init_utils.h"
#include "securec.h"

static int BuildClientSocket(void)
{
    int sockFd;
    sockFd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    BEGET_ERROR_CHECK(sockFd >= 0, return -1, "Failed to build socket, err = %d", errno);

    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int ret = strncpy_s(addr.sun_path, sizeof(addr.sun_path), INIT_HOLDER_SOCKET_PATH,
        strlen(INIT_HOLDER_SOCKET_PATH));
    BEGET_ERROR_CHECK(ret == 0, close(sockFd);
        return -1, "Failed to build socket path");

    socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1);

    ret = connect(sockFd, (struct sockaddr *)&addr, len);
    BEGET_ERROR_CHECK(ret >= 0, close(sockFd);
        return -1, "Failed to connect to socket, err = %d", errno);
    return sockFd;
}

STATIC int BuildSendData(char *buffer, size_t size, const char *serviceName, bool hold, bool poll)
{
    if (buffer == NULL || size == 0 || serviceName == 0) {
        return -1;
    }

    if (!hold && poll) {
        BEGET_LOGE("Get fd with poll set, invalid parameter");
        return -1;
    }

    char *holdString = ACTION_HOLD;
    if (!hold) {
        holdString = ACTION_GET;
    }
    char *pollString = WITHPOLL;
    if (!poll) {
        pollString = WITHOUTPOLL;
    }

    if (snprintf_s(buffer, size, size - 1, "%s|%s|%s", serviceName, holdString, pollString) == -1) {
        BEGET_LOGE("Failed to build send data");
        return -1;
    }
    return 0;
}

static int ServiceSendFds(const char *serviceName, int *fds, int fdCount, bool doPoll)
{
    int sock = BuildClientSocket();
    BEGET_CHECK(sock >= 0, return -1);

    struct iovec iovec = {};
    struct msghdr msghdr = {
        .msg_iov = &iovec,
        .msg_iovlen = 1,
    };

    char sendBuffer[MAX_FD_HOLDER_BUFFER] = {};
    int ret = BuildSendData(sendBuffer, sizeof(sendBuffer), serviceName, true, doPoll);
    BEGET_ERROR_CHECK(ret >= 0, close(sock);
        return -1, "Failed to build send data");

    BEGET_LOGV("Send data: [%s]", sendBuffer);
    iovec.iov_base = sendBuffer;
    iovec.iov_len = strlen(sendBuffer);

    if (BuildControlMessage(&msghdr, fds, fdCount, true) < 0) {
        BEGET_LOGE("Failed to build control message");
        if (msghdr.msg_control != NULL) {
            free(msghdr.msg_control);
        }
        msghdr.msg_controllen = 0;
        close(sock);
        return -1;
    }

    if (TEMP_FAILURE_RETRY(sendmsg(sock, &msghdr, MSG_NOSIGNAL)) < 0) {
        BEGET_LOGE("Failed to send fds to init, err = %d", errno);
        if (msghdr.msg_control != NULL) {
            free(msghdr.msg_control);
        }
        msghdr.msg_controllen = 0;
        close(sock);
        return -1;
    }
    if (msghdr.msg_control != NULL) {
        free(msghdr.msg_control);
    }
    msghdr.msg_controllen = 0;
    BEGET_LOGI("Send fds done");
    close(sock);
    return 0;
}

int ServiceSaveFd(const char *serviceName, int *fds, int fdCount)
{
    // Sanity checks
    if (serviceName == NULL || fds == NULL ||
        fdCount < 0 || fdCount > MAX_HOLD_FDS) {
        BEGET_LOGE("Invalid parameters");
        return -1;
    }
    return ServiceSendFds(serviceName, fds, fdCount, false);
}

int ServiceSaveFdWithPoll(const char *serviceName, int *fds, int fdCount)
{
    // Sanity checks
    if (serviceName == NULL || fds == NULL ||
        fdCount < 0 || fdCount > MAX_HOLD_FDS) {
        BEGET_LOGE("Invalid parameters");
        return -1;
    }
    return ServiceSendFds(serviceName, fds, fdCount, true);
}

int *ServiceGetFd(const char *serviceName, size_t *outfdCount)
{
    if (serviceName == NULL || outfdCount == NULL) {
        BEGET_LOGE("Invalid parameters");
        return NULL;
    }

    char path[MAX_FD_HOLDER_BUFFER] = {};
    int ret = snprintf_s(path, MAX_FD_HOLDER_BUFFER, MAX_FD_HOLDER_BUFFER - 1, ENV_FD_HOLD_PREFIX"%s", serviceName);
    BEGET_ERROR_CHECK(ret > 0, return NULL, "Failed snprintf_s err=%d", errno);
    const char *value = getenv(path);
    if (value == NULL) {
        BEGET_LOGE("Cannot get env %s\n", path);
        return NULL;
    }

    char fdBuffer[MAX_FD_HOLDER_BUFFER] = {};
    ret = strncpy_s(fdBuffer, MAX_FD_HOLDER_BUFFER - 1, value, strlen(value));
    BEGET_ERROR_CHECK(ret == 0, return NULL, "Failed strncpy_s err=%d", errno);
    BEGET_LOGV("fds = %s", fdBuffer);
    int fdCount = 0;
    char **fdList = SplitStringExt(fdBuffer, " ", &fdCount, MAX_HOLD_FDS);
    BEGET_ERROR_CHECK(fdList != NULL, return NULL, "Cannot get fd list");

    int *fds = calloc((size_t)fdCount, sizeof(int));
    BEGET_ERROR_CHECK(fds != NULL, FreeStringVector(fdList, fdCount);
        *outfdCount = 0;
        return NULL,
        "Allocate memory for fd failed. err = %d", errno);

    bool encounterError = false;
    for (int i = 0; i < fdCount; i++) {
        errno = 0;
        fds[i] = (int)strtol(fdList[i], NULL, DECIMAL_BASE);
        BEGET_ERROR_CHECK(errno == 0, encounterError = true;
            break, "Failed to convert \' %s \' to fd number", fdList[i]);
    }

    if (encounterError) {
        free(fds);
        fds = NULL;
        fdCount = 0;
    }
    *outfdCount = fdCount;
    FreeStringVector(fdList, fdCount);
    return fds;
}
