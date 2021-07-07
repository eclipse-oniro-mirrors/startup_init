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
#include "init_log.h"

#define HOS_SOCKET_DIR    "/dev/unix/socket"
#define HOS_SOCKET_ENV_PREFIX    "OHOS_SOCKET_"

static int CreateSocket(struct ServiceSocket *sockopt)
{
    if (!sockopt || !sockopt->name) {
        return -1;
    }
    if (sockopt->sockFd >= 0) {
        close(sockopt->sockFd);
        sockopt->sockFd = -1;
    }
    sockopt->sockFd = socket(PF_UNIX, sockopt->type, 0);
    if (sockopt->sockFd < 0) {
        INIT_LOGE("socket fail %d \n", errno);
        return -1;
    }

    struct sockaddr_un addr;
    bzero(&addr,sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), HOS_SOCKET_DIR"/%s",
             sockopt->name);
    if (access(addr.sun_path, F_OK)) {
        INIT_LOGE("%s already exist, remove it\n", addr.sun_path);
        if (unlink(addr.sun_path) != 0) {
            INIT_LOGE("ulink fail err %d \n", errno);
        }
    }
    if (sockopt->passcred) {
        int on = 1;
        if (setsockopt(sockopt->sockFd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on))) {
            unlink(addr.sun_path);
            close(sockopt->sockFd);
            return -1;
        }
    }

    if (bind(sockopt->sockFd, (struct sockaddr *)&addr, sizeof(addr))) {
        INIT_LOGE("Create socket for service %s failed: %d\n", sockopt->name, errno);
        unlink(addr.sun_path);
        close(sockopt->sockFd);
        return -1;
    }

    if (lchown(addr.sun_path, sockopt->uid, sockopt->gid)) {
        unlink(addr.sun_path);
        close(sockopt->sockFd);
        INIT_LOGE("lchown fail %d \n", errno);
        return -1;
    }

    if (fchmodat(AT_FDCWD, addr.sun_path, sockopt->perm, AT_SYMLINK_NOFOLLOW)) {
        unlink(addr.sun_path);
        close(sockopt->sockFd);
        INIT_LOGE("fchmodat fail %d \n", errno);
        return -1;
    }
    INIT_LOGI("CreateSocket success \n");
    return sockopt->sockFd;
}

static int SetSocketEnv(int fd, char *name)
{
    char pubName[64] = {0};
    char val[16] = {0};
    snprintf(pubName, sizeof(pubName), HOS_SOCKET_ENV_PREFIX"%s", name);
    snprintf(val, sizeof(val), "%d", fd);
    int ret = setenv(pubName, val, 1);
    if (ret < 0) {
        INIT_LOGE("setenv fail %d \n", errno);
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
    struct ServiceSocket *tmpSock = sockopt;
    while (tmpSock) {
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

