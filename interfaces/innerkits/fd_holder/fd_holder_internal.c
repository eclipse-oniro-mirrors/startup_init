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

#include "fd_holder_internal.h"
#include <errno.h>
#include <stdio.h>
#include "beget_ext.h"
#include "securec.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE (4096U)
#endif

int BuildControlMessage(struct msghdr *msghdr,  int *fds, int fdCount, bool sendUcred)
{
    if (msghdr == NULL || (fdCount > 0 && fds == NULL)) {
        BEGET_LOGE("Build control message with invalid parameter");
        return -1;
    }

    if (fdCount > 0) {
        msghdr->msg_controllen = CMSG_SPACE(sizeof(int) * fdCount);
    } else {
        msghdr->msg_controllen = 0;
    }

    if (sendUcred) {
        msghdr->msg_controllen += CMSG_SPACE(sizeof(struct ucred));
    }

    msghdr->msg_control = calloc(1, ((msghdr->msg_controllen == 0) ? 1 : msghdr->msg_controllen));
    if (msghdr->msg_control == NULL) {
        BEGET_LOGE("Failed to build control message");
        return -1;
    }

    struct cmsghdr *cmsg = NULL;
    cmsg = CMSG_FIRSTHDR(msghdr);

    if (fdCount > 0) {
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fdCount);
        int ret = memcpy_s(CMSG_DATA(cmsg), cmsg->cmsg_len, fds, sizeof(int) * fdCount);
        if (ret != 0) {
            BEGET_LOGE("Control message is not valid");
            free(msghdr->msg_control);
            return -1;
        }
        // build ucred info
        cmsg = CMSG_NXTHDR(msghdr, cmsg);
    }

    if (sendUcred) {
        if (cmsg == NULL) {
            BEGET_LOGE("Control message is not valid");
            free(msghdr->msg_control);
            return -1;
        }
        struct ucred *ucred;
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_CREDENTIALS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
        ucred = (struct ucred*) CMSG_DATA(cmsg);
        ucred->pid = getpid();
        ucred->uid = getuid();
        ucred->gid = getgid();
    }
    return 0;
}

STATIC int *GetFdsFromMsg(size_t *outFdCount, pid_t *requestPid, struct msghdr msghdr)
{
    if ((unsigned int)(msghdr.msg_flags) & (unsigned int)MSG_TRUNC) {
        BEGET_LOGE("Message was truncated when receiving fds");
        return NULL;
    }

    struct cmsghdr *cmsg = NULL;
    int *fds = NULL;
    size_t fdCount = 0;
    for (cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            fds = (int*)CMSG_DATA(cmsg);
            fdCount = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            BEGET_ERROR_CHECK(fdCount <= MAX_HOLD_FDS, return NULL, "Too many fds returned.");
        }
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS &&
            cmsg->cmsg_len == CMSG_LEN(sizeof(struct ucred))) {
            // Ignore credentials
            if (requestPid != NULL) {
                struct ucred *ucred = (struct ucred*)CMSG_DATA(cmsg);
                *requestPid = ucred->pid;
            }
            continue;
        }
    }
    int *outFds = NULL;
    if (fds != NULL && fdCount > 0) {
        outFds = calloc(fdCount + 1, sizeof(int));
        BEGET_ERROR_CHECK(outFds != NULL, return NULL, "Failed to allocate memory for fds");
        BEGET_ERROR_CHECK(memcpy_s(outFds, sizeof(int) * (fdCount + 1), fds, sizeof(int) * fdCount) == 0,
            free(outFds); return NULL, "Failed to copy fds");
    }
    *outFdCount = fdCount;
    return outFds;
}

// This function will allocate memory to store FDs
// Remember to delete when not used anymore.
int *ReceiveFds(int sock, struct iovec iovec, size_t *outFdCount, bool nonblock, pid_t *requestPid)
{
    CMSG_BUFFER_TYPE(CMSG_SPACE(sizeof(struct ucred)) +
         CMSG_SPACE(sizeof(int) * MAX_HOLD_FDS)) control;

    BEGET_ERROR_CHECK(sizeof(control) <= PAGE_SIZE, return NULL, "Too many fds, out of memory");

    struct msghdr msghdr = {
        .msg_iov = &iovec,
        .msg_iovlen = 1,
        .msg_control = &control,
        .msg_controllen = sizeof(control),
        .msg_flags = 0,
    };

    int flags = MSG_CMSG_CLOEXEC | MSG_TRUNC;
    if (nonblock) {
        flags |= MSG_DONTWAIT;
    }
    ssize_t rc = TEMP_FAILURE_RETRY(recvmsg(sock, &msghdr, flags));
    BEGET_ERROR_CHECK(rc >= 0, return NULL, "Failed to get fds from remote, err = %d", errno);
    return GetFdsFromMsg(outFdCount, requestPid, msghdr);
}