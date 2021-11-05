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
#include "param_request.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "init_utils.h"
#include "param_manager.h"
#include "param_message.h"

#define INVALID_SOCKET (-1)
#define INIT_PROCESS_PID 1

static const uint32_t RECV_BUFFER_MAX = 5 * 1024;

static atomic_uint g_requestId = ATOMIC_VAR_INIT(1);
static ClientWorkSpace g_clientSpace = { {}, -1, {} };

__attribute__((constructor)) static void ClientInit(void);
__attribute__((destructor)) static void ClientDeinit(void);

static int InitParamClient(void)
{
    if (getpid() == INIT_PROCESS_PID) {
        PARAM_LOGI("Init process, do not init client");
        return 0;
    }
    if (PARAM_TEST_FLAG(g_clientSpace.paramSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    PARAM_LOGD("InitParamClient");
    pthread_mutex_init(&g_clientSpace.mutex, NULL);
    g_clientSpace.clientFd = INVALID_SOCKET;
    return InitParamWorkSpace(&g_clientSpace.paramSpace, 1);
}

void ClientInit(void)
{
    PARAM_LOGD("ClientInit");
    (void)InitParamClient();
    // set log switch
    const uint32_t switchLength = 10; // 10 swtch length
    char logSwitch[switchLength] = {0};
    uint32_t len = switchLength;
    (void)SystemGetParameter("param.debugging", logSwitch, &len);
    if (strcmp(logSwitch, "1") == 0) {
        SetInitLogLevel(INIT_DEBUG);
    }
}

void ClientDeinit(void)
{
    CloseParamWorkSpace(&g_clientSpace.paramSpace);
}

static ParamSecurityOps *GetClientParamSecurityOps(void)
{
    return &g_clientSpace.paramSpace.paramSecurityOps;
}

static int FillLabelContent(const ParamMessage *request, uint32_t *start, uint32_t length)
{
    uint32_t bufferSize = request->msgSize - sizeof(ParamMessage);
    uint32_t offset = *start;
    PARAM_CHECK((offset + sizeof(ParamMsgContent) + length) <= bufferSize,
        return -1, "Invalid msgSize %u offset %u", request->msgSize, offset);
    ParamMsgContent *content = (ParamMsgContent *)(request->data + offset);
    content->type = PARAM_LABEL;
    content->contentSize = 0;
    ParamSecurityOps *ops = GetClientParamSecurityOps();
    if (length != 0 && ops != NULL && ops->securityEncodeLabel != NULL) {
        int ret = ops->securityEncodeLabel(g_clientSpace.paramSpace.securityLabel, content->content, &length);
        PARAM_CHECK(ret == 0, return -1, "Failed to get label length");
        content->contentSize = length;
    }
    offset += sizeof(ParamMsgContent) + PARAM_ALIGN(content->contentSize);
    *start = offset;
    return 0;
}

static int ProcessRecvMsg(const ParamMessage *recvMsg)
{
    PARAM_LOGD("ProcessRecvMsg type: %u msgId: %u name %s", recvMsg->type, recvMsg->id.msgId, recvMsg->key);
    int result = PARAM_CODE_INVALID_PARAM;
    switch (recvMsg->type) {
        case MSG_SET_PARAM:
            result = ((ParamResponseMessage *)recvMsg)->result;
            break;
        case MSG_NOTIFY_PARAM:
            result = 0;
            break;
        default:
            break;
    }
    return result;
}

static int StartRequest(int *fd, ParamMessage *request, int timeout)
{
    int ret = 0;
    struct timeval time;
    time.tv_sec = timeout;
    time.tv_usec = 0;
    do {
        int clientFd = *fd;
        if (clientFd == INVALID_SOCKET) {
            clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
            PARAM_CHECK(clientFd >= 0, return PARAM_CODE_FAIL_CONNECT, "Failed to create socket");
            ret = ConntectServer(clientFd, CLIENT_PIPE_NAME);
            PARAM_CHECK(ret == 0, close(clientFd);
                return PARAM_CODE_FAIL_CONNECT, "Failed to connect server");
            setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, (char *)&time, sizeof(struct timeval));
            setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&time, sizeof(struct timeval));
            *fd = clientFd;
        }
        ssize_t recvLen = 0;
        ssize_t sendLen = send(clientFd, (char *)request, request->msgSize, 0);
        if (sendLen > 0) {
            recvLen = recv(clientFd, (char *)request, RECV_BUFFER_MAX, 0);
            if (recvLen > 0) {
                break;
            }
        }
        ret = errno;
        close(clientFd);
        *fd = INVALID_SOCKET;
        if (errno == EAGAIN || recvLen == 0) {
            ret = PARAM_CODE_TIMEOUT;
            break;
        }
        PARAM_LOGE("Send or recv msg fail errno %d %zd %zd", errno, sendLen, recvLen);
    } while (1);

    if (ret == 0) { // check result
        ret = ProcessRecvMsg(request);
    }
    return ret;
}

