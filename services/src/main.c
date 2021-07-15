/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef OHOS_DEBUG
#include <time.h>
#endif // OHOS_DEBUG

#include <unistd.h>

#include "init_adapter.h"
#include "init_log.h"
#include "init_read_cfg.h"
#include "init_signal_handler.h"
#ifdef OHOS_LITE
#include "parameter.h"
#endif

#ifndef OHOS_LITE
#include "device.h"
#include "init_param.h"
#endif

static const pid_t INIT_PROCESS_PID = 1;

static void PrintSysInfo()
{
#ifdef OHOS_LITE
    const char* sysInfo = GetVersionId();
    if (sysInfo != NULL) {
        INIT_LOGE("%s", sysInfo);
        return;
    }
    INIT_LOGE("main, GetVersionId failed!");
#endif
}

#ifdef OHOS_DEBUG
static long TimeDiffMs(struct timespec* tmBefore, struct timespec* tmAfter)
{
    if (tmBefore != NULL && tmAfter != NULL) {
        long timeUsed = (tmAfter->tv_sec - tmBefore->tv_sec) * 1000 +     // 1 s = 1000 ms
            (tmAfter->tv_nsec - tmBefore->tv_nsec) / 1000000;    // 1 ms = 1000000 ns
        return timeUsed;
    }
    return -1;
}
#endif // OHOS_DEBUG

int main(int argc, char * const argv[])
{
#ifndef OHOS_LITE
    if(setenv("UV_THREADPOOL_SIZE", "1", 1) != 0) {
        INIT_LOGE("set UV_THREADPOOL_SIZE error : %d.", errno);
    }

#endif
#ifdef OHOS_DEBUG
    struct timespec tmEnter;
    if (clock_gettime(CLOCK_REALTIME, &tmEnter) != 0) {
        INIT_LOGE("main, enter, get time failed! err %d.\n", errno);
    }
#endif // OHOS_DEBUG

    if (getpid() != INIT_PROCESS_PID) {
        INIT_LOGE("main, current process id is %d not %d, failed!", getpid(), INIT_PROCESS_PID);
        return 0;
    }

    // 1. print system info
    PrintSysInfo();

#ifndef OHOS_LITE
    // 2. Mount basic filesystem and create common device node.
    MountBasicFs();
    CreateDeviceNode();
    MakeSocketDir("/dev/unix/socket/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

    // 3. signal register
    SignalInitModule();

#ifdef OHOS_DEBUG
    struct timespec tmSysInfo;
    if (clock_gettime(CLOCK_REALTIME, &tmSysInfo) != 0) {
        INIT_LOGE("main, after sysinfo, get time failed! err %d.", errno);
    }
#endif // OHOS_DEBUG

    // 4. execute rcs
    ExecuteRcs();

#ifdef OHOS_DEBUG
    struct timespec tmRcs;
    if (clock_gettime(CLOCK_REALTIME, &tmRcs) != 0) {
        INIT_LOGE("main, after rcs, get time failed! err %d.", errno);
    }
#endif // OHOS_DEBUG
    // 5. read configuration file and do jobs
    InitReadCfg();
#ifdef OHOS_DEBUG
    struct timespec tmCfg;
    if (clock_gettime(CLOCK_REALTIME, &tmCfg) != 0) {
        INIT_LOGE("main, get time failed! err %d.", errno);
    }
#endif // OHOS_DEBUG

    // 6. keep process alive
#ifdef OHOS_DEBUG
    INIT_LOGI("main, time used: sigInfo %ld ms, rcs %ld ms, cfg %ld ms.", \
        TimeDiffMs(&tmEnter, &tmSysInfo), TimeDiffMs(&tmSysInfo, &tmRcs), TimeDiffMs(&tmRcs, &tmCfg));
#endif

    INIT_LOGI("main, entering wait.");
#ifndef OHOS_LITE
    StartParamService();
#endif
    while (1) {
        // pause only returns when a signal was caught and the signal-catching function returned.
        // pause only returns -1, no need to process the return value.
        (void)pause();
    }
    return 0;
}
