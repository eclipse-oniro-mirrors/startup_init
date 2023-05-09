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

#include "le_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "le_utils.h"

static int CreatePipeServerSocket_(const char *server, int maxClient)
{
    int listenfd = socket(PF_UNIX, SOCK_STREAM, 0);
    LE_CHECK(listenfd > 0, return listenfd, "Failed to create socket errno %d", errno);

    unlink(server);
    struct sockaddr_un serverAddr;
    int ret = memset_s(&serverAddr, sizeof(serverAddr), 0, sizeof(serverAddr));
    LE_CHECK(ret == 0, close(listenfd);
        return ret, "Failed to memory set. error: %d", errno);
    serverAddr.sun_family = AF_UNIX;
    ret = strcpy_s(serverAddr.sun_path, sizeof(serverAddr.sun_path), server);
    LE_CHECK(ret == 0, close(listenfd);
        return ret, "Failed to copy.  error: %d", errno);
    uint32_t size = offsetof(struct sockaddr_un, sun_path) + strlen(server);
    ret = bind(listenfd, (struct sockaddr *)&serverAddr, size);
    LE_CHECK(ret >= 0, close(listenfd);
        return ret, "Failed to bind socket.  error: %d", errno);

    SetNoBlock(listenfd);
    ret = listen(listenfd, maxClient);
    LE_CHECK(ret >= 0, close(listenfd);
        return ret, "Failed to listen socket  error: %d", errno);
    ret = chmod(server, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    LE_CHECK(ret == 0, return -1, "Failed to chmod %s, err %d. ", server, errno);
    LE_LOGV("CreatePipeSocket listen fd: %d server:%s ", listenfd, serverAddr.sun_path);
    return listenfd;
}

static int CreatePipeSocket_(const char *server)
{
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    LE_CHECK(fd > 0, return fd, "Failed to create socket");
    SetNoBlock(fd);

    int on = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
    LE_CHECK(ret == 0, return ret, "Failed to set socket option");

    struct sockaddr_un serverAddr;
    ret = memset_s(&serverAddr, sizeof(serverAddr), 0, sizeof(serverAddr));
    LE_CHECK(ret == 0, close(fd);
        return ret, "Failed to memset_s serverAddr");
    serverAddr.sun_family = AF_UNIX;
    ret = strcpy_s(serverAddr.sun_path, sizeof(serverAddr.sun_path), server);
    LE_CHECK(ret == 0, close(fd);
        return ret, "Failed to strcpy_s sun_path");
    uint32_t size = offsetof(struct sockaddr_un, sun_path) + strlen(serverAddr.sun_path);
    ret = connect(fd, (struct sockaddr *)&serverAddr, size);
    LE_CHECK(ret >= 0, close(fd);
        return ret, "Failed to connect socket");
    LE_LOGV("CreatePipeSocket connect fd: %d server: %s ", fd, serverAddr.sun_path);
    return fd;
}

static LE_STATUS GetSockaddrFromServer_(const char *server, struct sockaddr_in *addr)
{
    int ret = memset_s(addr, sizeof(struct sockaddr_in), 0, sizeof(struct sockaddr_in));
    LE_CHECK(ret == 0, return ret, "Failed to memory set. error: %s", strerror(errno));
    addr->sin_family = AF_INET;
    const char *portStr = strstr(server, ":");
    LE_CHECK(portStr != NULL, return LE_FAILURE, "Failed to get addr %s", server);
    uint16_t port = atoi(portStr + 1);
    addr->sin_port = htons(port);
    ret = inet_pton(AF_INET, server, &addr->sin_addr);
    LE_CHECK(ret >= 0, return LE_FAILURE, "Failed to inet_pton addr %s", server);
    return LE_SUCCESS;
}

static int CreateTcpServerSocket_(const char *server, int maxClient)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    LE_CHECK(listenfd > 0, return listenfd, "Failed to create socket");

    struct sockaddr_in serverAddr;
    GetSockaddrFromServer_(server, &serverAddr);
    int ret = bind(listenfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    LE_CHECK(ret >= 0, close(listenfd);
        return ret, "Failed to bind socket");
    SetNoBlock(listenfd);

    ret = listen(listenfd, maxClient);
    LE_CHECK(ret >= 0, close(listenfd);
        return ret, "Failed to listen socket");
    return listenfd;
}

static int CreateTcpSocket_(const char *server)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    LE_CHECK(fd > 0, return fd, "Failed to create socket");
    SetNoBlock(fd);

    int on = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
    LE_CHECK(ret == 0, return ret, "Failed to set socket option");
    struct sockaddr_in serverAddr;
    GetSockaddrFromServer_(server, &serverAddr);
    LE_LOGV("CreateTcpSocket connect: %s ", server);
    ret = connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    LE_CHECK(ret >= 0, close(fd);
        return ret, "Failed to connect socket");
    return fd;
}

static int AcceptPipeSocket_(int serverFd)
{
    struct sockaddr_un clientAddr;
    socklen_t addrlen = sizeof(clientAddr);
    bzero(&clientAddr, addrlen);
    int fd = accept(serverFd, (struct sockaddr *)&clientAddr, &addrlen);
    LE_CHECK(fd >= 0, return fd, "Failed to accept socket");
    LE_LOGV("AcceptPipeSocket client fd %d %s ", fd, clientAddr.sun_path);
    return fd;
}

static int AcceptTcpSocket_(int serverFd)
{
    struct sockaddr_in clientAddr;
    socklen_t addrlen = sizeof(clientAddr);
    bzero(&clientAddr, addrlen);
    int fd = accept(serverFd, (struct sockaddr *)&clientAddr, &addrlen);
    LE_CHECK(fd >= 0, return fd, "Failed to accept socket");
    LE_LOGV("AcceptTcpSocket_ client: %s ", inet_ntoa(clientAddr.sin_addr));
    return fd;
}
INIT_LOCAL_API
int CreateSocket(int flags, const char *server)
{
    int fd = -1;
    int type = flags & 0x0000ff00;
    LE_LOGV("CreateSocket flags %x type %x server %s", flags, type, server);
    if (type == TASK_TCP) {
        if (LE_TEST_FLAGS(flags, TASK_SERVER)) {
            fd = CreateTcpServerSocket_(server, LOOP_MAX_CLIENT);
        } else if (LE_TEST_FLAGS(flags, TASK_CONNECT)) {
            fd = CreateTcpSocket_(server);
        }
    } else if (type == TASK_PIPE) {
        if (LE_TEST_FLAGS(flags, TASK_SERVER)) {
            fd = CreatePipeServerSocket_(server, LOOP_MAX_CLIENT);
        } else if (LE_TEST_FLAGS(flags, TASK_CONNECT)) {
            fd = CreatePipeSocket_(server);
        }
    }
    if (fd <= 0) {
        LE_LOGE("Invalid flags 0x%08x for server %s", flags, server);
        return -1;
    }
    return fd;
}
INIT_LOCAL_API
int AcceptSocket(int fd, int flags)
{
    int clientFd = -1;
    int type = flags & 0x0000ff00;
    if (type == TASK_TCP) {
        clientFd = AcceptTcpSocket_(fd);
    } else if (type == TASK_PIPE) {
        clientFd = AcceptPipeSocket_(fd);
    } else {
        LE_LOGE("AcceptSocket invalid flags %#8x ", flags);
        return -1;
    }
    SetNoBlock(clientFd);
    return clientFd;
}