static int NeedCheckParamPermission(const char *name)
{
    static char *ctrlParam[] = {
        "ohos.ctl.start",
        "ohos.ctl.stop",
        "ohos.startup.powerctrl"
    };
    for (size_t i = 0; i < ARRAY_LENGTH(ctrlParam); i++) {
        if (strcmp(name, ctrlParam[i]) == 0) {
            return 0;
        }
    }
    ParamSecurityLabel *securityLabel = g_clientSpace.paramSpace.securityLabel;
    if (securityLabel != NULL &&
        ((securityLabel->flags & LABEL_CHECK_FOR_ALL_PROCESS) == LABEL_CHECK_FOR_ALL_PROCESS) &&
        ((securityLabel->flags & LABEL_ALL_PERMISSION) != LABEL_ALL_PERMISSION)) {
        return 1;
    }
    return 0;
}

int SystemSetParameter(const char *name, const char *value)
{
    InitParamClient();
    PARAM_CHECK(name != NULL && value != NULL, return -1, "Invalid name or value");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + PARAM_ALIGN(strlen(value) + 1);
    uint32_t labelLen = 0;
    ParamSecurityOps *ops = GetClientParamSecurityOps();
    if (NeedCheckParamPermission(name)) {
        ret = CheckParamPermission(&g_clientSpace.paramSpace, g_clientSpace.paramSpace.securityLabel, name, DAC_WRITE);
        PARAM_CHECK(ret == 0, return ret, "Forbit to set parameter %s", name);
    } else if (!LABEL_IS_ALL_PERMITTED(g_clientSpace.paramSpace.securityLabel)) { // check local can check permissions
        PARAM_CHECK(ops != NULL && ops->securityEncodeLabel != NULL, return -1, "Invalid securityEncodeLabel");
        ret = ops->securityEncodeLabel(g_clientSpace.paramSpace.securityLabel, NULL, &labelLen);
        PARAM_CHECK(ret == 0, return -1, "Failed to get label length");
    }
    msgSize += sizeof(ParamMsgContent) + labelLen;
    msgSize = (msgSize < RECV_BUFFER_MAX) ? RECV_BUFFER_MAX : msgSize;

    ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SET_PARAM, name, msgSize);
    PARAM_CHECK(request != NULL, return -1, "Failed to malloc for connect");
    uint32_t offset = 0;
    ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
    PARAM_CHECK(ret == 0, free(request);
        return -1, "Failed to fill value");
    ret = FillLabelContent(request, &offset, labelLen);
    PARAM_CHECK(ret == 0, free(request);
        return -1, "Failed to fill label");
    request->msgSize = offset + sizeof(ParamMessage);
    request->id.msgId = atomic_fetch_add(&g_requestId, 1);

    pthread_mutex_lock(&g_clientSpace.mutex);
    ret = StartRequest(&g_clientSpace.clientFd, request, DEFAULT_PARAM_SET_TIMEOUT);
    pthread_mutex_unlock(&g_clientSpace.mutex);
    free(request);
    return ret;
}

