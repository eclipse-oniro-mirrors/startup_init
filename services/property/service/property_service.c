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

#include "property_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "property.h"
#include "property_manager.h"
#include "property_request.h"
#include "trigger.h"

#include "uv.h"

#define BUFFER_SIZE 256
#define LABEL "Server"

static char *g_initContext = "";
static PropertyWorkSpace g_propertyWorkSpace = {ATOMIC_VAR_INIT(0), {}, {}, {}};

void InitPropertyService()
{
    int ret = InitPropertyWorkSpace(&g_propertyWorkSpace, 0, g_initContext);
    PROPERTY_CHECK(ret == 0, return, "Init propert workspace fail");
}

int LoadDefaultProperty(const char *fileName)
{
    u_int32_t flags = atomic_load_explicit(&g_propertyWorkSpace.flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) {
        return PROPERTY_CODE_NOT_INIT;
    }

    FILE *fp = fopen(fileName, "r");
    PROPERTY_CHECK(fp != NULL, return -1, "Open file %s fail", fileName);
    char buff[BUFFER_SIZE];
    SubStringInfo *info = malloc(sizeof(SubStringInfo) * (SUBSTR_INFO_LABEL + 1));
    while(fgets(buff, BUFFER_SIZE, fp) != NULL) {
        int subStrNumber = GetSubStringInfo(buff, strlen(buff), '\n',  info, SUBSTR_INFO_LABEL + 1);
        if (subStrNumber <= SUBSTR_INFO_LABEL) {
            continue;
        }

        if (strncmp(info[0].value, "ctl.", strlen("ctl.")) == 0) {
            PROPERTY_LOGE("Do not set ctl. properties from init %s", info[0].value);
            continue;
        }
        if (strcmp(info[0].value, "selinux.restorecon_recursive") == 0) {
            PROPERTY_LOGE("Do not set selinux.restorecon_recursive from init %s", info[0].value);
            continue;
        }
        int ret = CheckPropertyName(info[0].value, 0);
        PROPERTY_CHECK(ret == 0, continue, "Illegal property name %s", info[0].value);

        ret = WriteProperty(&g_propertyWorkSpace.propertySpace, info[0].value, info[1].value);
        PROPERTY_CHECK(ret == 0, continue, "Failed to set property %d %s", ret, buff);

        ret = WritePersistProperty(info[0].value, info[1].value);
        PROPERTY_CHECK(ret == 0, continue, "Failed to set persist property %d %s", ret, buff);
    }
    fclose(fp);
    free(info);
    PROPERTY_LOGI("LoadDefaultProperty proterty success %s", fileName);
    return 0;
}

int LoadPropertyInfo(const char *fileName)
{
    u_int32_t flags = atomic_load_explicit(&g_propertyWorkSpace.flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) {
        return PROPERTY_CODE_NOT_INIT;
    }
    FILE *fp = fopen(fileName, "r");
    PROPERTY_CHECK(fp != NULL, return -1, "Open file %s fail", fileName);
    SubStringInfo *info = malloc(sizeof(SubStringInfo) * SUBSTR_INFO_MAX);
    char buff[BUFFER_SIZE];
    int propInfoCount = 0;
    while(fgets(buff, BUFFER_SIZE, fp) != NULL) {
        int subStrNumber = GetSubStringInfo(buff, strlen(buff), ' ',  info, SUBSTR_INFO_MAX);
        if (subStrNumber <= 0) {
            continue;
        }
        int ret = WritePropertyInfo(&g_propertyWorkSpace, info, subStrNumber);
        PROPERTY_CHECK(ret == 0, continue, "Failed to write property info %d %s", ret, buff);
        propInfoCount++;
    }
    fclose(fp);
    free(info);
    PROPERTY_LOGI("Load proterty info %d success %s", propInfoCount, fileName);
    return 0;
}

static int ProcessPropertySet(RequestMsg *msg)
{
    PROPERTY_CHECK(msg != NULL, return PROPERTY_CODE_INVALID_PARAM, "Failed to check param");

    SubStringInfo info[3];
    int ret = GetSubStringInfo(msg->content, msg->contentSize, '=',  info, sizeof(info)/sizeof(info[0]));
    PROPERTY_CHECK(ret >= 2, return ret, "Failed to get name from content %s", msg->content);

    PROPERTY_LOGI("ProcessPropertySet name %s value: %s", info[0].value, info[1].value);
    ret = WritePropertyWithCheck(&g_propertyWorkSpace, &msg->securitylabel, info[0].value, info[1].value);
    PROPERTY_CHECK(ret == 0, return ret, "Failed to set property %d name %s %s", ret, info[0].value, info[1].value);

    ret = WritePersistProperty(info[0].value, info[1].value);
    PROPERTY_CHECK(ret == 0, return ret, "Failed to set property");

    // notify event to process trigger
    PostTrigger(EVENT_PROPERTY, msg->content, msg->contentSize);
    return 0;
}

static void OnClose(uv_handle_t *handle)
{
    free(handle);
}

static void OnReceiveAlloc(uv_handle_t *handle, size_t suggestedSize, uv_buf_t* buf)
{
    // 这里需要按实际消息的大小申请内存，取最大消息的长度
    buf->base = (char *)malloc(sizeof(RequestMsg) + BUFFER_SIZE * 2);
    buf->len = suggestedSize;
}

static void OnWriteResponse(uv_write_t *req, int status)
{
    // 发送成功，释放请求内存
    PROPERTY_LOGI("OnWriteResponse status %d", status);
    ResponseNode *node = (ResponseNode*)req;
    free(node);
}

