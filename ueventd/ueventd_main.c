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

#include <poll.h>
#include "ueventd.h"
#include "ueventd_device_handler.h"
#include "ueventd_read_cfg.h"
#include "ueventd_socket.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"

int main(int argc, char **argv)
{
    char *ueventdConfigs[] = {"/etc/ueventd.config", NULL};
    int i = 0;
    int ret = -1;
    while (ueventdConfigs[i] != NULL) {
        ParseUeventdConfigFile(ueventdConfigs[i++]);
    }
    int ueventSockFd = UeventdSocketInit();
    if (ueventSockFd < 0) {
        INIT_LOGE("Failed to create uevent socket");
        return -1;
    }

    RetriggerUevent(ueventSockFd);
    struct pollfd pfd = {};
    pfd.events = POLLIN;
    pfd.fd = ueventSockFd;

    while (1) {
        pfd.revents = 0;
        ret = poll(&pfd, 1, -1);
        if (ret <= 0) {
            continue;
        }
        if (pfd.revents & POLLIN) {
            ProcessUevent(ueventSockFd);
        }
    }
    return 0;
}
