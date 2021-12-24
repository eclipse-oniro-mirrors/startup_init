/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include <sys/epoll.h>
#include "init_log.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "securec.h"
#include "uv.h"
#define POLL_HANDLER_COUNT 1024

uv_poll_t g_socketPollHandler[POLL_HANDLER_COUNT];
static int handlerCounter = 0;

static void UVSocketPollHandler(uv_poll_t* handle, int status, int events)
{
    if (handle == NULL) {
        INIT_LOGE("handle is NULL!");
        return;
    }
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    char paramValue[PARAM_VALUE_LEN_MAX] = { 0 };
    unsigned int len = PARAM_VALUE_LEN_MAX;
    INIT_CHECK_ONLY_ELOG(snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "init.socket.%d",
        handle->io_watcher.fd) >= 0, "snprintf_s paramName error %d ", errno);
    int ret = SystemReadParam(paramName, paramValue, &len);
    if (ret != 0) {
        INIT_LOGE("Failed to read param, param name = %s", paramName);
    }
    INIT_LOGI("Socket information detected, sockFd:%d service name:%s", handle->io_watcher.fd, paramValue);
    uv_poll_stop(handle);
    StartServiceByName(paramValue, false);
    return;
}

static void SetSocketParam(Service *service)
{
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    ServiceSocket *tmpSock = service->socketCfg;
    while (tmpSock != NULL) {
        INIT_CHECK_ONLY_ELOG(snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "init.socket.%d",
            tmpSock->sockFd) >= 0, "snprintf_s paramName error %d ", errno);
        SystemWriteParam(paramName, service->name);
        (void)memset_s(paramName, PARAM_NAME_LEN_MAX, 0, PARAM_NAME_LEN_MAX);
        tmpSock = tmpSock->next;
    }
}

void SocketPollInit(int sockFd, const char* serviceName)
{
    if (handlerCounter >= POLL_HANDLER_COUNT) {
        INIT_LOGE("Socket poll handler is not enough!");
        return;
    }
    int ret = uv_poll_init_user_defined(uv_default_loop(), &g_socketPollHandler[handlerCounter], sockFd);
    if (ret != 0) {
        INIT_LOGE("Failed to init socket poll!");
        return;
    }
    ret = uv_poll_start(&g_socketPollHandler[handlerCounter], UV_READABLE, UVSocketPollHandler);
    if (ret != 0) {
        INIT_LOGE("Failed to start socket poll!");
        return;
    }
    handlerCounter += 1;
    INIT_LOGI("Start to poll socket, sockFd:%d service name:%s", sockFd, serviceName);
}

int CreateAndPollSocket(Service *service)
{
    int ret = CreateServiceSocket(service->socketCfg);
    if (ret != 0) {
        INIT_LOGE("Create socket failed!");
        return SERVICE_FAILURE;
    }
    SetSocketParam(service);
    ServiceSocket *tmpSock = service->socketCfg;
    while (tmpSock != NULL) {
        SocketPollInit(tmpSock->sockFd, service->name);
        tmpSock = tmpSock->next;
    }
    return 0;
}
