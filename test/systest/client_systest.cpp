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

#define TASKINFO \
    uint32_t flags; \
    union { \
        int fd; \
    } taskId

typedef struct {
    TASKINFO;
} TaskId;

typedef struct LiteTask_ {
    TASKINFO;
    HashNode hashNode;
    LE_Close close;
    DumpTaskInfo dumpTaskInfo;
    HandleTaskEvent handleEvent;
    HandleTaskClose innerClose;
    uint16_t userDataOffset;
    uint16_t userDataSize;
} BaseTask;

typedef struct {
    BaseTask base;
    LE_IncommingConnect incommingConnect;
    char server[0];
} StreamServerTask;

typedef struct MyTask_ {
    TaskHandle task;
    struct ListNode item;
    int ptyFd;
    pid_t pid;
} MyTask;

static LoopHandle g_loop = NULL;

static int HandleServerEvent(const LoopHandle loopHandle, const TaskHandle serverTask, uint32_t oper)
{
    printf("HandleServerEvent_ fd %d oper 0x%x \n", GetSocketFd(serverTask), oper);
    if (!LE_TEST_FLAGS(oper, EVENT_READ)) {
        return LE_FAILURE;
    }

    StreamServerTask *server = (StreamServerTask *)serverTask;
    if (server->incommingConnect == NULL) {
        return 0;
    }

    int ret = server->incommingConnect(loopHandle, serverTask);
    if (ret != 0) {
        printf("HandleServerEvent_ fd %d do not accept socket \n", GetSocketFd(serverTask));
    }

    EventLoop *loop = (EventLoop *)loopHandle;
    loop->modEvent(loop, (const BaseTask *)serverTask, EVENT_READ);
    return 0;
}

static void HandleStreamTaskClose(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
    BaseTask *task = (BaseTask *)taskHandle;
    DelTask((EventLoop *)loopHandle, task);
    CloseTask(loopHandle, task);
    if (task->taskId.fd > 0) {
        printf("HandleStreamTaskClose::close fd:%d \n", task->taskId.fd);
        close(task->taskId.fd);
    }
}

static void DumpStreamServerTaskInfo(const TaskHandle task)
{
    if (task == nullptr) {
        printf("Dump empty task \n");
        return;
    }
    BaseTask *baseTask = dynamic_cast<BaseTask *>(task);
    StreamServerTask *serverTask = dynamic_cast<StreamServerTask *>(baseTask);

    printf("\tfd: %d \n", serverTask->base.taskId.fd);
    printf("\t  TaskType: %s \n", "ServerTask");
    if (strlen(serverTask->server) > 0) {
        printf("\t  Server socket:%s \n", serverTask->server);
    } else {
        printf("\t  Server socket:%s \n", "NULL");
    }
}

static int IncommingConnect(const LoopHandle loop, const TaskHandle server)
{
    TaskHandle client = NULL;
    LE_StreamInfo info = {};
#ifndef STARTUP_INIT_TEST
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
#else
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT | TASK_TEST;
#endif
    info.baseInfo.close = OnClose;
    info.baseInfo.userDataSize = sizeof(MyTask);
    info.disConnectComplete = NULL;
    info.sendMessageComplete = NULL;
    info.recvMessage = CmdOnRecvMessage;
    int ret = LE_AcceptStreamClient(g_loop, server, &client, &info);
    if (ret != 0) {
        printf("Failed accept stream \n");
        return -1;
    }
    MyTask *agent = (MyTask *)LE_GetUserData(client);
    if (agent == nullptr) {
        printf("Invalid agent \n");
        return -1;
    }
    agent->task = client;
    OH_ListInit(&agent->item);
    ret = SendMessage(g_loop, agent->task, "connect success.");
    if (ret != 0) {
        printf("Failed send msg \n");
        return -1;
    }
    OH_ListAddTail(&g_cmdService.head, &agent->item);
    return 0;
}

int main()
{
    printf("请输入创建socket的类型：(pipe, tcp)\n");
    std::string socket_type;
    std::cin >> socket_type;

    int type;
    char *server;
    std::string path;
    if (socket_type == "pipe") {
        type = TASK_STREAM | TASK_PIPE |TASK_SERVER | TASK_TEST;
        path = "/data/testpipe";
    } else if (socket_type == "tcp") {
        type = TASK_STREAM | TASK_TCP |TASK_SERVER | TASK_TEST;
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
    LoopHandle loopHandle = LE_GetDefaultLoop();
    EventLoop *loop = (EventLoop *)loopHandle;
    StreamServerTask *task = (StreamServerTask *)CreateTask(loopHandle, fd, &baseInfo,
        sizeof(StreamServerTask) + strlen(server) + 1);
    if (task == nullptr) {
        printf("Failed to create task \n");
        close(fd);
        system("pause");
        return 0;
    }

+   TaskHandle *taskHandle = NULL;
    task->base.handleEvent = HandleServerEvent;
    task->base.innerClose = HandleStreamTaskClose;
    task->base.dumpTaskInfo = DumpStreamServerTaskInfo;
    task->incommingConnect = incommingConnect;
    loop->addEvent(loop, (const BaseTask *)task, EVENT_READ);
    int ret = memcpy_s(task->server, strlen(server) + 1, server, strlen(server) + 1);
    if (ret != 0) {
        printf("Failed to copy server name %s", server);
        system("pause");
        return 0;
    }
    
    *taskHandle = (TaskHandle)task;
}