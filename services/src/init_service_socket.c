/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "init_service_socket.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>

#define HOS_SOCKET_DIR    "/dev/unix/socket"
#define HOS_SOCKET_ENV_PREFIX    "OHOS_SOCKET_"

static int CreateSocket(struct ServiceSocket *sockopt)
{
    if (!sockopt || !sockopt->name) {
        return -1;
    }
    printf("[init] CreateSocket\n");
    int sockFd = socket(PF_UNIX, sockopt->type, 0);
    if (sockFd < 0) {
        printf("[init] socket fail %d \n", errno);
        return -1;
    }

    struct sockaddr_un addr;
    bzero(&addr,sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), HOS_SOCKET_DIR"/%s",
             sockopt->name);
    printf("[init] addr.sun_path is %s \n", addr.sun_path);
    if (access(addr.sun_path, F_OK)) {
        printf("[init] %s already exist, remove it\n", addr.sun_path);
        unlink(addr.sun_path);
    }
    if (sockopt->passcred) {
        int on = 1;
        if (setsockopt(sockFd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on))) {
            unlink(addr.sun_path);
            close(sockFd);
            return -1;
        }
    }

    if (bind(sockFd, (struct sockaddr *)&addr, sizeof(addr))) {
        printf("[Init] Create socket for service %s failed: %d\n", sockopt->name, errno);
        unlink(addr.sun_path);
        close(sockFd);
        return -1;
    }
    printf("[init] bind socket success\n");
    if (lchown(addr.sun_path, sockopt->uid, sockopt->gid)) {
        unlink(addr.sun_path);
        close(sockFd);
        printf("[init] lchown fail %d \n", errno);
        return -1;
    }

    if (fchmodat(AT_FDCWD, addr.sun_path, sockopt->perm, AT_SYMLINK_NOFOLLOW)) {
        unlink(addr.sun_path);
        close(sockFd);
        printf("[Init] fchmodat fail %d \n", errno);
        return -1;
    }
    printf("[Init] CreateSocket success \n");
    return sockFd;
}

static int SetSocketEnv(int fd, char *name)
{
    printf("[init] SetSocketEnv\n");
    char pubName[64] = {0};
    char val[16] = {0};
    snprintf(pubName, sizeof(pubName), HOS_SOCKET_ENV_PREFIX"%s", name);
    printf("[init] pubName is %s \n", pubName);
    snprintf(val, sizeof(val), "%d", fd);
    printf("[init] val is %s \n", val);
    int ret = setenv(pubName, val, 1);
    if (ret < 0) {
        printf("[init] setenv fail %d \n", errno);
        return -1;
    }
    fcntl(fd, F_SETFD, 0);
    return 0;
}

int DoCreateSocket(struct ServiceSocket *sockopt)
{
    if (!sockopt) {
        return -1;
    }
    printf("[init] DoCreateSocket \n");
    struct ServiceSocket *tmpSock = sockopt;
    while (tmpSock) {
        printf("[init] tmpSock %p \n", tmpSock);
        int fd = CreateSocket(tmpSock);
        if (fd < 0) {
            return -1;
        }
        int ret = SetSocketEnv(fd, tmpSock->name);
        if (ret < 0) {
            return -1;
        }
        tmpSock = tmpSock->next;
    }
    return 0;
}

int GetControlFromEnv(char *path)
{
    if (path == NULL) {
        return -1;
    }
     char *cp = path;
    while (*cp) {
        if (!isalnum(*cp)) *cp = '_';
        ++cp;
    }
    const char *val = getenv(path);
    if (val == NULL) {
        return -1;
    }
    errno = 0;
    int fd = strtol(val, NULL, 10);
    if (errno) {
        return -1;
    }
    if (fcntl(fd, F_GETFD) < 0) {
        return -1;
    }
    return fd;
}

int GetControlSocket(const char *name)
{
    if (name == NULL) {
        return -1;
    }
    char path[128] = {0};
    snprintf(path, sizeof(path), HOS_SOCKET_ENV_PREFIX"%s", name);
    int fd = GetControlFromEnv(path);

    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    int ret = getsockname(fd, (struct sockaddr*)&addr, &addrlen);
    if (ret < 0) {
        return -1;
    }
    char sockDir[128] = {0};
    snprintf(sockDir, sizeof(sockDir), HOS_SOCKET_DIR"/%s", name);
    if (strncmp(sockDir, addr.sun_path, strlen(sockDir)) == 0) {
        return fd;
    }
    return -1;
}

