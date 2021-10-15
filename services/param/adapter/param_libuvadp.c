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
#include "param_libuvadp.h"
#include <sys/wait.h>

static const uint32_t RECV_BUFFER_MAX = 5 * 1024;

static LibuvBaseTask *CreateLibuvTask(uint32_t size, uint32_t flags, uint16_t userDataSize, TaskClose close)
{
    PARAM_CHECK(size <= RECV_BUFFER_MAX, return NULL, "Invaid size %u", size);
    PARAM_CHECK(userDataSize <= RECV_BUFFER_MAX, return NULL, "Invaid user size %u", userDataSize);
    LibuvBaseTask *worker = (LibuvBaseTask *)calloc(1, size + userDataSize);
    PARAM_CHECK(worker != NULL, return NULL, "Failed to create param woker");
    worker->worker.flags = flags;
    worker->userDataSize = userDataSize;
    worker->userDataOffset = size;
    worker->close = close;
    return worker;
}

static void OnClientClose(uv_handle_t *handle)
{
    PARAM_LOGD("OnClientClose handle: %p", handle);
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    LibuvStreamTask *worker = PARAM_ENTRY(handle, LibuvStreamTask, stream);
    if (worker->base.close != NULL) {
        worker->base.close((ParamTaskPtr)worker);
    }
    free(worker);
}

static void OnServerClose(uv_handle_t *handle)
{
    PARAM_LOGD("OnServerClose handle: %p", handle);
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    LibuvServerTask *worker = PARAM_ENTRY(handle, LibuvServerTask, server);
    if (worker->base.close != NULL) {
        worker->base.close((ParamTaskPtr)worker);
    }
    free(worker);
}

static void OnTimerClose(uv_handle_t *handle)
{
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    LibuvTimerTask *worker = PARAM_ENTRY(handle, LibuvTimerTask, timer);
    if (worker->base.close != NULL) {
        worker->base.close((ParamTaskPtr)worker);
    }
    free(worker);
}

static void OnReceiveAlloc(uv_handle_t *handle, size_t suggestedSize, uv_buf_t *buf)
{
    UNUSED(suggestedSize);
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    buf->len = RECV_BUFFER_MAX;
    buf->base = (char *)malloc(buf->len);
}

static void OnWriteResponse(uv_write_t *req, int status)
{
    UNUSED(status);
    PARAM_CHECK(req != NULL, return, "Invalid req");
    PARAM_LOGD("OnWriteResponse handle: %p", req);
}

static void OnReceiveRequest(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
    if (nread <= 0 || buf == NULL || buf->base == NULL) {
        uv_close((uv_handle_t *)handle, OnClientClose);
        if (buf != NULL) {
            free(buf->base);
        }
        return;
    }
    LibuvStreamTask *client = PARAM_ENTRY(handle, LibuvStreamTask, stream);
    if (client->recvMessage != NULL) {
        client->recvMessage(&client->base.worker, (const ParamMessage *)buf->base);
    }
    free(buf->base);
}

static void OnAsyncCloseCallback(uv_handle_t *handle)
{
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    free(handle);
}

static void OnAsyncCallback(uv_async_t *handle)
{
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    LibuvAsyncEvent *event = (LibuvAsyncEvent *)handle;
    if (event->task != NULL && event->task->process != NULL) {
        event->task->process(event->eventId, event->content, event->contentSize);
    }
    uv_close((uv_handle_t *)handle, OnAsyncCloseCallback);
}

static void OnConnection(uv_stream_t *server, int status)
{
    PARAM_CHECK(status >= 0, return, "Error status %d", status);
    PARAM_CHECK(server != NULL, return, "Error server");
    LibuvServerTask *pipeServer = PARAM_ENTRY(server, LibuvServerTask, server);
    PARAM_LOGD("OnConnection pipeServer: %p pip %p", pipeServer, server);
    if (pipeServer->incomingConnect) {
        pipeServer->incomingConnect((ParamTaskPtr)pipeServer, 0);
    }
}

static void LibuvFreeMsg(const ParamTaskPtr stream, const ParamMessage *msg)
{
    PARAM_CHECK(stream != NULL, return, "Invalid stream");
    PARAM_CHECK(msg != NULL, return, "Invalid msg");
    ParamMessage *message = (ParamMessage *)msg;
    free(message);
}

static int InitPipeSocket(uv_pipe_t *pipeServer)
{
    uv_fs_t req;
    uv_fs_unlink(uv_default_loop(), &req, PIPE_NAME, NULL);
    int ret = uv_pipe_init(uv_default_loop(), pipeServer, 0);
    PARAM_CHECK(ret == 0, return ret, "Failed to uv_pipe_init %d", ret);
    ret = uv_pipe_bind(pipeServer, PIPE_NAME);
    PARAM_CHECK(ret == 0, return ret, "Failed to uv_pipe_bind %d %s", ret, uv_err_name(ret));
    ret = uv_listen((uv_stream_t *)pipeServer, SOMAXCONN, OnConnection);
    PARAM_CHECK(ret == 0, return ret, "Failed to uv_listen %d %s", ret, uv_err_name(ret));
    PARAM_CHECK(chmod(PIPE_NAME, S_IRWXU | S_IRWXG | S_IRWXO) == 0,
        return -1, "Open file %s error %d", PIPE_NAME, errno);
    return 0;
}

