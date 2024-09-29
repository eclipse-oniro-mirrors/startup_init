/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <string>
#include <iostream>

#include <linux/in.h>
#include <linux/socket.h>
#include <linux/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include "le_streamtask.c"
#include "loop_systest.h"
#include "loop_event.h"
#include "le_socket.h"
#include "le_task.h"
#include "list.h"

static LE_STATUS HandleEvent(const LoopHandle loopHandle, const TaskHandle handle, uint32_t oper)
{
    StreamConnectTask *task = (StreamConnectTask *)handle;
    printf("HandleEvent fd:%d oper 0x%x \n", GetSocketFd(handle), oper);

    LE_STATUS status = LE_SUCCESS;
    if (LE_TEST_FLAGS(oper, EVENT_WRITE)) {
        status = HandleSendMsg(loopHandle, handle, task->sendMessageComplete);
    }
    if (LE_TEST_FLAGS(oper, EVENT_READ)) {
        status = HandleRecvMsg(loopHandle, handle, task->recvMessage, task->handleRecvMsg);
    }
    if (LE_TEST_FLAGS(oper, EVENT_ERROR)) {
        if (task->disConnectComplete) {
            task->disConnectComplete(handle);
        }
        LE_CloseStreamTask(loopHandle, handle);
    }
    return status;
}

static LE_STATUS HandleSendMsg(const LoopHandle loopHandle,
    const TaskHandle taskHandle, const LE_SendMessageComplete complete)
{
    EventLoop *loop = (EventLoop *)loopHandle;
    StreamTask *task = (StreamTask *)taskHandle;
    LE_Buffer *buffer = GetFirstBuffer(task);
    while (buffer) {
        int ret = write(GetSocketFd(taskHandle), buffer->data, buffer->dataSize);
        if (ret < buffer->dataSize) {
            printf("fd:%d size %d, err:%d \n", GetSocketFd(taskHandle), buffer->dataSize, errno);
        }
        printf("HandleSendMsg fd:%d send data size %d %d \n", GetSocketFd(taskHandle), buffer->dataSize, ret);
        buffer->result = (ret == (int)buffer->dataSize) ? 0 : errno;
        if (complete != NULL) {
            complete(taskHandle, buffer);
        }
        FreeBuffer(loopHandle, task, buffer);
        buffer = GetFirstBuffer(task);
    }
    if (IsBufferEmpty(task)) {
        printf("HandleSendMsg fd:%d empty wait read \n", GetSocketFd(taskHandle));
        loop->modEvent(loop, (const BaseTask *)taskHandle, EVENT_READ);
        return LE_SUCCESS;
    }
    return LE_SUCCESS;
}

static LE_STATUS HandleClientEvent(const LoopHandle loopHandle, const TaskHandle handle, uint32_t oper)
{
    StreamClientTask *client = (StreamClientTask *)handle;
    printf("HandleClientEvent fd:%d oper 0x%x \n", GetSocketFd(handle), oper);

    LE_STATUS status = LE_SUCCESS;
    if (LE_TEST_FLAGS(oper, EVENT_WRITE)) {
        if (client->connected == 0 && client->connectComplete) {
            client->connectComplete(handle);
        }
        client->connected = 1;
        status = HandleSendMsg(loopHandle, handle, client->sendMessageComplete);
    }
    if (LE_TEST_FLAGS(oper, EVENT_READ)) {
        status = HandleRecvMsg(loopHandle, handle, client->recvMessage, client->handleRecvMsg);
    }
    if (status == LE_DIS_CONNECTED) {
        if (client->disConnectComplete) {
            client->disConnectComplete(handle);
        }
        client->connected = 0;
        LE_CloseStreamTask(loopHandle, handle);
    }
    return status;
}

static void HandleTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    BaseTask *baseTask = (BaseTask *)taskHandle;
    printf("HandleTaskClose::DelTask \n");
    DelTask((EventLoop *)loopHandle, baseTask);

    printf("HandleTaskClose::DCloseTask \n");
    CloseTask(loopHandle, task);

    if (baseTask->taskId.fd > 0) {
        printf("HandleTaskClose fd: %d \n", ask->taskId.fd);
        close(baseTask->taskId.fd);
    }
}