int SystemWaitParameter(const char *name, const char *value, int32_t timeout)
{
    InitParamClient();
    PARAM_CHECK(name != NULL, return -1, "Invalid name");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    ParamHandle handle = 0;
    ret = ReadParamWithCheck(&g_clientSpace.paramSpace, name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to wait parameter %s", name);
    }
    if (timeout == 0) {
        timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
    }
    uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + sizeof(ParamMsgContent) + sizeof(uint32_t);
    msgSize = (msgSize < RECV_BUFFER_MAX) ? RECV_BUFFER_MAX : msgSize;
    uint32_t offset = 0;
    ParamMessage *request = NULL;
    if (value != NULL) {
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
    int fd = INVALID_SOCKET;
    ret = StartRequest(&fd, request, timeout);
    if (fd != INVALID_SOCKET) {
        close(fd);
    }
    free(request);
    PARAM_LOGI("SystemWaitParameter %s value %s result %d ", name, value, ret);
    return ret;
}

int SystemGetParameter(const char *name, char *value, unsigned int *len)
{
    InitParamClient();
    PARAM_CHECK(name != NULL && len != NULL, return -1, "The name or value is null");
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(&g_clientSpace.paramSpace, name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to get parameter %s", name);
    }
    return ReadParamValue(&g_clientSpace.paramSpace, handle, value, len);
}

int SystemFindParameter(const char *name, ParamHandle *handle)
{
    InitParamClient();
    PARAM_CHECK(name != NULL && handle != NULL, return -1, "The name or handle is null");
    int ret = ReadParamWithCheck(&g_clientSpace.paramSpace, name, DAC_READ, handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
    }
    return 0;
}

int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId)
{
    PARAM_CHECK(handle != 0 || commitId != NULL, return -1, "The handle is null");
    return ReadParamCommitId(&g_clientSpace.paramSpace, handle, commitId);
}

int SystemGetParameterName(ParamHandle handle, char *name, unsigned int len)
{
    PARAM_CHECK(name != NULL && handle != 0, return -1, "The name is null");
    return ReadParamName(&g_clientSpace.paramSpace, handle, name, len);
}

int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len)
{
    PARAM_CHECK(len != NULL && handle != 0, return -1, "The value is null");
    return ReadParamValue(&g_clientSpace.paramSpace, handle, value, len);
}

int SystemTraversalParameter(
    void (*traversalParameter)(ParamHandle handle, void *cookie), void *cookie)
{
    InitParamClient();
    PARAM_CHECK(traversalParameter != NULL, return -1, "The param is null");
    ParamHandle handle = 0;
    // check default dac
    int ret = ReadParamWithCheck(&g_clientSpace.paramSpace, "#", DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to traversal parameters");
    }
    return TraversalParam(&g_clientSpace.paramSpace, traversalParameter, cookie);
}

void SystemDumpParameters(int verbose)
{
    InitParamClient();
    // check default dac
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(&g_clientSpace.paramSpace, "#", DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0) {
        PARAM_CHECK(ret == 0, return, "Forbid to dump parameters");
    }
    DumpParameters(&g_clientSpace.paramSpace, verbose);
}

int WatchParamCheck(const char *keyprefix)
{
    InitParamClient();
    PARAM_CHECK(keyprefix != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid keyprefix");
    int ret = CheckParamName(keyprefix, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", keyprefix);
    ParamHandle handle = 0;
    ret = ReadParamWithCheck(&g_clientSpace.paramSpace, keyprefix, DAC_WATCH, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to watch parameter %s", keyprefix);
    }
    return 0;
}

#if (defined STARTUP_INIT_TEST || defined PARAM_TEST)
ParamWorkSpace *GetClientParamWorkSpace(void)
{
    return &g_clientSpace.paramSpace;
}
#endif