static void LibuvTimerCallback(uv_timer_t *handle)
{
    PARAM_CHECK(handle != NULL, return, "Invalid handle");
    LibuvTimerTask *timer = PARAM_ENTRY(handle, LibuvTimerTask, timer);
    timer->timerProcess(&timer->base.worker, timer->context);
}

int ParamServerCreate(ParamTaskPtr *stream, const ParamStreamInfo *info)
{
    PARAM_CHECK(stream != NULL && info != NULL, return -1, "Invalid param");
    PARAM_CHECK(info->incomingConnect != NULL, return -1, "Invalid incomingConnect");

    LibuvServerTask *worker = (LibuvServerTask *)CreateLibuvTask(
        sizeof(LibuvServerTask), WORKER_TYPE_SERVER | info->flags, 0, info->close);
    PARAM_CHECK(worker != NULL, return -1, "Failed to add param woker");
    InitPipeSocket(&worker->server.pipe);
    PARAM_LOGD("OnConnection pipeServer: %p pipe %p", worker, &worker->server.pipe);
    worker->incomingConnect = info->incomingConnect;
    *stream = &worker->base.worker;
    return 0;
}

int ParamStreamCreate(ParamTaskPtr *stream, ParamTaskPtr server, const ParamStreamInfo *info, uint16_t userDataSize)
{
    PARAM_CHECK(stream != NULL && info != NULL, return -1, "Invalid stream");
    PARAM_CHECK(info->recvMessage != NULL, return -1, "Invalid recvMessage");
    PARAM_CHECK(info->close != NULL, return -1, "Invalid close");

    LibuvServerTask *pipeServer = (LibuvServerTask *)server;
    LibuvStreamTask *client = (LibuvStreamTask *)CreateLibuvTask(sizeof(LibuvStreamTask),
        info->flags | WORKER_TYPE_MSG, userDataSize, info->close);
    PARAM_CHECK(client != NULL, return -1, "Failed to add client");
    if (server != NULL) {
        uv_pipe_t *pipe = &client->stream.pipe;
        int ret = uv_pipe_init(uv_default_loop(), (uv_pipe_t *)pipe, 1);
        PARAM_CHECK(ret == 0, free(client);
            return -1, "Failed to uv_pipe_init %d", ret);
        pipe->data = &pipeServer->server;
        PARAM_LOGD("OnConnection pipeServer: %p pipe %p", pipeServer, &pipeServer->server);
#ifndef STARTUP_INIT_TEST
        ret = uv_accept((uv_stream_t *)&pipeServer->server.pipe, (uv_stream_t *)pipe);
        PARAM_CHECK(ret == 0, uv_close((uv_handle_t *)pipe, NULL);
            free(client);
            return -1, "Failed to uv_accept %d", ret);
        ret = uv_read_start((uv_stream_t *)pipe, OnReceiveAlloc, OnReceiveRequest);
        PARAM_CHECK(ret == 0, uv_close((uv_handle_t *)pipe, NULL);
            free(client);
            return -1, "Failed to uv_read_start %d", ret);
#endif
    }
    client->recvMessage = info->recvMessage;
    *stream = &client->base.worker;
    return 0;
}

void *ParamGetTaskUserData(ParamTaskPtr stream)
{
    PARAM_CHECK(stream != NULL, return NULL, "Invalid stream");
    if ((stream->flags & WORKER_TYPE_CLIENT) != WORKER_TYPE_CLIENT) {
        return NULL;
    }
    LibuvStreamTask *client = (LibuvStreamTask *)stream;
    if (client->base.userDataSize == 0) {
        return NULL;
    }
    return (void *)(((char *)stream) + client->base.userDataOffset);
}

int ParamTaskSendMsg(const ParamTaskPtr stream, const ParamMessage *msg)
{
    PARAM_CHECK(stream != NULL && msg != NULL, LibuvFreeMsg(stream, msg);
        return -1, "Invalid stream");
    if ((stream->flags & WORKER_TYPE_MSG) != WORKER_TYPE_MSG) {
        LibuvFreeMsg(stream, msg);
        return -1;
    }
#ifndef STARTUP_INIT_TEST
    LibuvStreamTask *worker = (LibuvStreamTask *)stream;
    uv_buf_t buf = uv_buf_init((char *)msg, msg->msgSize);
    int ret = uv_write(&worker->writer, (uv_stream_t *)&worker->stream.pipe, &buf, 1, OnWriteResponse);
    PARAM_CHECK(ret >= 0, LibuvFreeMsg(stream, msg);
        return -1, "Failed to uv_write2 ret %s", uv_strerror(ret));
#endif
    LibuvFreeMsg(stream, msg);
    return 0;
}

