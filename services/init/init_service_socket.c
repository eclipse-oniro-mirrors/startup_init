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
#include "init_service_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>

#include "init_log.h"
#include "init_service.h"
#include "securec.h"

#define HOS_SOCKET_DIR "/dev/unix/socket"
#define HOS_SOCKET_ENV_PREFIX "OHOS_SOCKET_"
#define MAX_SOCKET_ENV_PREFIX_LEN 64
#define MAX_SOCKET_FD_LEN 16

static int GetSocketAddr(struct sockaddr_un *addr, const char *name)
{
    (void)memset_s(addr, sizeof(struct sockaddr_un), 0x0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    size_t addrLen = sizeof(addr->sun_path);
    int ret = snprintf_s(addr->sun_path, addrLen, addrLen - 1, HOS_SOCKET_DIR "/%s", name);
    INIT_ERROR_CHECK(ret >= 0, return -1, "Failed to format addr %s", name);
    return 0;
}

static int CreateSocket(ServiceSocket *sockopt)
{
    INIT_ERROR_CHECK(sockopt != NULL, return SERVICE_FAILURE, "Invalid socket opt");
    if (sockopt->sockFd >= 0) {
        close(sockopt->sockFd);
        sockopt->sockFd = -1;
    }
    sockopt->sockFd = socket(PF_UNIX, sockopt->type, 0);
    INIT_ERROR_CHECK(sockopt->sockFd >= 0, return -1, "socket fail %d ", errno);

    struct sockaddr_un addr;
    int ret = GetSocketAddr(&addr, sockopt->name);
    INIT_ERROR_CHECK(ret == 0, return -1, "Failed to format addr %s", sockopt->name);

    do {
        ret = -1;
        if (access(addr.sun_path, F_OK) == 0) {
            INIT_LOGI("%s already exist, remove it", addr.sun_path);
            unlink(addr.sun_path);
        }
        if (sockopt->passcred) {
            int on = 1;
            if (setsockopt(sockopt->sockFd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on))) {
                break;
            }
        }
        if (bind(sockopt->sockFd, (struct sockaddr *)&addr, sizeof(addr))) {
            INIT_LOGE("Create socket for service %s failed: %d", sockopt->name, errno);
            break;
        }
        if (lchown(addr.sun_path, sockopt->uid, sockopt->gid)) {
            INIT_LOGE("lchown fail %d ", errno);
            break;
        }
        if (fchmodat(AT_FDCWD, addr.sun_path, sockopt->perm, AT_SYMLINK_NOFOLLOW)) {
            INIT_LOGE("fchmodat fail %d ", errno);
            break;
        }
        ret = 0;
    } while (0);
    if (ret != 0) {
        close(sockopt->sockFd);
        unlink(addr.sun_path);
        return -1;
    }
    INIT_LOGI("CreateSocket %s success", sockopt->name);
    return sockopt->sockFd;
}

static int SetSocketEnv(int fd, const char *name)
{
    INIT_ERROR_CHECK(name != NULL, return SERVICE_FAILURE, "Invalid name");
    char pubName[MAX_SOCKET_ENV_PREFIX_LEN] = { 0 };
    char val[MAX_SOCKET_FD_LEN] = { 0 };
    INIT_CHECK_RETURN_VALUE(snprintf_s(pubName, sizeof(pubName), sizeof(pubName) - 1, HOS_SOCKET_ENV_PREFIX "%s",
        name) >= 0, -1);
    INIT_CHECK_RETURN_VALUE(snprintf_s(val, sizeof(val), sizeof(val) - 1, "%d", fd) >= 0, -1);
    int ret = setenv(pubName, val, 1);
    INIT_ERROR_CHECK(ret >= 0, return -1, "setenv fail %d ", errno);
    fcntl(fd, F_SETFD, 0);
    return 0;
}

int CreateServiceSocket(ServiceSocket *sockopt)
{
    INIT_CHECK(sockopt != NULL, return 0);
    ServiceSocket *tmpSock = sockopt;
    while (tmpSock != NULL) {
        int fd = CreateSocket(tmpSock);
        INIT_CHECK_RETURN_VALUE(fd >= 0, -1);
        int ret = SetSocketEnv(fd, tmpSock->name);
        INIT_CHECK_RETURN_VALUE(ret >= 0, -1);
        tmpSock = tmpSock->next;
    }
    return 0;
}

void CloseServiceSocket(ServiceSocket *sockopt)
{
    INIT_CHECK(sockopt != NULL, return);
    struct sockaddr_un addr;
    ServiceSocket *tmpSock = sockopt;
    while (tmpSock != NULL) {
        if (sockopt->sockFd >= 0) {
            close(sockopt->sockFd);
            sockopt->sockFd = -1;
        }
        if (GetSocketAddr(&addr, sockopt->name) == 0) {
            unlink(addr.sun_path);
        }
        tmpSock = tmpSock->next;
    }
    return;
}
