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

#include "property_request.h"

#include <string.h>
#include <unistd.h>
#include <time.h>

#include "property_manager.h"
#include "uv.h"

#define LABEL "Client"
#define BUFFER_SIZE  200
#define PropertyEntry(ptr, type, member)   (type *)((char *)(ptr) - offsetof(type, member))

static PropertyWorkSpace g_propertyWorkSpaceReadOnly = {ATOMIC_VAR_INIT(0), {}, {}, {}};

static void OnWrite(uv_write_t* req, int status)
{
    PROPERTY_LOGI("OnWrite status %d", status);
}

static void OnReceiveAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf)
{
    // 这里需要按实际回复大小申请内存，不需要大内存
    buf->base = (char *)malloc(sizeof(ResponseMsg));
    buf->len = suggestedSize;
    PROPERTY_LOGI("OnReceiveAlloc handle %p %zu", handle, suggestedSize);
}

static void OnReceiveResponse(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
    RequestNode *req = PropertyEntry(handle, RequestNode, handle);
    PROPERTY_LOGI("OnReceiveResponse %p", handle);
    if (nread <= 0 || buf == NULL || buf->base == NULL) {
        free(buf->base);
        uv_close((uv_handle_t*)handle, NULL);
        uv_stop(req->loop);
        return;
    }
    ResponseMsg *response = (ResponseMsg *)(buf->base);
    PROPERTY_CHECK(response != NULL, return, "The response is null");
    PROPERTY_LOGI("OnReceiveResponse %p cmd %d result: %d", handle, response->type, response->result);
    switch (response->type) {
        case SET_PROPERTY:
            req->result = response->result;
            break;
        default:
            PROPERTY_LOGE("not supported the command: %d", response->type);
            break;
    }

    PROPERTY_LOGE("Close handle %p", handle);
    free(buf->base);
    uv_close((uv_handle_t*)handle, NULL);
    uv_stop(req->loop);
}

static void OnConnection(uv_connect_t *connect, int status)
{
    PROPERTY_CHECK(status >= 0, return, "Failed to conntect status %s", uv_strerror(status));
    uv_write_t wr;
    RequestNode *request = PropertyEntry(connect, RequestNode, connect);
    PROPERTY_LOGI("Connect to server handle %p", &(request->handle));
    uv_buf_t buf = uv_buf_init((char*)&request->msg, request->msg.contentSize + sizeof(request->msg));
    int ret = uv_write2(&wr, (uv_stream_t*)&(request->handle), &buf, 1, (uv_stream_t*)&(request->handle), OnWrite);
    PROPERTY_CHECK(ret >= 0, return, "Failed to uv_write2 porperty");

    // read result
    ret = uv_read_start((uv_stream_t*)&(request->handle), OnReceiveAlloc, OnReceiveResponse);
    PROPERTY_CHECK(ret >= 0, return, "Failed to uv_read_start response");
}

static int StartRequest(int cmd, RequestNode *request)
{
    PROPERTY_CHECK(request != NULL, return -1, "Invalid request");
    request->result = -1;
    request->msg.type = cmd;
    request->loop = uv_loop_new();
    uv_pipe_init(request->loop, &request->handle, 1);
    uv_pipe_connect(&request->connect, &request->handle, PIPE_NAME, OnConnection);
    uv_run(request->loop, UV_RUN_DEFAULT);
    int result = request->result;
    free(request);
    return result;
}

int SystemSetParameter(const char *name, const char *value)
{
    PROPERTY_CHECK(name != NULL && value != NULL, return -1, "Invalid param");
    int ret = CheckPropertyName(name, 0);
    PROPERTY_CHECK(ret == 0, return ret, "Illegal property name");

    PROPERTY_LOGI("StartRequest %s", name);
    u_int32_t msgSize = sizeof(RequestMsg) + strlen(name) + strlen(value) + 2;
    RequestNode *request = (RequestNode *)malloc(sizeof(RequestNode) + msgSize);
    PROPERTY_CHECK(request != NULL, return -1, "Failed to malloc for connect");

    // 带字符串结束符
    int contentSize = BuildPropertyContent(request->msg.content, msgSize - sizeof(RequestMsg), name, value);
    PROPERTY_CHECK(contentSize > 0, return -1, "Failed to copy porperty");
    request->msg.contentSize = contentSize;
    return StartRequest(SET_PROPERTY, request);
}

int SystemGetParameter(const char *name, char *value, unsigned int *len)
{
    PROPERTY_CHECK(name != NULL && len != NULL, return -1, "The name or value is null");
    InitPropertyWorkSpace(&g_propertyWorkSpaceReadOnly, 1, NULL);

    PropertyHandle handle = 0;
    int ret = ReadPropertyWithCheck(&g_propertyWorkSpaceReadOnly, name, &handle);
    PROPERTY_CHECK(ret == 0, return ret, "Can not get param for %s", name);
    return ReadPropertyValue(&g_propertyWorkSpaceReadOnly, handle, value, len);
}

int SystemGetParameterName(PropertyHandle handle, char *name, unsigned int len)
{
    PROPERTY_CHECK(name != NULL && handle != 0, return -1, "The name is null");
    InitPropertyWorkSpace(&g_propertyWorkSpaceReadOnly, 1, NULL);
    return ReadPropertyName(&g_propertyWorkSpaceReadOnly, handle, name, len);
}

int SystemGetParameterValue(PropertyHandle handle, char *value, unsigned int *len)
{
    PROPERTY_CHECK(len != NULL && handle != 0, return -1, "The value is null");
    InitPropertyWorkSpace(&g_propertyWorkSpaceReadOnly, 1, NULL);
    return ReadPropertyValue(&g_propertyWorkSpaceReadOnly, handle, value, len);
}

int SystemTraversalParameter(void (*traversalParameter)(PropertyHandle handle, void* cookie), void* cookie)
{
    PROPERTY_CHECK(traversalParameter != NULL, return -1, "The param is null");
    InitPropertyWorkSpace(&g_propertyWorkSpaceReadOnly, 1, NULL);
    return TraversalProperty(&g_propertyWorkSpaceReadOnly, traversalParameter, cookie);
}

const char *SystemDetectPropertyChange(PropertyCache *cache,
    PropertyEvaluatePtr evaluate, u_int32_t count, const char *properties[][2])
{
    PROPERTY_CHECK(cache != NULL && evaluate != NULL && properties != NULL, return NULL, "The param is null");
    return DetectPropertyChange(&g_propertyWorkSpaceReadOnly, cache, evaluate, count, properties);
}