int ParamEventTaskCreate(ParamTaskPtr *stream, EventProcess eventProcess, EventProcess eventBeforeProcess)
{
    PARAM_CHECK(stream != NULL && eventProcess != NULL, return -1, "Invalid info or stream");
    LibuvEventTask *worker = (LibuvEventTask *)CreateLibuvTask(sizeof(LibuvEventTask),
        WORKER_TYPE_EVENT | WORKER_TYPE_ASYNC, 0, NULL);
    PARAM_CHECK(worker != NULL, return -1, "Failed to alloc worker");
    worker->process = eventProcess;
    worker->beforeProcess = eventBeforeProcess;
    *stream = &worker->base.worker;
    return 0;
}

int ParamEventSend(ParamTaskPtr stream, uint64_t eventId, const char *content, uint32_t size)
{
    PARAM_CHECK(stream != NULL, return -1, "Invalid stream");
    PARAM_CHECK(size <= RECV_BUFFER_MAX, return -1, "Invalid stream");
    PARAM_CHECK((stream->flags & WORKER_TYPE_EVENT) == WORKER_TYPE_EVENT, return -1, "Invalid stream type");
    int ret = PARAM_CODE_INVALID_PARAM;
    if (stream->flags & WORKER_TYPE_ASYNC) {
        LibuvEventTask *worker = (LibuvEventTask *)stream;
        LibuvAsyncEvent *event = (LibuvAsyncEvent *)calloc(1, sizeof(LibuvAsyncEvent) + size + 1);
        PARAM_CHECK(event != NULL, return -1, "Failed to alloc event");
        event->eventId = eventId;
        event->contentSize = size + 1;
        event->task = worker;
        if (content != NULL) {
            ret = memcpy_s(event->content, event->contentSize, content, size);
            PARAM_CHECK(ret == EOK, free(event);
                return -1, "Failed to memcpy content ");
            event->content[size] = '\0';
        }
        uv_async_init(uv_default_loop(), &event->async, OnAsyncCallback);
        if (worker->beforeProcess != NULL) {
            worker->beforeProcess(eventId, content, size);
        }
        uv_async_send(&event->async);
        ret = 0;
    }
    return ret;
}

int ParamTaskClose(ParamTaskPtr stream)
{
    PARAM_CHECK(stream != NULL, return -1, "Invalid param");
    if (stream->flags & WORKER_TYPE_TIMER) {
        LibuvTimerTask *worker = (LibuvTimerTask *)stream;
        uv_timer_stop(&worker->timer);
        uv_close((uv_handle_t *)(&worker->timer), OnTimerClose);
    } else if (stream->flags & WORKER_TYPE_SERVER) {
        LibuvServerTask *worker = (LibuvServerTask *)stream;
        uv_close((uv_handle_t *)(&worker->server.pipe), OnServerClose);
    } else if (stream->flags & WORKER_TYPE_MSG) {
        LibuvStreamTask *worker = (LibuvStreamTask *)stream;
        uv_close((uv_handle_t *)(&worker->stream.pipe), OnClientClose);
    } else {
        free(stream);
    }
    return 0;
}

int ParamTimerCreate(ParamTaskPtr *timer, TimerProcess process, void *context)
{
    PARAM_CHECK(timer != NULL && process != NULL, return -1, "Invalid timer");
    LibuvTimerTask *worker = (LibuvTimerTask *)CreateLibuvTask(sizeof(LibuvTimerTask), WORKER_TYPE_TIMER, 0, NULL);
    PARAM_CHECK(worker != NULL, return -1, "Failed to alloc timer worker");
    worker->base.worker.flags = WORKER_TYPE_TIMER;
    worker->timerProcess = process;
    worker->context = context;
    uv_timer_init(uv_default_loop(), &worker->timer);
    *timer = &worker->base.worker;
    return 0;
}

int ParamTimerStart(ParamTaskPtr timer, uint64_t timeout, uint64_t repeat)
{
    PARAM_CHECK(timer != NULL, return -1, "Invalid timer");
    if (timer->flags & WORKER_TYPE_TIMER) {
        LibuvTimerTask *worker = (LibuvTimerTask *)timer;
        uv_timer_start(&worker->timer, LibuvTimerCallback, timeout, repeat);
        return 0;
    }
    return -1;
}

int ParamServiceStart()
{
    if (uv_default_loop() == NULL) {
        return -1;
    }
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}

int ParamServiceStop(void)
{
    uv_fs_t req;
    uv_fs_unlink(uv_default_loop(), &req, PIPE_NAME, NULL);
    uv_stop(uv_default_loop());
    return 0;
}