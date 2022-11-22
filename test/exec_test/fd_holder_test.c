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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "beget_ext.h"
#include "fd_holder.h"

#define BUFFER_LENGTH 5
#define FD_COUNT 2
#define SLEEP_TIME 3
#define ENV_FD_HOLD_PREFIX "OHOS_FD_HOLD_"

static void SaveFds(const char *serviceName, int argc, char **argv)
{
    int fds[10];
    int i;
    for (i = 0; i < argc; i++) {
        BEGET_LOGI("Opening %s \n", argv[i]);
        fds[i] = open(argv[i], O_APPEND | O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (fds[i] < 0) {
            BEGET_LOGI("Failed to open file %s\n", argv[i]);
            break;
        }
        (void)write(fds[i], "hello", BUFFER_LENGTH);
    }
    int fdCount = i;
    BEGET_LOGI("fdCount = %d\n", fdCount);
    if (fdCount <= 0) {
        BEGET_LOGE("Invalid fds for hold, abort\n");
        return;
    }
    int ret = ServiceSaveFd(serviceName, fds, (size_t)fdCount);
    if (ret < 0) {
        BEGET_LOGE("Request init save fds failed\n");
    } else {
        BEGET_LOGI("Request init save fds done\n");
    }
}


int main(int argc, char **argv)
{
    size_t outfdCount = 0;
    int *fds = ServiceGetFd("fd_holder_test", &outfdCount);
    if (fds == NULL) {
        BEGET_LOGE("Cannot get fds, maybe first time\n");
    } else {
        for (size_t i = 0; i < outfdCount; i++) {
            int fd = fds[i];
            if (write(fd, "world", BUFFER_LENGTH) < 0) {
                BEGET_LOGE("Failed to write content to fd: %d, err = %d", fd, errno);
            }
            close(fd);
        }
        free(fds);
        outfdCount = 0;
        while (1) {
            sleep(SLEEP_TIME);
        }
    }
    char *files[] = {"/data/test/1", "/data/test/2"};
    SaveFds("fd_holder_test", FD_COUNT, (char **)files);
    while (1) {
        sleep(SLEEP_TIME);
    }
    return 0;
}
