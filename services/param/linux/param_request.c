/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "init_param.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "init_utils.h"
#include "param_manager.h"
#include "param_message.h"
#include "param_security.h"
#include "securec.h"

#define INVALID_SOCKET (-1)
static const uint32_t RECV_BUFFER_MAX = 5 * 1024;
static atomic_uint g_requestId = ATOMIC_VAR_INIT(1);
static int g_clientFd = INVALID_SOCKET;
static pthread_mutex_t g_clientMutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor)) static void ParameterInit(void)
{
    if (getpid() == 1) {
        return;
    }
    EnableInitLog(INIT_INFO);
    InitParamWorkSpace(1);
}

__attribute__((destructor)) static void ParameterDeinit(void)
{
    PARAM_LOGI("ParameterDeinit ");
    if (g_clientFd != INVALID_SOCKET) {
        close(g_clientFd);
        g_clientFd = INVALID_SOCKET;
    }
    pthread_mutex_destroy(&g_clientMutex);
}

static int ProcessRecvMsg(const ParamMessage *recvMsg)
{
    PARAM_LOGV("ProcessRecvMsg type: %u msgId: %u name %s", recvMsg->type, recvMsg->id.msgId, recvMsg->key);
    int result = PARAM_CODE_INVALID_PARAM;
    switch (recvMsg->type) {
        case MSG_SET_PARAM:
            result = ((ParamResponseMessage *)recvMsg)->result;
            break;
        case MSG_NOTIFY_PARAM: {
            uint32_t offset = 0;
            ParamMsgContent *valueContent = GetNextContent(recvMsg, &offset);
            PARAM_CHECK(valueContent != NULL, return PARAM_CODE_TIMEOUT, "Invalid msg");
            result = 0;
            break;
        }
        default:
            break;
    }
    return result;
}

static int ReadMessage(int fd, char *buffer, uint32_t timeout)
{
    int ret = 0;
    uint32_t diff = 0;
    time_t startTime;
    (void)time(&startTime);
    do {
        ssize_t recvLen = recv(fd, (char *)buffer, RECV_BUFFER_MAX, 0);
        if (recvLen > 0) {
            break;
        }
        time_t finishTime;
        (void)time(&finishTime);
        diff = (uint32_t)difftime(finishTime, startTime);
        if (diff >= timeout) {
            ret = PARAM_CODE_TIMEOUT;
            break;
        }
        if (errno == EAGAIN || errno == EINTR) {
            usleep(10*1000); // 10*1000 wait 10ms
            continue;
        }
    } while (1);
    if (ret != 0) {
        PARAM_LOGE("ReadMessage errno %d diff %u timeout %d ret %d", errno, diff, timeout, ret);
    }
    return ret;
}

static int GetClientSocket(int timeout)
{
    struct timeval time;
    time.tv_sec = timeout;
    time.tv_usec = 0;
    int clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    PARAM_CHECK(clientFd >= 0, return -1, "Failed to create socket");
    int ret = ConntectServer(clientFd, CLIENT_PIPE_NAME);
    if (ret == 0) {
        setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, (char *)&time, sizeof(struct timeval));
        setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&time, sizeof(struct timeval));
    } else {
        close(clientFd);
        clientFd = INVALID_SOCKET;
    }
    return clientFd;
}

static int StartRequest(int clientFd, ParamMessage *request, int timeout)
{
    int ret = 0;
    ssize_t sendLen = send(clientFd, (char *)request, request->msgSize, 0);
    PARAM_CHECK(sendLen >= 0, return PARAM_CODE_FAIL_CONNECT, "Failed to send message");
    PARAM_LOGV("sendMessage sendLen fd %d %zd", clientFd, sendLen);
    ret = ReadMessage(clientFd, (char *)request, timeout);
    if (ret == 0) {
        ret = ProcessRecvMsg(request);
    }
    return ret;
}

