/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/major.h>
#include "securec.h"
#include "device.h"
#include "init.h"
#include "init_log.h"
#include "init_utils.h"
#include "init_mount.h"
#include "fs_manager/fs_manager.h"
#include "switch_root.h"
#include "ueventd.h"
#include "ueventd_socket.h"
#include "bootstage.h"

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
    if (num == 0) {
        return NULL;
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
    close(ueventSockFd);
    return 0;
}

static void MountRequiredPartitions(void)
{
    int requiredNum = 0;
    Fstab *fstab = LoadRequiredFstab();
    char **devices = (fstab != NULL) ? GetRequiredDevices(*fstab, &requiredNum) : NULL;
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
            INIT_LOGE("[startup_failed]Mount required partitions failed; please check fstab file %d", FSTAB_MOUNT_FAILED);
            // Execute sh for debugging
#ifndef STARTUP_INIT_TEST
            execv("/bin/sh", NULL);
            abort();
#endif
        }
    }

    if (fstab != NULL) {
        ReleaseFstab(fstab);
        fstab = NULL;
    }
}

static void StartSecondStageInit(long long uptime)
{
    INIT_LOGI("Start init second stage.");
    // It will panic if close stdio before execv("/bin/sh", NULL)
    CloseStdio();

    SwitchRoot("/usr");
    char buf[64];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%lld", uptime);
    // Execute init second stage
    char * const args[] = {
        "/bin/init",
        "--second-stage",
        buf,
        NULL,
    };
    if (execv("/bin/init", args) != 0) {
        INIT_LOGE("Failed to exec \"/bin/init\", err = %d", errno);
        exit(-1);
    }
}

static void EarlyLogInit(void)
{
    int ret = mknod("/dev/kmsg", S_IFCHR | S_IWUSR | S_IRUSR,
        makedev(MEM_MAJOR, DEV_KMSG_MINOR));
    if (ret == 0) {
        OpenLogDevice();
    }
}

void SystemPrepare(long long upTimeInMicroSecs)
{
    (void)signal(SIGPIPE, SIG_IGN);

    EnableInitLog(INIT_INFO);
    EarlyLogInit();
    INIT_LOGI("Start init first stage.");

    CreateFsAndDeviceNode();

    HookMgrExecute(GetBootStageHookMgr(), INIT_FIRST_STAGE, NULL, NULL);

    // Updater mode no need to mount and switch root
    if (InUpdaterMode() != 0) {
        return;
    }

    MountRequiredPartitions();

    StartSecondStageInit(upTimeInMicroSecs);
}