static void SendResponse(uv_stream_t *handle, RequestType type, int result, void *content, int size)
{
    int ret = 0;
    // 申请整块内存，用于回复数据和写请求
    ResponseNode *response = (ResponseNode *)malloc(sizeof(ResponseNode) + size);
    PROPERTY_CHECK(response != NULL, return, "Failed to alloc memory for response");
    response->msg.type = type;
    response->msg.contentSize = size;
    response->msg.result = result;
    if (content != NULL && size != 0) {
        ret = memcpy_s(response->msg.content, size, content, size);
        PROPERTY_CHECK(ret == 0, return, "Failed to copy content");
    }
    uv_buf_t buf = uv_buf_init((char *)&response->msg, sizeof(response->msg) + size);
    ret = uv_write2(&response->writer, handle, &buf, 1, handle, OnWriteResponse);
    PROPERTY_CHECK(ret >= 0, return, "Failed to uv_write2 ret %s", uv_strerror(ret));
}

static void OnReceiveRequest(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
    if (nread <= 0 || buf == NULL || buf->base == NULL) {
        uv_close((uv_handle_t*)handle, OnClose);
        free(buf->base);
        return;
    }
    int freeHandle = 1;
    RequestMsg *msg = (RequestMsg *)buf->base;
    switch (msg->type) {
        case SET_PROPERTY: {
            freeHandle = 0;
            int ret = ProcessPropertySet(msg);
            SendResponse(handle, SET_PROPERTY, ret, NULL, 0);
            break;
        }
        default:
            PROPERTY_LOGE("not supported the command: %d", msg->type);
            break;
    }
    free(buf->base);
    uv_close((uv_handle_t*)handle, OnClose);
}

static void RemoveSocket(int sig)
{
    uv_fs_t req;
    uv_fs_unlink(uv_default_loop(), &req, PIPE_NAME, NULL);
    ClosePropertyWorkSpace(&g_propertyWorkSpace);
    ClosePersistPropertyWorkSpace();
    uv_stop(uv_default_loop());
    exit(0);
}

static void OnConnection(uv_stream_t *server, int status)
{
    PROPERTY_CHECK(status >= 0, return, "Error status %d", status);
    PROPERTY_CHECK(server != NULL, return, "Error server");
    uv_pipe_t *stream = (uv_pipe_t*)malloc(sizeof(uv_pipe_t));
    PROPERTY_CHECK(stream != NULL, return, "Failed to alloc stream");

    int ret = uv_pipe_init(uv_default_loop(), (uv_pipe_t*)stream, 1);
    PROPERTY_CHECK(ret == 0, free(stream); return, "Failed to uv_pipe_init %d", ret);

    stream->data = server;
    ret = uv_accept(server, (uv_stream_t *)stream);
    PROPERTY_CHECK(ret == 0, uv_close((uv_handle_t*)stream, NULL); free(stream);
        return, "Failed to uv_accept %d", ret);

    ret = uv_read_start((uv_stream_t *)stream, OnReceiveAlloc, OnReceiveRequest);
    PROPERTY_CHECK(ret == 0, uv_close((uv_handle_t*)stream, NULL); free(stream);
        return, "Failed to uv_read_start %d", ret);
}

int StartPropertyService()
{
    PROPERTY_LOGI("StartPropertyService.");
    uv_fs_t req;
    uv_fs_unlink(uv_default_loop(), &req, PIPE_NAME, NULL);

    signal(SIGINT, RemoveSocket);

    uv_pipe_t pipeServer;
    int ret = uv_pipe_init(uv_default_loop(), &pipeServer, 0);
    PROPERTY_CHECK(ret == 0, return ret, "Failed to uv_pipe_init %d", ret);
    ret = uv_pipe_bind(&pipeServer, PIPE_NAME);
    PROPERTY_CHECK(ret == 0, return ret, "Failed to uv_pipe_bind %d %s", ret, uv_err_name(ret));
    ret = uv_listen((uv_stream_t*)&pipeServer, SOMAXCONN, OnConnection);
    PROPERTY_CHECK(ret == 0, return ret, "Failed to uv_listen %d %s", ret, uv_err_name(ret));

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    PROPERTY_LOGI("Start service exit.");
    return 0;
}

int SystemWriteParameter(const char *name, const char *value)
{
    PROPERTY_CHECK(name != NULL && value != NULL, return -1, "The name is null");
    PROPERTY_LOGI("SystemWriteParameter name %s value: %s", name, value);
    int ret = WritePropertyWithCheck(&g_propertyWorkSpace, &g_propertyWorkSpace.label, name, value);
    if (ret == 0) {
        ret = WritePersistProperty(name, value);
        PROPERTY_CHECK(ret == 0, return ret, "Failed to set property");
    } else {
        PROPERTY_LOGE("Failed to set property %d name %s %s", ret, name, value);
    }
    // notify event to process trigger
    PostPropertyTrigger(name, value);
    return 0;
}

int SystemReadParameter(const char *name, char *value, unsigned int *len)
{
    PROPERTY_CHECK(name != NULL && len != NULL, return -1, "The name is null");
    PropertyHandle handle = 0;
    int ret = ReadPropertyWithCheck(&g_propertyWorkSpace, name, &handle);
    if (ret == 0) {
        ret = ReadPropertyValue(&g_propertyWorkSpace, handle, value, len);
    }
    return ret;
}

int SystemTraversalParameters(void (*traversalParameter)(PropertyHandle handle, void* cookie), void* cookie)
{
    PROPERTY_CHECK(traversalParameter != NULL, return -1, "The param is null");
    return TraversalProperty(&g_propertyWorkSpace, traversalParameter, cookie);
}

PropertyWorkSpace *GetPropertyWorkSpace()
{
    return &g_propertyWorkSpace;
}

int LoadPersistProperties()
{
    return RefreshPersistProperties(&g_propertyWorkSpace, g_initContext);
}