int SystemSetParameter(const char *name, const char *value)
{
    PARAM_CHECK(name != NULL && value != NULL, return -1, "Invalid name or value");
    int ctrlService = 0;
    int ret = CheckParameterSet(name, value, GetParamSecurityLabel(), &ctrlService);
    PARAM_CHECK(ret == 0, return ret, "Forbid to set parameter %s", name);

    size_t msgSize = sizeof(ParamMsgContent);
    msgSize = (msgSize < RECV_BUFFER_MAX) ? RECV_BUFFER_MAX : msgSize;

    ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SET_PARAM, name, msgSize);
    PARAM_CHECK(request != NULL, return -1, "Failed to malloc for connect");
    uint32_t offset = 0;
    ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
    PARAM_CHECK(ret == 0, free(request);
        return -1, "Failed to fill value");
    request->msgSize = offset + sizeof(ParamMessage);
    request->id.msgId = atomic_fetch_add(&g_requestId, 1);

    PARAM_LOGI("SystemSetParameter name %s", name);
    int fd = INVALID_SOCKET;
    pthread_mutex_lock(&g_clientMutex);
    if (g_clientFd == INVALID_SOCKET) {
        g_clientFd = GetClientSocket(DEFAULT_PARAM_SET_TIMEOUT);
    }
    fd = g_clientFd;
    pthread_mutex_unlock(&g_clientMutex);
    PARAM_CHECK(fd >= 0, return -1, "Failed to connect server for set %s", name);
    ret = StartRequest(fd, request, DEFAULT_PARAM_SET_TIMEOUT);
    free(request);
    PARAM_LOGI("SystemSetParameter name %s %d", name, ret);
    return ret;
}

int SystemWaitParameter(const char *name, const char *value, int32_t timeout)
{
    PARAM_CHECK(name != NULL, return -1, "Invalid name");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    ParamHandle handle = 0;
    ret = ReadParamWithCheck(name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to wait parameter %s", name);
    }
    if (timeout <= 0) {
        timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
    }
    uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + sizeof(ParamMsgContent) + sizeof(uint32_t);
    msgSize = (msgSize < RECV_BUFFER_MAX) ? RECV_BUFFER_MAX : msgSize;
    uint32_t offset = 0;
    ParamMessage *request = NULL;
    if (value != NULL && strlen(value) > 0) {
        msgSize += PARAM_ALIGN(strlen(value) + 1);
        request = (ParamMessage *)CreateParamMessage(MSG_WAIT_PARAM, name, msgSize);
        PARAM_CHECK(request != NULL, return -1, "Failed to malloc for wait");
        ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
    } else {
        msgSize += PARAM_ALIGN(1);
        request = (ParamMessage *)CreateParamMessage(MSG_WAIT_PARAM, name, msgSize);
        PARAM_CHECK(request != NULL, return -1, "Failed to malloc for wait");
        ret = FillParamMsgContent(request, &offset, PARAM_VALUE, "*", 1);
    }
    PARAM_CHECK(ret == 0, free(request);
        return -1, "Failed to fill value");
    ParamMsgContent *content = (ParamMsgContent *)(request->data + offset);
    content->type = PARAM_WAIT_TIMEOUT;
    content->contentSize = sizeof(uint32_t);
    *((uint32_t *)(content->content)) = timeout;
    offset += sizeof(ParamMsgContent) + sizeof(uint32_t);

    request->msgSize = offset + sizeof(ParamMessage);
    request->id.waitId = atomic_fetch_add(&g_requestId, 1);
#ifdef STARTUP_INIT_TEST
    timeout = 1;
#endif
    int fd = GetClientSocket(timeout);
    PARAM_CHECK(fd >= 0, return -1, "Failed to connect server for wait %s", name);
    ret = StartRequest(fd, request, timeout);
    close(fd);
    free(request);
    PARAM_LOGI("SystemWaitParameter %s value %s result %d ", name, value, ret);
    return ret;
}

int SystemCheckParamExist(const char *name)
{
    return SysCheckParamExist(name);
}

int SystemFindParameter(const char *name, ParamHandle *handle)
{
    PARAM_CHECK(name != NULL && handle != NULL, return -1, "The name or handle is null");
    int ret = ReadParamWithCheck(name, DAC_READ, handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
    }
    return ret;
}

int WatchParamCheck(const char *keyprefix)
{
    PARAM_CHECK(keyprefix != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid keyprefix");
    int ret = CheckParamName(keyprefix, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", keyprefix);
    ParamHandle handle = 0;
    ret = ReadParamWithCheck(keyprefix, DAC_WATCH, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to watch parameter %s", keyprefix);
    }
    return 0;
}
