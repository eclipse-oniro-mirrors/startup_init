/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "param_osadp.h"

#include <pthread.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>

#include "param_message.h"
#include "param_utils.h"
#include "securec.h"

static const uint32_t RECV_BUFFER_MAX = 5 * 1024;

static RecvMessage g_recvMessage;
static void OnReceiveRequest(const TaskHandle task, const uint8_t *buffer, uint32_t nread)
{
    if (nread == 0 || buffer == NULL || g_recvMessage == NULL) {
        return;
    }
    uint32_t curr = 0;
    while (curr < nread) {
        const ParamMessage *msg = (const ParamMessage *)(buffer + curr);
        if (((msg->msgSize + curr) > nread)
            || ((sizeof(ParamMessage) + curr) > nread)
            || (msg->msgSize < sizeof(ParamMessage))) {
            break;
        }
        curr += msg->msgSize;
        g_recvMessage(task, msg);
    }
}

int ParamServerCreate(ParamTaskPtr *stream, const ParamStreamInfo *streamInfo)
{
    PARAM_CHECK(stream != NULL && streamInfo != NULL, return -1, "Invalid param");
    PARAM_CHECK(streamInfo->incomingConnect != NULL, return -1, "Invalid incomingConnect");
    LE_StreamServerInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_SERVER;
    info.server = streamInfo->server;
    info.baseInfo.close = streamInfo->close;
    info.incommingConnect = streamInfo->incomingConnect;
    return LE_CreateStreamServer(LE_GetDefaultLoop(), stream, &info);
}

int ParamStreamCreate(ParamTaskPtr *stream, ParamTaskPtr server,
    const ParamStreamInfo *streamInfo, uint16_t userDataSize)
{
    PARAM_CHECK(stream != NULL && streamInfo != NULL, return -1, "Invalid stream");
    PARAM_CHECK(streamInfo->recvMessage != NULL, return -1, "Invalid recvMessage");
    PARAM_CHECK(streamInfo->close != NULL, return -1, "Invalid close");
    LE_StreamInfo info = {};
    info.baseInfo.userDataSize = userDataSize;
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
    if (streamInfo->flags & PARAM_TEST_FLAGS) {
        info.baseInfo.flags |= TASK_TEST;
    }
    info.baseInfo.close = streamInfo->close;
    info.disConnectComplete = NULL;
    info.sendMessageComplete = NULL;
    info.recvMessage = OnReceiveRequest;
    g_recvMessage = streamInfo->recvMessage;
    LE_STATUS status = LE_AcceptStreamClient(LE_GetDefaultLoop(), server, stream, &info);
    PARAM_CHECK(status == 0, return -1, "Failed to create client");
    return 0;
}

void *ParamGetTaskUserData(const ParamTaskPtr stream)
{
    PARAM_CHECK(stream != NULL, return NULL, "Invalid stream");
    return LE_GetUserData(stream);
}

int ParamTaskSendMsg(const ParamTaskPtr stream, const ParamMessage *msg)
{
    PARAM_CHECK(msg != NULL, return -1, "Invalid stream");
    PARAM_CHECK(stream != NULL, free((void *)msg);
        return -1, "Invalid stream");
    uint32_t dataSize = msg->msgSize;
    BufferHandle bufferHandle = LE_CreateBuffer(LE_GetDefaultLoop(), dataSize);
    PARAM_CHECK(bufferHandle != NULL, return -1, "Failed to create request");
    uint8_t *buffer = LE_GetBufferInfo(bufferHandle, NULL, NULL);
    int ret = memcpy_s(buffer, dataSize, msg, dataSize);
    free((void *)msg);
    PARAM_CHECK(ret == EOK, LE_FreeBuffer(LE_GetDefaultLoop(), NULL, bufferHandle);
        return -1, "Failed to copy message");
    return LE_Send(LE_GetDefaultLoop(), stream, bufferHandle, dataSize);
}

int ParamEventTaskCreate(ParamTaskPtr *stream, LE_ProcessAsyncEvent eventProcess)
{
    PARAM_CHECK(stream != NULL && eventProcess != NULL, return -1, "Invalid info or stream");
    return LE_CreateAsyncTask(LE_GetDefaultLoop(), stream, eventProcess);
}

int ParamEventSend(const ParamTaskPtr stream, uint64_t eventId, const char *content, uint32_t size)
{
    PARAM_CHECK(stream != NULL, return -1, "Invalid stream");
    PARAM_CHECK(size <= RECV_BUFFER_MAX, return -1, "Invalid stream");
    return LE_StartAsyncEvent(LE_GetDefaultLoop(), stream, eventId, (const uint8_t *)content, size);
}

int ParamTaskClose(const ParamTaskPtr stream)
{
    PARAM_CHECK(stream != NULL, return -1, "Invalid param");
    LE_CloseTask(LE_GetDefaultLoop(), stream);
    return 0;
}


int ParamTimerCreate(ParamTaskPtr *timer, ProcessTimer process, void *context)
{
    PARAM_CHECK(timer != NULL && process != NULL, return -1, "Invalid timer");
    LE_STATUS status = LE_CreateTimer(LE_GetDefaultLoop(), timer, (LE_ProcessTimer)process, context);
    return (int)status;
}

int ParamTimerStart(const ParamTaskPtr timer, uint64_t timeout, uint64_t repeat)
{
    PARAM_CHECK(timer != NULL, return -1, "Invalid timer");
    return LE_StartTimer(LE_GetDefaultLoop(), timer, timeout, repeat);
}

void ParamTimerClose(ParamTaskPtr timer)
{
    PARAM_CHECK(timer != NULL, return, "Invalid param");
    LE_CloseTask(LE_GetDefaultLoop(), (ParamTaskPtr)timer);
}

int ParamServiceStart(void)
{
    LE_RunLoop(LE_GetDefaultLoop());
    return 0;
}

int ParamServiceStop(void)
{
    LE_StopLoop(LE_GetDefaultLoop());
    return 0;
}