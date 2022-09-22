/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef LINUX_WATCHDOG
#include <linux/watchdog.h>
#else
#include <linux/types.h>
#define WDIOC_KEEPALIVE      _IO('W', 5)
#define WDIOC_SETTIMEOUT     _IOWR('W', 6, int)
#define WDIOC_GETTIMEOUT     _IOR('W', 7, int)
#endif

#include "init_log.h"

#define WAIT_MAX_COUNT 20
#define DEFAULT_INTERVAL 3
#define DEFAULT_GAP 3
#define CONVERSION_BASE 1000000U

#define PRETIMEOUT_GAP 5
#define PRETIMEOUT_DIV 2

#define WATCHDOG_DEV "/dev/watchdog"

static int WaitForWatchDogDevice(void)
{
    unsigned int count = 0;
    struct stat sourceInfo;
    const unsigned int waitTime = 500000;
    while ((stat(WATCHDOG_DEV, &sourceInfo) < 0) && (errno == ENOENT) && (count < WAIT_MAX_COUNT)) {
        usleep(waitTime);
        count++;
    }

    if (count == WAIT_MAX_COUNT) {
        INIT_LOGE("Wait for watchdog device failed after %u seconds", (WAIT_MAX_COUNT * waitTime) / CONVERSION_BASE);
        return 0;
    }
    return 1;
}

int main(int argc, const char *argv[])
{
    if (WaitForWatchDogDevice() == 0) {
        return -1;
    }
    int fd = open(WATCHDOG_DEV, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        INIT_LOGE("Open watchdog device failed, err = %d", errno);
        return -1;
    }

    int interval = 0;
    if (argc >= 2) { // Argument nums greater than or equal to 2.
        interval = atoi(argv[1]);
    }
    interval = (interval > 0) ? interval : DEFAULT_INTERVAL;

    int gap = 0;
    if (argc >= 3) { // Argument nums greater than or equal to 3.
        gap = atoi(argv[2]); // 2 second parameter.
    }
    gap = (gap > 0) ? gap : DEFAULT_GAP;

#ifdef OHOS_LITE_WATCHDOG
#ifndef LINUX_WATCHDOG
    if (setpriority(PRIO_PROCESS, 0, 14) != 0) { // 14 is process priority
        INIT_LOGE("setpriority failed, err=%d", errno);
    }
#endif
#endif
    int timeoutSet = interval + gap;
    int timeoutGet = 0;

#ifdef WDIOC_SETPRETIMEOUT
    int preTimeout = 0;
    int preTimeoutGet = 0;
#endif

    int ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeoutSet);
    if (ret < 0) {
        INIT_LOGE("ioctl failed with command WDIOC_SETTIMEOUT, err = %d", errno);
        close(fd);
        return -1;
    }
    ret = ioctl(fd, WDIOC_GETTIMEOUT, &timeoutGet);
    if (ret < 0) {
        INIT_LOGE("ioctl failed with command WDIOC_GETTIMEOUT, err = %d", errno);
        close(fd);
        return -1;
    }

    if (timeoutGet > 0) {
        interval = (timeoutGet > gap) ? (timeoutGet - gap) : 1;
    }

#ifdef WDIOC_SETPRETIMEOUT
    preTimeout = timeoutGet - PRETIMEOUT_GAP; // ensure pre timeout smaller then timeout
    if (preTimeout > 0) {
        ret = ioctl(fd, WDIOC_SETPRETIMEOUT, &preTimeout);
        if (ret < 0) {
            INIT_LOGE("ioctl failed with command WDIOC_SETPRETIMEOUT, err = %d", errno);
            close(fd);
            return -1;
        }
        ret = ioctl(fd, WDIOC_GETPRETIMEOUT, &preTimeoutGet);
        if (ret < 0) {
            INIT_LOGE("ioctl failed with command WDIOC_GETPRETIMEOUT, err = %d", errno);
            close(fd);
            return -1;
        }
    }

    if (preTimeoutGet > 0 && preTimeoutGet < interval) {
        interval = preTimeoutGet / PRETIMEOUT_DIV;
    }
#endif

    INIT_LOGI("watchdog started (interval %d, margin %d)", interval, gap);
    while (1) {
        ret = ioctl(fd, WDIOC_KEEPALIVE);
        if (ret < 0) {
            // Fed watchdog failed, we don't need to quit the process.
            // Wait for kernel to trigger panic.
            INIT_LOGE("ioctl failed with command WDIOC_KEEPALIVE, err = %d", errno);
        }
        sleep(interval);
    }
    close(fd);
    fd = -1;
    return -1;
}