static void DumpConnectTaskInfo(const TaskHandle task)
{
    if (task == NULL) {
        printf("task is null \n");
        return;
    }
    
    BaseTask *baseTask = (BaseTask *)task;
    StreamConnectTask *connectTask = (StreamConnectTask *)baseTask;
    TaskHandle taskHandle = (TaskHandle)connectTask;
    printf("\tfd: %d \n", connectTask->stream.base.taskId.fd);
    printf("\t  TaskType: %s \n", "ConnectTask");
    printf("\t  ServiceInfo: \n");

    struct ucred cred = {-1, -1, -1};
    socklen_t credSize  = sizeof(struct ucred);
    if (getsockopt(LE_GetSocketFd(taskHandle), SOL_SOCKET, SO_PEERCRED, &cred, &credSize) == 0) {
        printf("\t      Service Pid: %d \n", cred.pid);
        printf("\t      Service Uid: %d \n", cred.uid);
        printf("\t      Service Gid: %d \n", cred.gid);
    } else {
        printf("\t      Service Pid: %s \n", "NULL");
        printf("\t      Service Uid: %s \n", "NULL");
        printf("\t      Service Gid: %s \n", "NULL");
    }
}

int AcceptClient(const LoopHandle loopHandle, const TaskHandle server,
    TaskHandle *taskHandle, const LE_StreamInfo *info)
{
    if (loopHandle == NULL || info == NULL) {
        printf("Invalid parameters \n");
        return -1;
    }
    if (server == NULL || taskHandle == NULL) {
        printf("Invalid parameters \n");
        return -1;
    }
    if (info->recvMessage == NULL) {
        printf("Invalid parameters recvMessage \n");
        return -1;
    }

    int fd = -1;
    if ((info->baseInfo.flags & TASK_TEST) != TASK_TEST) {
        fd = AcceptSocket(GetSocketFd(server), info->baseInfo.flags);
        if (fd <= 0) {
            printf("Failed to accept socket %d \n", GetSocketFd(server));
            return -1;
        }
    }
    StreamConnectTask *task = (StreamConnectTask *)CreateTask(
        loopHandle, fd, &info->baseInfo, sizeof(StreamConnectTask));
    if (task == NULL) {
        printf("Failed to create task %d \n", GetSocketFd(server));
        close(fd);
        return -1;
    }

    task->stream.base.handleEvent = HandleEvent;
    task->stream.base.innerClose = HandleTaskClose;
    task->stream.base.dumpTaskInfo = DumpConnectTaskInfo;
    task->disConnectComplete = info->disConnectComplete;
    task->sendMessageComplete = info->sendMessageComplete;
    task->serverTask = (StreamServerTask *)server;
    task->handleRecvMsg = info->handleRecvMsg;
    task->recvMessage = info->recvMessage;
    
    OH_ListInit(&task->stream.buffHead);
    LoopMutexInit(&task->stream.mutex);
    if ((info->baseInfo.flags & TASK_TEST) != TASK_TEST) {
        EventLoop *loop = (EventLoop *)loopHandle;
        loop->addEvent(loop, (const BaseTask *)task, EVENT_READ);
    }
    *taskHandle = (TaskHandle)task;
    return 0;
}

int main()
{
    printf("请输入创建socket的类型：(pipe, tcp)\n");
    std::string socket_type;
    std::cin >> socket_type;

    int type;
    std::string path;
    if (socket_type == "pipe") {
        type = TASK_STREAM | TASK_PIPE |TASK_SERVER;
        path = "/data/testpipe";
    } else if (socket_type == "tcp") {
        type = TASK_STREAM | TASK_TCP |TASK_SERVER;
        path = "127.0.0.1:7777";
    } else {
        printf("输入有误，请输入pipe或者tcp!");
        system("pause");
        return 0;
    }

    char *server = path.c_str();
    int fd = CreateSocket(type, server);
    if (fd == -1) {
        printf("Create socket failed!\n");
        system("pause");
        return 0;
    }

    int ret = listenSocket(fd, type, server);
    if (ret != 0) {
        printf("Failed to listen socket %s\n", server);
        system("pause");
        return 0;
    }

    while (true) {
        int clientfd = AcceptSocket(fd, type);
        if (clientfd == -1) {
            printf("Failed to accept socket %s\n", server);
            continue;
        }

        std::cout << "Accept connection from %d" << clientfd << std::endl;
        break;
    }
    return 0;
}