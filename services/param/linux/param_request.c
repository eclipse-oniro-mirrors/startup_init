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
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "init_log.h"
#include "init_utils.h"
#include "param_atomic.h"
#include "param_manager.h"
#include "param_message.h"
#include "param_security.h"

#define INVALID_SOCKET (-1)
static const uint32_t RECV_BUFFER_MAX = 5 * 1024;
static ATOMIC_UINT32 g_requestId;
static int g_clientFd = INVALID_SOCKET;
static pthread_mutex_t g_clientMutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor)) static void ParameterInit(void)
{
    ATOMIC_INIT(&g_requestId, 1);
    EnableInitLog(INIT_INFO);

    PARAM_WORKSPACE_OPS ops = {0};
    ops.updaterMode = 0;
    ops.logFunc = InitLog;
#ifdef PARAM_SUPPORT_SELINUX
    ops.setfilecon = NULL;
#endif
    InitParamWorkSpace(1, &ops);

    // modify log level
    char logLevel[2] = {0}; // 2 is set param "persist.init.debug.loglevel" value length.
    uint32_t len = sizeof(logLevel);
    int ret = SystemReadParam(INIT_DEBUG_LEVEL, logLevel, &len);
    if (ret == 0) {
        errno = 0;
        int level = atoi(logLevel);
        if (errno != 0) {
            return;
        }
        SetInitLogLevel((InitLogLevel)level);
    }
}

__attribute__((destructor)) static void ParameterDeinit(void)
{
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
        case MSG_SAVE_PARAM:
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
    struct timespec startTime = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &startTime);
    do {
        ssize_t recvLen = recv(fd, (char *)buffer, RECV_BUFFER_MAX, 0);
        if (recvLen <= 0) {
            PARAM_LOGE("ReadMessage failed! errno %d", errno);
            struct timespec finishTime = {0};
            (void)clock_gettime(CLOCK_MONOTONIC, &finishTime);
            diff = IntervalTime(&finishTime, &startTime);
            if (diff >= timeout) {
                ret = PARAM_CODE_TIMEOUT;
                break;
            }
            if (errno == EAGAIN || errno == EINTR) {
                usleep(10*1000); // 10*1000 wait 10ms
                continue;
            }
        }

        if (recvLen > sizeof(ParamMessage)) {
            PARAM_LOGV("recv message len is %d", recvLen);
            break;
        }
    } while (1);

    if (ret != 0) {
        PARAM_LOGE("ReadMessage errno %d diff %u timeout %d ret %d", errno, diff, timeout, ret);
    }
    return ret;
}

static int GetClientSocket(int timeout)
{
    struct timeval time = {0};
    time.tv_sec = timeout;
    time.tv_usec = 0;
    int clientFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    PARAM_CHECK(clientFd >= 0, return INVALID_SOCKET, "Failed to create socket");
    int ret = ConnectServer(clientFd, CLIENT_PIPE_NAME);
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
    errno = 0;
    ssize_t sendLen = send(clientFd, (char *)request, request->msgSize, 0);
    if (errno == EINVAL || errno == EACCES) {
        PARAM_LOGE("send Message failed!");
        return PARAM_CODE_IPC_ERROR;
    }
    PARAM_CHECK(sendLen >= 0, return PARAM_CODE_IPC_ERROR, "Failed to send message err: %d", errno);
    PARAM_LOGV("sendMessage sendLen fd %d %zd", clientFd, sendLen);
    if (timeout <= 0) {
        return 0;
    }
    int ret = ReadMessage(clientFd, (char *)request, timeout);
    if (ret == 0) {
        ret = ProcessRecvMsg(request);
    }
    return ret;
}

static int SystemSetParameter_(const char *name, const char *value, int timeout)
{
    PARAM_CHECK(name != NULL && value != NULL, return -1, "Invalid name or value");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    ret = CheckParamValue(NULL, name, value, GetParamValueType(name));
    PARAM_CHECK(ret == 0, return ret, "Illegal param value %s", value);

    size_t msgSize = sizeof(ParamMsgContent);
    msgSize = (msgSize < RECV_BUFFER_MAX) ? RECV_BUFFER_MAX : msgSize;

    ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SET_PARAM, name, msgSize);
    PARAM_CHECK(request != NULL, return PARAM_CODE_ERROR, "Failed to create Param Message");
    uint32_t offset = 0;
    ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
    PARAM_CHECK(ret == 0, free(request);
        return PARAM_CODE_ERROR, "Failed to fill value");
    request->msgSize = offset + sizeof(ParamMessage);
    request->id.msgId = ATOMIC_SYNC_ADD_AND_FETCH(&g_requestId, 1, MEMORY_ORDER_RELAXED);

    pthread_mutex_lock(&g_clientMutex);
    int retryCount = 0;
    while (retryCount < 2) { // max retry 2
        if (g_clientFd == INVALID_SOCKET) {
            g_clientFd = GetClientSocket(DEFAULT_PARAM_SET_TIMEOUT);
        }

        if (g_clientFd < 0) {
            ret = PARAM_CODE_FAIL_CONNECT;
            PARAM_LOGE("connect param server failed!");
            break;
        }
        ret = StartRequest(g_clientFd, request, timeout);
        if (ret == PARAM_CODE_IPC_ERROR) {
            close(g_clientFd);
            g_clientFd = INVALID_SOCKET;
            retryCount++;
        } else {
            break;
        }
    }
    PARAM_LOGI("SystemSetParameter name %s msgid:%d  ret: %d ", name, request->id.msgId, ret);
    pthread_mutex_unlock(&g_clientMutex);
    free(request);
    return ret;
}

