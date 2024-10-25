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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "loop_event.h"
#include "le_socket.h"

#define PTY_BUF_SIZE 4096
#define PTY_PATH_SIZE 128

typedef struct Service_ {
    TaskHandle serverTask;
    struct ListNode head;
} Service;

typedef struct Task_ {
    TaskHandle task;
    struct ListNode item;
    int ptyFd;
    pid_t pid;
} Task;

typedef struct {
    uint16_t msgSize;
    uint16_t type;
    char ptyName[PTY_PATH_SIZE];
    char cmd[0]; // 数组长度为 0 是为了方便扩展
} Message;

typedef enum {
    ACTION_ONE = 0,
    ACTION_TWO,
    ACTION_THREE,
    ACTION_MAX
} ActionType;

#define SERVICE_SOCKET_PATH "/dev/unix/socket/test_service"
static Service g_service;
static LoopHandle g_controlFdLoop = NULL;

static void OnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen)
{
    if (buffer == NULL) {
        return;
    }
    Task *agent = (Task *)LE_GetUserData(task);
    if (agent == NULL) {
        printf("Can not get agent \n");
        return;
    }

    // parse msg to exec
    Message *msg = (Message *)buffer;
    if ((msg->type >= ACTION_MAX) || (msg->cmd[0] == '\0') || (msg->ptyName[0] == '\0')) {
        printf("Received invalid msg\n");
    }
    return;
}

static int SendMessage(LoopHandle loop, TaskHandle task, const char *message)
{
    if (message == NULL) {
        printf("Invalid parameter \n");
        return -1;
    }
    BufferHandle handle = NULL;
    uint32_t bufferSize = strlen(message) + 1;
    handle = LE_CreateBuffer(loop, bufferSize);

    char *buff = (char *)LE_GetBufferInfo(handle, NULL, &bufferSize);
    if (buff == NULL) {
        printf("Failed get buffer info \n");
        return -1;
    }

    int ret = memcpy_s(buff, bufferSize, message, strlen(message) + 1);
    if (ret != 0) {
        printf("Failed memcpy_s err=%d \n", errno);
        LE_FreeBuffer(g_controlFdLoop, task, handle);
        return -1;
    }

    LE_STATUS status = LE_Send(loop, task, handle, strlen(message) + 1);
    if (status != E_SUCCESS) {
        printf("Failed le send msg \n");
        return -1;
    }
    return 0;
}

static int OnIncommingConnect(const LoopHandle loop, const TaskHandle server)
{
    TaskHandle client = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.close = OnClose;
    info.disConnectComplete = NULL;
    info.sendMessageComplete = NULL;
    info.recvMessage = OnRecvMessage;
    info.baseInfo.userDataSize = sizeof(Task);
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT | TASK_TEST;
    int ret = LE_AcceptStreamClient(g_controlFdLoop, server, &client, &info);
    if (ret != 0) {
        printf("Failed accept stream \n");
        return -1;
    }
    Task *agent = (Task *)LE_GetUserData(client);
    if (agent == NULL) {
        printf("Invalid agent \n");
        return -1;
    }
    agent->task = client;
    OH_ListInit(&agent->item);
    ret = SendMessage(g_controlFdLoop, agent->task, "connect success.");
    if (ret != 0) {
        printf("Failed send msg \n");
        return -1;
    }
    OH_ListAddTail(&g_service.head, &agent->item);
    return 0;
}

void ServiceInit(const char *socketPath, LoopHandle loop)
{
    if ((socketPath == NULL) || (loop == NULL)) {
        return;
    }
    OH_ListInit(&g_service.head);
    LE_StreamServerInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_SERVER | TASK_PIPE;
    info.server = (char *)socketPath;
    info.socketId = -1;
    info.baseInfo.close = NULL;
    info.disConnectComplete = NULL;
    info.incommingConnect = OnIncommingConnect;
    info.sendMessageComplete = NULL;
    info.recvMessage = NULL;
    if (g_controlFdLoop == NULL) {
        g_controlFdLoop = loop;
    }
    (void)LE_CreateStreamServer(g_controlFdLoop, &g_service.serverTask, &info);
}
int main()
{
    ServiceInit(SERVICE_SOCKET_PATH, LE_GetDefaultLoop());
    LE_RunLoop(LE_GetDefaultLoop());
    return 0;
}