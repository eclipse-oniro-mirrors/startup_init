/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include "init.h"

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/major.h>

#include "device.h"
#include "init_log.h"
#include "init_mount.h"
#include "init_param.h"
#include "init_utils.h"
#include "securec.h"
#include "switch_root.h"

void SystemInit(void)
{
    SignalInit();
    MakeDirRecursive("/dev/unix/socket", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

void LogInit(void)
{
    CloseStdio();
    int ret = mknod("/dev/kmsg", S_IFCHR | S_IWUSR | S_IRUSR,
        makedev(MEM_MAJOR, DEV_KMSG_MINOR));
    if (ret == 0) {
        OpenLogDevice();
    }
}

#ifndef DISABLE_INIT_TWO_STAGES
static pid_t StartUeventd(void)
{
    char *const argv[] = {
        "/bin/ueventd",
        NULL,
    };
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("Failed to fork child process");
    }
    if (pid == 0) {
        if (execv(argv[0], argv) != 0) {
            INIT_LOGE("service %s execve failed! err %d.", argv[0], errno);
        }
        _exit(0x7f); // 0x7f: user specified
    }
    return pid;
}

static void StartInitSecondStage(void)
{
    pid_t ueventPid = StartUeventd();
    if (ueventPid < 0) {
        INIT_LOGE("Failed to start ueventd");
        abort();
    }
    if (MountRequriedPartitions() < 0) {
        // If mount required partitions failure.
        // There is no necessary to continue.
        // Just abort
        INIT_LOGE("Mount requried partitions failed");
        abort();
    }
    // Kill ueventd, because init second stage will start it again.
    (void)kill(ueventPid, SIGKILL);
    // The init process in ramdisk has done its job.
    // It's ready to switch to system partition.
    // The system partition mounted in first stage to /usr
    // Because the directory /system is in use. we cannot use it.
    // After switch root. /usr will become new root.
    SwitchRoot("/usr");
    // Execute init second stage
    char *const args[] = {
        "/bin/init",
        "--second-stage",
        NULL,
    };
    if (execv("/bin/init", args) != 0) {
        INIT_LOGE("Failed to exec \"/bin/init\", err = %d", errno);
        exit(-1);
    }
}
#endif

void SystemPrepare(void)
{
    MountBasicFs();
    // Make sure init log always output to /dev/kmsg.
    EnableDevKmsg();
    CreateDeviceNode();
#ifndef DISABLE_INIT_TWO_STAGES
    // Only ohos normal system support
    // two stages of init.
    // If we are in updater mode, only one stage of init,
    INIT_LOGI("DISABLE_INIT_TWO_STAGES not defined");
    if (InUpdaterMode() == 0) {
        StartInitSecondStage();
    }
#else
    INIT_LOGI("DISABLE_INIT_TWO_STAGES defined");
#endif
}

void SystemConfig(void)
{
    InitParamService();
    // parse parameters
    LoadDefaultParams("/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    LoadDefaultParams("/vendor/etc/param", LOAD_PARAM_NORMAL);
    LoadDefaultParams("/system/etc/param", LOAD_PARAM_ONLY_ADD);
    // read config
    ReadConfig();
    INIT_LOGI("Parse init config file done.");

    // dump config
#ifdef OHOS_SERVICE_DUMP
    DumpAllServices();
    DumpParametersAndTriggers();
#endif

    // execute init
    PostTrigger(EVENT_TRIGGER_BOOT, "pre-init", strlen("pre-init"));
    PostTrigger(EVENT_TRIGGER_BOOT, "init", strlen("init"));
    PostTrigger(EVENT_TRIGGER_BOOT, "post-init", strlen("post-init"));
}

void SystemRun(void)
{
    StartParamService();
}
