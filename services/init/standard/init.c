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
#include <poll.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/major.h>
#include "device.h"
#include "fs_manager/fs_manager.h"
#include "init_log.h"
#include "init_mount.h"
#include "init_param.h"
#include "init_utils.h"
#include "securec.h"
#include "switch_root.h"
#include "ueventd.h"
#include "ueventd_socket.h"
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#endif // WITH_SELINUX

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

static char **GetRequiredDevices(Fstab fstab, int *requiredNum)
{
    int num = 0;
    FstabItem *item = fstab.head;
    while (item != NULL) {
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            num++;
        }
        item = item->next;
    }
    char **devices = (char **)calloc(num, sizeof(char *));
    INIT_ERROR_CHECK(devices != NULL, return NULL, "Failed calloc err=%d", errno);

    int i = 0;
    item = fstab.head;
    while (item != NULL) {
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            devices[i] = strdup(item->deviceName);
            INIT_ERROR_CHECK(devices[i] != NULL, FreeStringVector(devices, num); return NULL,
                "Failed strdup err=%d", errno);
            i++;
        }
        item = item->next;
    }
    *requiredNum = num;
    return devices;
}

static int StartUeventd(char **requiredDevices, int num)
{
    INIT_ERROR_CHECK(requiredDevices != NULL && num > 0, return -1, "Failed parameters");
    int ueventSockFd = UeventdSocketInit();
    if (ueventSockFd < 0) {
        INIT_LOGE("Failed to create uevent socket");
        return -1;
    }
    RetriggerUevent(ueventSockFd, requiredDevices, num);
    return 0;
}

static void StartInitSecondStage(void)
{
    const char *fstabFile = "/etc/fstab.required";
    Fstab *fstab = NULL;
    INIT_ERROR_CHECK(access(fstabFile, F_OK) == 0, abort(), "Failed get fstab.required");
    fstab = ReadFstabFromFile(fstabFile, false);
    INIT_ERROR_CHECK(fstab != NULL, abort(), "Read fstab file \" %s \" failed\n", fstabFile);

    int requiredNum = 0;
    char **devices = GetRequiredDevices(*fstab, &requiredNum);
    if (devices != NULL && requiredNum > 0) {
        int ret = StartUeventd(devices, requiredNum);
        if (ret == 0) {
            ret = MountRequriedPartitions(fstab);
        }
        FreeStringVector(devices, requiredNum);
        devices = NULL;
        ReleaseFstab(fstab);
        fstab = NULL;
        if (ret < 0) {
            // If mount required partitions failure.
            // There is no necessary to continue.
            // Just abort
            INIT_LOGE("Mount requried partitions failed");
            abort();
        }
    }
    SwitchRoot("/usr");
    // Execute init second stage
    char * const args[] = {
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

void SystemLoadSelinux(void)
{
#ifdef WITH_SELINUX
    // load selinux policy and context
    if (load_policy() < 0) {
        INIT_LOGE("main, load_policy failed.");
    } else {
        INIT_LOGI("main, load_policy success.");
    }
    if (restorecon() < 0) {
        INIT_LOGE("main, restorecon failed.");
    } else {
        INIT_LOGI("main, restorecon success.");
    }
#endif // WITH_SELINUX
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

    // load SELinux context and policy
    SystemLoadSelinux();
}

void SystemRun(void)
{
    StartParamService();
}
