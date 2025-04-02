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

#define SUPPORT_AB_PARTITION_UPDATE 2

static char **GetRequiredDevices(Fstab fstab, int *requiredNum)
{
    int num = 0;
    FstabItem *item = fstab.head;
    while (item != NULL) {
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            num++;
        }
#ifdef EROFS_OVERLAY
        if (item->next != NULL && strcmp(item->deviceName, item->next->deviceName) == 0) {
            item = item->next->next;
        } else {
            item = item->next;
        }
#else
        item = item->next;
#endif
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
#ifdef EROFS_OVERLAY
        if (item->next != NULL && strcmp(item->deviceName, item->next->deviceName) == 0) {
            item = item->next->next;
        } else {
            item = item->next;
        }
#else
        item = item->next;
#endif
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
    fdsan_close_with_tag(ueventSockFd, BASE_DOMAIN);
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
            if (GetBootSlots() >= SUPPORT_AB_PARTITION_UPDATE) {
                int snapshotRet = HookMgrExecute(GetBootStageHookMgr(), INIT_SNAPSHOT_ACTIVE,
                                                 (void *)(fstab), NULL);
                INIT_LOGI("active sanpshot ret = %d", snapshotRet);
            }
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
            INIT_LOGE("[startup_failed]Mount required partitions failed; please check fstab file %d",
                FSTAB_MOUNT_FAILED);
#ifndef STARTUP_INIT_TEST
            abort();
#endif
        }
    }

    if (fstab != NULL) {
        ReleaseFstab(fstab);
        fstab = NULL;
    }
}

#ifdef ASAN_DETECTOR
static void ChekcAndRunAsanInit(char * const args[])
{
    const char* asanInitPath = "/system/asan/bin/init";
    char rebootReason[MAX_BUFFER_LEN] = {0};
    INIT_LOGI("ChekcAndRunAsan Begin");
    int ret = GetParameterFromCmdLine("reboot_reason", rebootReason, MAX_BUFFER_LEN);
    if (ret) {
        INIT_LOGE("Failed get reboot_reason from cmdline.");
        return;
    }
    if (strcmp(rebootReason, "COLDBOOT") != 0) {
        INIT_LOGE("rebootReason is not COLDBOOT, skip.");
        return;
    }
    if (access(asanInitPath, X_OK) != 0) {
        INIT_LOGE("%s not exit, skip.", asanInitPath);
        return;
    }
    INIT_LOGI("redirect stdio to /dev/kmsg");
    OpenKmsg();

    setenv("ASAN_OPTIONS", "include=/system/etc/asan.options", 1);
    setenv("TSAN_OPTIONS", "include=/system/etc/asan.options", 1);
    setenv("UBSAN_OPTIONS", "include=/system/etc/asan.options", 1);
    setenv("HWASAN_OPTIONS", "include=/system/etc/asan.options", 1);
    INIT_LOGI("Execute %s, process id %d.", asanInitPath, getpid());
    if (execv(asanInitPath, args) != 0) {
        INIT_LOGE("Execute %s, execle failed! err %d.", asanInitPath, errno);
    }
}
#endif
static void StartSecondStageInit(long long uptime)
{
    INIT_LOGI("Start init second stage.");
    // It will panic if close stdio before execv("/bin/sh", NULL)
    CloseStdio();

    char buf[64];
    uptime = GetUptimeInMicroSeconds(NULL);
    INIT_CHECK_ONLY_ELOG(snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%lld", uptime) >= 0,
                         "snprintf_s uptime to buf failed");
    // Execute init second stage
    char * const args[] = {
        "/bin/init",
        "--second-stage",
        buf,
        NULL,
    };
#ifdef ASAN_DETECTOR
    ChekcAndRunAsanInit(args);
#endif
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
