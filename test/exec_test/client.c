/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "init_log.h"
#include "init_socket.h"
#include "securec.h"

#define BUFFER_LENGTH 15
#define ARG_COUNT 2

int main(int argc, char* argv[])
{
    if (argc < ARG_COUNT) {
        INIT_LOGE("Client need an argument");
        return 0;
    }
    int sockFd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sockFd < 0) {
        INIT_LOGE("Failed to create client socket");
        return -1;
    }
    struct sockaddr_un addr;
    bzero(&addr, sizeof(struct sockaddr_un));
    addr.sun_family = PF_UNIX;
    size_t addrLen = sizeof(addr.sun_path);
    int ret = 0;
    if (strcmp(argv[1], "server") == 0) {
        ret = snprintf_s(addr.sun_path, addrLen, addrLen - 1, "/dev/unix/socket/serversock");
        INIT_ERROR_CHECK(ret >= 0, return -1, "Failed to format addr");
        if (connect(sockFd, (struct sockaddr *)&addr, sizeof(addr))) {
            INIT_LOGE("Failed to connect socket: %d", errno);
            return -1;
        }
    } else {
        INIT_LOGE("input error, invalid server name");
        return -1;
    }
    ret = write(sockFd, argv[ARG_COUNT], strlen(argv[ARG_COUNT]));
    if (ret < 0) {
        INIT_LOGE("Failed to write, errno = %d", errno);
    }
    close(sockFd);
    return 0;
}