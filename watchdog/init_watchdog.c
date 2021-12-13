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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
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

#define WAIT_MAX_COUNT 10
#define DEFAULT_INTERVAL 3
#define DEFAULT_GAP 3

static void WaitAtStartup(const char *source)
{
    unsigned int count = 0;
    struct stat sourceInfo;
    const unsigned int waitTime = 500000;
    do {
        usleep(waitTime);
        count++;
    } while ((stat(source, &sourceInfo) < 0) && (errno == ENOENT) && (count < WAIT_MAX_COUNT));
    if (count == WAIT_MAX_COUNT) {
        INIT_LOGE("wait for file:%s failed after %f.", source, WAIT_MAX_COUNT>>1);
    }
    return;
}

int main(int argc, const char *argv[])
{
    WaitAtStartup("/dev/watchdog");
    int fd = open("/dev/watchdog", O_RDWR);
    if (fd == -1) {
        INIT_LOGE("Can't open /dev/watchdog.");
        return 1;
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

    INIT_LOGI("watchdoge started (interval %d, margin %d), fd = %d\n", interval, gap, fd);

    int timeoutSet = interval + gap;
    int timeoutGet = 0;
    int ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeoutSet);
    if (ret) {
        INIT_LOGE("Failed to set timeout to %d\n", timeoutSet);
    }
    ret = ioctl(fd, WDIOC_GETTIMEOUT, &timeoutGet);
    if (ret) {
        INIT_LOGE("Failed to get timeout\n");
    } else {
        interval = (timeoutGet > gap) ? (timeoutGet - gap) : 1;
    }
    while (1) {
        ioctl(fd, WDIOC_KEEPALIVE);
        sleep(interval);
    }
    close(fd);
    fd = -1;
    return -1;
}
