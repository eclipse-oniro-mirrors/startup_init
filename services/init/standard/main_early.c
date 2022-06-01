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
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/major.h>
#include "device.h"
#include "init_log.h"
#include "init_utils.h"
#include "init_mount.h"
#include "fs_manager/fs_manager.h"
#include "switch_root.h"
#include "ueventd.h"
#include "ueventd_socket.h"

static void EnableDevKmsg(void)
{
    /* printk_devkmsg default value is ratelimit, We need to set "on" and remove the restrictions */
    int fd = open("/proc/sys/kernel/printk_devkmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0) {
        return;
    }
    char *kmsgStatus = "on";
    write(fd, kmsgStatus, strlen(kmsgStatus) + 1);
    close(fd);
    fd = -1;
    return;
}

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
    close(ueventSockFd);
    return 0;
}

static void MountRequiredPartitions(void)
{
    int requiredNum = 0;
    Fstab* fstab = LoadRequiredFstab();
    INIT_ERROR_CHECK(fstab != NULL, abort(), "Failed to load required fstab");
    char **devices = GetRequiredDevices(*fstab, &requiredNum);
    if (devices == NULL || requiredNum <= 0) {
        return;
    }

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
        INIT_LOGE("Mount requried partitions failed; please check fstab file");
        // Execute sh for debugging
        execv("/bin/sh", NULL);
        abort();
    }
    SwitchRoot("/usr");
}

int main(int argc, char * const argv[])
{
    MountBasicFs();
    CreateDeviceNode();
    OpenLogDevice();
    // Make sure init log always output to /dev/kmsg.
    EnableDevKmsg();

    if (InUpdaterMode() == 0) {
        MountRequiredPartitions();
    }

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
    return 0;
}
