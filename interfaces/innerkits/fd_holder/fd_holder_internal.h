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

#ifndef BASE_STARTUP_INITLITE_FD_HOLDER_INTERNAL_H
#define BASE_STARTUP_INITLITE_FD_HOLDER_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INIT_HOLDER_SOCKET_PATH "/dev/unix/socket/fd_holder"
#define MAX_HOLD_FDS (64)
#define MAX_FD_HOLDER_BUFFER (MAX_HOLD_FDS * sizeof(int))

#define ENV_FD_HOLD_PREFIX "OHOS_FD_HOLD_"

// actions
#define ACTION_HOLD "hold"
#define ACTION_GET "get"

// poll or not
#define WITHPOLL "poll"
#define WITHOUTPOLL "withoutpoll"

int BuildControlMessage(struct msghdr *msghdr,  int *fds, int fdCount, bool sendUcred);
// This function will allocate memory to store FDs
// Remember to delete when not used anymore.
int *ReceiveFds(int sock, struct iovec iovec, size_t *outFdCount, bool nonblock, pid_t *requestPid);

#define CMSG_BUFFER_TYPE(size)                                  \
    union {                                                     \
        struct cmsghdr cmsghdr;                                 \
        uint8_t buf[size];                                      \
        uint8_t align_check[(size) >= CMSG_SPACE(0) &&          \
        (size) == CMSG_ALIGN(size) ? 1 : -1];                   \
    }

#ifdef __cplusplus
}
#endif
#endif // BASE_STARTUP_INITLITE_FD_HOLDER_INTERNAL_H
