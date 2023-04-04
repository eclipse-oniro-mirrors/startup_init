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

#include "ueventd_firmware_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include "ueventd.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"
#include "securec.h"

void HandleFimwareDeviceEvent(const struct Uevent *uevent)
{
    char fwLoadingPath[PATH_MAX] = {};

    if (snprintf_s(fwLoadingPath, PATH_MAX, PATH_MAX - 1, "/sys%s/loading", uevent->syspath) == -1) {
        INIT_LOGE("Failed to build firmware loading path");
        return;
    }

    int fd = open(fwLoadingPath, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        INIT_LOGE("Failed to open %s, err = %d", fwLoadingPath, errno);
        return;
    }

    char *endCode = "-1";
    (void)write(fd, "-1", strlen(endCode));
    close(fd);
    fd = -1;
}