int SystemSetParameter(const char *name, const char *value)
{
    int ret = SystemSetParameter_(name, value, DEFAULT_PARAM_SET_TIMEOUT);
    BEGET_CHECK_ONLY_ELOG(ret == 0, "SystemSetParameter failed! name is :%s, the errNum is:%d", name, ret);
    return ret;
}

int SystemSetParameterNoWait(const char *name, const char *value)
{
    int ret = SystemSetParameter_(name, value, 0);
    BEGET_CHECK_ONLY_ELOG(ret == 0, "SystemSetParameterNoWait failed! name is:%s, the errNum is:%d", name, ret);
    return ret;
}

int SystemSaveParameters(void)
{
    const char *name = "persist.all";
    size_t msgSize = RECV_BUFFER_MAX;
    uint32_t offset = 0;
    ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SAVE_PARAM, name, msgSize);
    PARAM_CHECK(request != NULL, return -1, "SystemSaveParameters failed! name is:%s, the errNum is:-1", name);
    int ret = FillParamMsgContent(request, &offset, PARAM_VALUE, "*", 1);
    PARAM_CHECK(ret == 0, free(request);
        return -1, "SystemSaveParameters failed! the errNum is:-1");
    int fd = GetClientSocket(DEFAULT_PARAM_WAIT_TIMEOUT);
    PARAM_CHECK(fd >= 0, return fd, "SystemSaveParameters failed! the errNum is:%d", ret);
    request->msgSize = offset + sizeof(ParamMessage);
    request->id.msgId = ATOMIC_SYNC_ADD_AND_FETCH(&g_requestId, 1, MEMORY_ORDER_RELAXED);
    ret = StartRequest(fd, request, DEFAULT_PARAM_WAIT_TIMEOUT);
    close(fd);
    free(request);
    BEGET_CHECK_ONLY_ELOG(ret == 0, "SystemSaveParameters failed! the errNum is:%d", ret);
    return ret;
}

int SystemWaitParameter(const char *name, const char *value, int32_t timeout)
{
    PARAM_CHECK(name != NULL, return -1, "SystemWaitParameter failed! name is:%s, the errNum is:-1", name);
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "SystemWaitParameter failed! name is:%s, the errNum is:%d", name, ret);
    ret = CheckParamPermission(GetParamSecurityLabel(), name, DAC_READ);
    PARAM_CHECK(ret == 0, return ret, "SystemWaitParameter failed! name is:%s, the errNum is:%d", name, ret);
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
        PARAM_CHECK(request != NULL, return -1, "SystemWaitParameter failed! name is:%s, the errNum is:-1", name);
        ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
    } else {
        msgSize += PARAM_ALIGN(1);
        request = (ParamMessage *)CreateParamMessage(MSG_WAIT_PARAM, name, msgSize);
        PARAM_CHECK(request != NULL, return -1, "SystemWaitParameter failed! name is:%s, the errNum is:-1", name);
        ret = FillParamMsgContent(request, &offset, PARAM_VALUE, "*", 1);
    }
    PARAM_CHECK(ret == 0, free(request);
        return -1, "SystemWaitParameter failed! name is:%s, the errNum is:-1", name);
    ParamMsgContent *content = (ParamMsgContent *)(request->data + offset);
    content->type = PARAM_WAIT_TIMEOUT;
    content->contentSize = sizeof(uint32_t);
    *((uint32_t *)(content->content)) = timeout;
    offset += sizeof(ParamMsgContent) + sizeof(uint32_t);

    request->msgSize = offset + sizeof(ParamMessage);
    request->id.waitId = ATOMIC_SYNC_ADD_AND_FETCH(&g_requestId, 1, MEMORY_ORDER_RELAXED);
#ifdef STARTUP_INIT_TEST
    timeout = 1;
#endif
    int fd = GetClientSocket(timeout);
    PARAM_CHECK(fd >= 0, return fd, "SystemWaitParameter failed! name is:%s, the errNum is:%d", name, ret);
    ret = StartRequest(fd, request, timeout);
    close(fd);
    free(request);
    PARAM_LOGI("SystemWaitParameter %s value %s result %d ", name, value, ret);
    BEGET_CHECK_ONLY_ELOG(ret == 0, "SystemWaitParameter failed! name is:%s, the errNum is:%d", name, ret);
    return ret;
}

int SystemCheckParamExist(const char *name)
{
    return SysCheckParamExist(name);
}

int WatchParamCheck(const char *keyprefix)
{
    PARAM_CHECK(keyprefix != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid keyprefix");
    int ret = CheckParamName(keyprefix, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", keyprefix);
    ret = CheckParamPermission(GetParamSecurityLabel(), keyprefix, DAC_WATCH);
    PARAM_CHECK(ret == 0, return ret, "Forbid to watcher parameter %s", keyprefix);
    return 0;
}

void ResetParamSecurityLabel(void)
{
#ifdef RESET_CHILD_FOR_VERIFY
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return, "Invalid paramSpace");
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
    paramSpace->securityLabel.cred.pid = getpid();
    paramSpace->securityLabel.cred.uid = geteuid();
    paramSpace->securityLabel.cred.gid = getegid();
    paramSpace->flags |= WORKSPACE_FLAGS_NEED_ACCESS;
#endif
#endif
    PARAM_LOGI("ResetParamSecurityLabel g_clientFd: %d ", g_clientFd);
    pthread_mutex_lock(&g_clientMutex);
    if (g_clientFd != INVALID_SOCKET) {
        close(g_clientFd);
        g_clientFd = INVALID_SOCKET;
    }
    pthread_mutex_unlock(&g_clientMutex);
}
