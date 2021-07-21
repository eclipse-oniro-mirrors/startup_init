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

#include "init_reboot.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/reboot.h>
#include "securec.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_log.h"

#define MAX_VALUE_LENGTH 500
#define MAX_COMMAND_SIZE 20
#define MAX_UPDATE_SIZE 100

struct RBMiscUpdateMessage {
    char command[MAX_COMMAND_SIZE];
    char update[MAX_UPDATE_SIZE];
};

static bool RBMiscWriteUpdaterMessage(const char *path, struct RBMiscUpdateMessage *boot)
{
    if (path == NULL || boot == NULL) {
        INIT_LOGE("path or boot is NULL.");
        return false;
    }
    FILE* fp = fopen(path, "rb+");
    if (fp == NULL) {
        INIT_LOGE("open %s failed", path);
        return false;
    }

    size_t ret = fwrite(boot, sizeof(struct RBMiscUpdateMessage), 1, fp);
    if (ret < 0) {
        INIT_LOGE("write to misc failed");
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

static bool RBMiscReadUpdaterMessage(const char *path, struct RBMiscUpdateMessage *boot)
{
    if (path == NULL || boot == NULL) {
        INIT_LOGE("path or boot is NULL.");
        return false;
    }
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        INIT_LOGE("open %s failed", path);
        return false;
    }

    size_t ret = fread(boot, 1, sizeof(struct RBMiscUpdateMessage), fp);
    if (ret <= 0) {
        INIT_LOGE("read to misc failed");
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static int GetMountStatusForMountPoint(const char *mountPoint)
{
    const int bufferMaxSize = 512;
    char buffer[bufferMaxSize];
    size_t n;
    const char *mountFile = "/proc/mounts";
    FILE *fp = fopen(mountFile, "r");
    if (fp == NULL) {
        INIT_LOGE("DoReboot %s can't open.", mountPoint);
        return 1;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        n = strlen(buffer);
        if (buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
        }
        if (strstr(buffer, mountPoint) != NULL) {
            fclose(fp);
            return 1;
        }
    }

    // Cannot find it from system.
    fclose(fp);
    return 0;
}

void DoReboot(const char *value)
{
    if (value == NULL) {
        INIT_LOGE("DoReboot value = NULL");
        return;
    }
    INIT_LOGI("DoReboot value = %s", value);

    if (strlen(value) > MAX_VALUE_LENGTH) {
        INIT_LOGE("DoReboot reboot value error, value = %s.", value);
        return;
    }

    const char *valueData = NULL;
    if (strncmp(value, "reboot,", strlen("reboot,")) != 0) {
        INIT_LOGE("DoReboot reboot value = %s, must started with reboot ,error.", value);
        return;
    } else {
        valueData = value + strlen("reboot,");
    }
    if (strncmp(valueData, "shutdown", strlen("shutdown")) != 0
        && strncmp(valueData, "updater:", strlen("updater:")) != 0
        && strncmp(valueData, "updater", strlen("updater")) != 0
        && strncmp(valueData, "NoArgument", strlen("NoArgument")) != 0) {
        INIT_LOGE("DoReboot value = %s, parameters error.", value);
        return;
    }

    StopAllServicesBeforeReboot();
    if (GetMountStatusForMountPoint("/vendor") != 0) {
        if (umount("/vendor") != 0) {
            INIT_LOGE("DoReboot umount vendor failed! errno = %d.", errno);
        }
    }
    if (GetMountStatusForMountPoint("/data") != 0) {
        if (umount("/data") != 0) {
            INIT_LOGE("DoReboot umount data failed! errno = %d.", errno);
        }
    }
    // "shutdown"
    if (strncmp(valueData, "shutdown", strlen("shutdown")) == 0) {
        int ret = reboot(RB_POWER_OFF);
        if (ret != 0) {
            INIT_LOGE("DoReboot reboot(RB_POWER_OFF) failed! syscall ret %d, err %d.", ret, errno);
        }
        return;
    }
    // "updater" or "updater:"
    const char *miscFile = "/dev/block/platform/soc/10100000.himci.eMMC/by-name/misc";
    struct RBMiscUpdateMessage msg;
    bool ret = RBMiscReadUpdaterMessage(miscFile, &msg);
    if(!ret) {
        INIT_LOGE("DoReboot RBMiscReadUpdaterMessage error.");
        return;
    }
    const int commandSize = 12;
    snprintf(msg.command, MAX_COMMAND_SIZE, "%s", "boot_updater");
    msg.command[commandSize] = 0;

    if (strlen(valueData) > strlen("updater:") && strncmp(valueData, "updater:", strlen("updater:")) == 0) {
        const char *p = valueData + strlen("updater:");
        if (snprintf_s(msg.update, MAX_UPDATE_SIZE, MAX_UPDATE_SIZE - 1, "%s", p) == -1) {
            INIT_LOGE("DoReboot updater: RBMiscWriteUpdaterMessage error");
            return;
        }
        msg.update[MAX_UPDATE_SIZE - 1] = 0;
        ret = RBMiscWriteUpdaterMessage(miscFile, &msg);
        if(true != ret) {
            INIT_LOGE("DoReboot updater: RBMiscWriteUpdaterMessage error");
            return;
        }
        ret = reboot(RB_AUTOBOOT);
        if (ret != 0) {
            INIT_LOGE("DoReboot updater: reboot(RB_AUTOBOOT) failed! syscall ret %d, err %d.", ret, errno);
        }
        return;
    }
    if (strlen(valueData) == strlen("updater") && strncmp(valueData, "updater", strlen("updater")) == 0) {
        ret = RBMiscWriteUpdaterMessage(miscFile, &msg);
        if(true != ret) {
            INIT_LOGE("DoReboot updater RBMiscWriteUpdaterMessage error");
            return;
        }
        ret = reboot(RB_AUTOBOOT);
        if (ret != 0) {
            INIT_LOGE("DoReboot updater reboot(RB_AUTOBOOT) failed! syscall ret %d, err %d.", ret, errno);
        }
        return;
    }
    if (strlen(valueData) == strlen("NoArgument") && strncmp(valueData, "NoArgument", strlen("NoArgument")) == 0) {
        ret = reboot(RB_AUTOBOOT);
        if (ret != 0) {
            INIT_LOGE("DoReboot updater: reboot(RB_AUTOBOOT) failed! syscall ret %d, err %d.", ret, errno);
        }
        return;
    }
    INIT_LOGE("DoReboot value = %s, error.", value);
    return;

}

