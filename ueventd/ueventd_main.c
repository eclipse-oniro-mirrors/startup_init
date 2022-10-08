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

#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include "ueventd.h"
#include "ueventd_read_cfg.h"
#include "ueventd_socket.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"
#include "init_socket.h"

static void PollUeventdSocketTimeout(int ueventSockFd, bool ondemand)
{
    struct pollfd pfd = {};
    pfd.events = POLLIN;
    pfd.fd = ueventSockFd;
    int timeout = ondemand ? UEVENTD_POLL_TIME : -1;
    int ret = -1;

    while (1) {
        pfd.revents = 0;
        ret = poll(&pfd, 1, timeout);
        if (ret == 0) {
            INIT_LOGI("poll ueventd socket timeout, ueventd exit");
            return;
        } else if (ret < 0) {
            INIT_LOGE("Failed to poll ueventd socket!");
            return;
        }
        if (pfd.revents & POLLIN) {
            ProcessUevent(ueventSockFd, NULL, 0); // Not require boot devices
        }
    }
}

int main(int argc, char **argv)
{
    // start log
    EnableInitLog(INIT_INFO);
    char *ueventdConfigs[] = {"/etc/ueventd.config", "/vendor/etc/ueventd.config", NULL};
    int i = 0;
    while (ueventdConfigs[i] != NULL) {
        ParseUeventdConfigFile(ueventdConfigs[i++]);
    }
    bool ondemand = true;
    int ueventSockFd = GetControlSocket("ueventd");
    if (ueventSockFd < 0) {
        INIT_LOGW("Failed to get uevent socket, try to create");
        ueventSockFd = UeventdSocketInit();
        ondemand = false;
    }
    if (ueventSockFd < 0) {
        INIT_LOGE("Failed to create uevent socket!");
        return -1;
    }
    if (access(UEVENTD_FLAG, F_OK)) {
        INIT_LOGI("Ueventd started, trigger uevent");
        RetriggerUevent(ueventSockFd, NULL, 0); // Not require boot devices
        int fd = open(UEVENTD_FLAG, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            INIT_LOGE("Failed to create ueventd flag!");
            return -1;
        }
        (void)close(fd);
    } else {
        INIT_LOGI("ueventd start to process uevent message");
        ProcessUevent(ueventSockFd, NULL, 0); // Not require boot devices
    }
    PollUeventdSocketTimeout(ueventSockFd, ondemand);
    return 0;
}
