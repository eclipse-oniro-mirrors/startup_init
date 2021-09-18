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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/reboot.h>
#include <unistd.h>
#include "init_log.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"

#define MAX_VALUE_LENGTH 500
#define MAX_COMMAND_SIZE 20
#define MAX_UPDATE_SIZE 100

struct RBMiscUpdateMessage {
    char command[MAX_COMMAND_SIZE];
    char update[MAX_UPDATE_SIZE];
};

static bool RBMiscWriteUpdaterMessage(const char *path, const struct RBMiscUpdateMessage *boot)
{
    if (path == NULL || boot == NULL) {
        INIT_LOGE("path or boot is NULL.");
        return false;
    }

    char realPath[PATH_MAX] = {0};
    if (Realpath(path, realPath, sizeof(realPath)) == NULL) {
        return false;
    }
    FILE *fp = fopen(realPath, "rb+");
    if (fp == NULL) {
        INIT_LOGE("open %s failed", path);
        return false;
    }

    size_t ret = fwrite(boot, sizeof(struct RBMiscUpdateMessage), 1, fp);
    if (ret < 0) {
        INIT_LOGE("write to misc failed");
        (void)fclose(fp);
        return false;
    }
    (void)fclose(fp);
    return true;
}

static bool RBMiscReadUpdaterMessage(const char *path, struct RBMiscUpdateMessage *boot)
{
    if (path == NULL || boot == NULL) {
        INIT_LOGE("path or boot is NULL.");
        return false;
    }
    char realPath[PATH_MAX] = {0};
    if (Realpath(path, realPath, sizeof(realPath)) == NULL) {
        return false;
    }
    FILE *fp = fopen(realPath, "rb");
    if (fp == NULL) {
        INIT_LOGE("open %s failed", path);
        return false;
    }

    size_t ret = fread(boot, 1, sizeof(struct RBMiscUpdateMessage), fp);
    if (ret <= 0) {
        INIT_LOGE("read to misc failed");
        (void)fclose(fp);
        return false;
    }
    (void)fclose(fp);
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

    while (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
        n = strlen(buffer);
        if (buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
        }
        if (strstr(buffer, mountPoint) != NULL) {
            (void)fclose(fp);
            return 1;
        }
    }

    // Cannot find it from system.
    (void)fclose(fp);
    return 0;
}

static int CheckAndRebootToUpdater(const char *valueData, const char *cmd, const char *cmdExt, const char *boot)
{
    // "updater" or "updater:"
    const char *miscFile = "/dev/block/platform/soc/10100000.himci.eMMC/by-name/misc";
    struct RBMiscUpdateMessage msg;
    bool bRet = RBMiscReadUpdaterMessage(miscFile, &msg);
    INIT_ERROR_CHECK(bRet, return -1, "Failed to get misc info for %s.", cmd);

    int ret = snprintf_s(msg.command, MAX_COMMAND_SIZE, MAX_COMMAND_SIZE - 1, "%s", boot);
    INIT_ERROR_CHECK(ret > 0, return -1, "Failed to format cmd for %s.", cmd);
    msg.command[MAX_COMMAND_SIZE - 1] = 0;

    if (strncmp(valueData, cmdExt, strlen(cmdExt)) == 0) {
        const char *p = valueData + strlen(cmdExt);
        ret = snprintf_s(msg.update, MAX_UPDATE_SIZE, MAX_UPDATE_SIZE - 1, "%s", p);
        INIT_ERROR_CHECK(ret > 0, return -1, "Failed to format param for %s.", cmd);
        msg.update[MAX_UPDATE_SIZE - 1] = 0;
    }

    if (RBMiscWriteUpdaterMessage(miscFile, &msg)) {
        return reboot(RB_AUTOBOOT);
    }
    return -1;
}

static int CheckRebootValue(const char **cmdParams, const char *valueData)
{
    size_t i = 0;
    for (; i < ARRAY_LENGTH(cmdParams); i++) {
        if (strncmp(valueData, cmdParams[i], strlen(cmdParams[i])) == 0) {
            break;
        }
    }
    if (i >= ARRAY_LENGTH(cmdParams)) {
        INIT_LOGE("DoReboot valueData = %s, parameters error.", valueData);
        return -1;
    }
    return 0;
}

void DoReboot(const char *value)
{
#ifndef OHOS_LITE
    static const char *g_cmdParams[] = {
        "shutdown", "updater", "updater:", "flashing", "flashing:", "NoArgument", "bootloader"
    };
    if (value == NULL || strlen(value) > MAX_VALUE_LENGTH) {
        INIT_LOGE("DoReboot value = NULL");
        return;
    }
    const char *valueData = NULL;
    if (strcmp(value, "reboot") == 0) {
        valueData = NULL;
    } else if (strncmp(value, "reboot,", strlen("reboot,")) != 0) {
        INIT_LOGE("DoReboot reboot value = %s, must started with reboot ,error.", value);
        return;
    } else {
        valueData = value + strlen("reboot,");
    }
    if (valueData != NULL) {
        if (CheckRebootValue(g_cmdParams, valueData) < 0) {
            return;
        }
    }
    StopAllServicesBeforeReboot();
    sync();
    if (GetMountStatusForMountPoint("/vendor") != 0 && umount("/vendor") != 0) {
        INIT_LOGE("DoReboot umount vendor failed! errno = %d.", errno);
    }
    if (GetMountStatusForMountPoint("/data") != 0 && umount("/data") != 0) {
        INIT_LOGE("DoReboot umount data failed! errno = %d.", errno);
    }
    int ret = 0;
    if (valueData == NULL) {
        ret = reboot(RB_AUTOBOOT);
    } else if (strcmp(valueData, "shutdown") == 0) {
        ret = reboot(RB_POWER_OFF);
    } else if (strcmp(valueData, "bootloader") == 0) {
        ret = reboot(RB_POWER_OFF);
    } else if (strncmp(valueData, "updater", strlen("updater")) == 0) {
        ret = CheckAndRebootToUpdater(valueData, "updater", "updater:", "boot_updater");
    } else if (strncmp(valueData, "flashing", strlen("flashing")) == 0) {
        ret = CheckAndRebootToUpdater(valueData, "flashing", "flashing:", "boot_flashing");
    }
    INIT_LOGI("DoReboot value = %s %s.", value, (ret == 0) ? "success" : "fail");
#else
    int ret = reboot(RB_AUTOBOOT);
    if (ret != 0) {
        INIT_LOGE("reboot failed! syscall ret %d, err %d.", ret, errno);
    }
#endif
    return;
}
