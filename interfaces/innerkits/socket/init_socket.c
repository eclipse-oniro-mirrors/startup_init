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

#include "init_socket.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "beget_ext.h"
#include "securec.h"

#define N_DEC 10
#define MAX_SOCKET_ENV_PREFIX_LEN 64
#define MAX_SOCKET_DIR_LEN 128

static int GetControlFromEnv(const char *path, int length)
{
    BEGET_CHECK_RETURN_VALUE(path != NULL && length > 0, -1);
    const char *val = getenv(path);
    BEGET_ERROR_CHECK(val != NULL, return -1, "Get environment from %s failed", path);
    errno = 0;
    int fd = strtol(val, NULL, N_DEC);
    BEGET_ERROR_CHECK(errno == 0, return -1, "Failed strtol err=%d", errno);
    BEGET_ERROR_CHECK(fcntl(fd, F_GETFD) >= 0, return -1, "Failed fcntl err=%d ", errno);
    return fd;
}

int GetControlSocket(const char *name)
{
    BEGET_CHECK_RETURN_VALUE(name != NULL, -1);
    char path[MAX_SOCKET_ENV_PREFIX_LEN] = {0};
    BEGET_CHECK_RETURN_VALUE(snprintf_s(path, sizeof(path), sizeof(path) - 1, OHOS_SOCKET_ENV_PREFIX"%s",
        name) != -1, -1);
    int fd = GetControlFromEnv(path, MAX_SOCKET_ENV_PREFIX_LEN);
    BEGET_ERROR_CHECK(fd >= 0, return -1, "Get control fd from environment failed");
    int addrFamily = 0;
    socklen_t afLen = sizeof(addrFamily);
    int ret = getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &addrFamily, &afLen);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Get socket option fail, err=%d ", errno);
    if (addrFamily != AF_UNIX) {
        return fd;
    }
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    ret = getsockname(fd, (struct sockaddr*)&addr, &addrlen);
    BEGET_ERROR_CHECK(ret >= 0, return -1, "Failed getsockname err=%d ", errno);
    char sockDir[MAX_SOCKET_DIR_LEN] = {0};
    BEGET_CHECK_RETURN_VALUE(snprintf_s(sockDir, sizeof(sockDir), sizeof(sockDir) - 1, OHOS_SOCKET_DIR"/%s",
        name) != -1, -1);
    BEGET_LOGV("Compary sockDir %s and addr.sun_path %s", sockDir, addr.sun_path);
    if (strncmp(sockDir, addr.sun_path, strlen(sockDir)) == 0) {
        return fd;
    }
    return -1;
}
