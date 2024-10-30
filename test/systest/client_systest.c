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
#include "le_task.h"
#include "le_utils.h"
#include "le_epoll.h"
#include "le_idle.h"
#include "le_timer.h"

static int SetSockTimeout(int fd)
{
    struct timeval timeout;
    timeout.tv_sec = SOCKET_TIMEOUT;
    timeout.tv_usec = 0;
    int ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (ret != 0) {
        printf("Failed to set socket option. \n");
        return ret;
    }

    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (ret != 0) {
        printf("Failed to set socket option. \n");
    }
    return ret;
}

static int GetSockaddrFromServer(const char *server, struct sockaddr_in *addr)
{
    int ret = memset_s(addr, sizeof(struct sockaddr_in), 0, sizeof(struct sockaddr_in));
    if (ret != 0) {
        printf("Failed to memory set. error: %s \n", strerror(errno));
        return ret;
    }

    addr->sin_family = AF_INET;
    const char *portStr = strstr(server, ":");
    if (portStr == NULL) {
        printf("Failed to get addr %s \n", server);
        return -1;
    }
    uint16_t port = atoi(portStr + 1);
    addr->sin_port = htons(port);
    ret = inet_pton(AF_INET, server, &addr->sin_addr);
    if (ret < 0) {
        printf("Failed to inet_pton addr %s \n", server);
        return -1;
    }

    return 0;
}

static int CreateTcpSocket(const char *server)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd <= 0) {
        printf("Failed to create socket. \n");
        return fd;
    }
    SetNoBlock(fd);

    int on = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
    if (ret != 0) {
        printf("Failed to set socket option. \n");
        return -1;
    }

    ret = SetSockTimeout(fd);
    if (ret != 0) {
        printf("Failed to set socket timeout. \n");
        return -1;
    }

    struct sockaddr_in serverAddr;
    ret = GetSockaddrFromServer(server, &serverAddr);
    if (ret != 0) {
        printf("Failed to get addr. \n");
        return -1;
    }
    ret = connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        printf("Failed to connect socket errno:%d \n", errno);
        return -1;
    }

    printf("CreateTcpSocket connect fd: %d \n", fd);
    return fd;
}

static int CreatePipeSocket(const char *server)
{
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd <= 0) {
        printf("Failed to create socket. \n");
        return fd;
    }
    SetNoBlock(fd);

    int on = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
    if (ret != 0) {
        printf("Failed to set socket option. \n");
        return -1;
    }

    ret = SetSockTimeout(fd);
    if (ret != 0) {
        printf("Failed to set socket timeout. \n");
        return -1;
    }

    struct sockaddr_in serverAddr;
    ret = memset_s(&serverAddr, sizeof(serverAddr), 0, sizeof(serverAddr));
    if (ret != 0) {
        printf("Failed to memset_s serverAddr. \n");
        close(fd);
        return ret;
    }
    serverAddr.sun_family = AF_UNIX;
    ret = strcpy_s(serverAddr.sun_path, sizeof(serverAddr.sun_path), server);
    if (ret != 0) {
        printf("Failed to strcpy_s sun_path. \n");
        close(fd);
        return ret;
    }
    uint32_t size = offsetof(struct sockaddr_un, sun_path) + strlen(serverAddr.sun_path);
    ret = connect(fd, (struct sockaddr *)&serverAddr, size);
    if (ret < 0) {
        printf("Failed to connect socket errno:%d \n", errno);
        close(fd);
        return -1;
    }

    printf("CreatePipeSocket connect fd: %d server: %s \n", fd, serverAddr.sun_path);
    return fd;
}

int CreateSocket(int flags, const char *server)
{
    int fd = -1;
    int type = flags & 0x0000ff00;
    LE_LOGV("CreateSocket flags %x type %x server %s", flags, type, server);
    if (type == TASK_TCP) {
        fd = CreateTcpSocket(server);
    } else if (type == TASK_PIPE) {
        fd = CreatePipeSocket(server);
    }
    if (fd <= 0) {
        printf("Invalid flags 0x%08x for server %s \n", flags, server);
        return -1;
    }
    return fd;
}

int SendMessage(LoopHandle loop, TaskHandle task, const char *message)
{
    if (message == NULL) {
        printf("Invalid parameter. \n");
        return -1;
    }
    BufferHandle handle = NULL;
    uint32_t bufferSize = strlen(message) + 1;
    handle = LE_CreateBuffer(loop, bufferSize);
    char *buff = (char *)LE_GetBufferInfo(handle, NULL, &bufferSize);
    if (buff == NULL) {
        printf("Invalid parameter. \n");
        return -1;
    }
    int ret = memcpy_s(buff, bufferSize, message, strlen(message) + 1);
    if (ret != 0) {
        LE_FreeBuffer(g_controlFdLoop, task, handle);
        printf("Failed memcpy_s err=%d \n", errno);
        return -1;
    }
    int status = LE_Send(loop, task, handle, strlen(message) + 1);
    if (status != 0) {
        printf("Failed le send msg. \n");
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 1) {
        return 0;
    }

    printf("请输入创建socket的类型：(pipe, tcp)\n");
    char type[128];
    int ret = scanf_s("%s", type, sizeof(type));
    if (ret <= 0) {
        printf("input error \n");
        return 0;
    }

    int flags;
    char *server;
    if (strcmp(type, "pipe") == 0) {
        flags = TASK_STREAM | TASK_PIPE |TASK_SERVER | TASK_TEST;
        server = (char *)"/data/testpipe";
    } else if (strcmp(type, "tcp") == 0) {
        flags = TASK_STREAM | TASK_TCP |TASK_SERVER | TASK_TEST;
        server = (char *)"127.0.0.1:7777";
    } else {
        printf("输入有误，请输入pipe或者tcp!");
        system("pause");
        return 0;
    }

    int fd = CreateSocket(flags, server);
    if (fd == -1) {
        printf("Create socket failed!\n");
        system("pause");
        return 0;
    }

    EventEpoll *epoll = (EventEpoll *)LE_GetDefaultLoop();
    if (epoll == NULL || epoll->epollFd < 0) {
        printf("Invalid epoll. \n");
        return 0;
    }
}