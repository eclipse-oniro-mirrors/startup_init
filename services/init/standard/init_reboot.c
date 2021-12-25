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
#include <limits.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#include "fs_manager/fs_manager.h"
#include "init_log.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"

#ifdef PRODUCT_RK
#include <sys/syscall.h>

#define REBOOT_MAGIC1       0xfee1dead
#define REBOOT_MAGIC2       672274793
#define REBOOT_CMD_RESTART2 0xA1B2C3D4
#endif

#define MAX_VALUE_LENGTH 500
#define MAX_COMMAND_SIZE 20
#define MAX_UPDATE_SIZE 100

struct RBMiscUpdateMessage {
    char command[MAX_COMMAND_SIZE];
    char update[MAX_UPDATE_SIZE];
};

static int RBMiscWriteUpdaterMessage(const char *path, const struct RBMiscUpdateMessage *boot)
{
    char *realPath = GetRealPath(path);
    INIT_CHECK_RETURN_VALUE(realPath != NULL, -1);
    int ret = 0;
    FILE *fp = fopen(realPath, "rb+");
    free(realPath);
    realPath = NULL;
    if (fp != NULL) {
        size_t writeLen = fwrite(boot, sizeof(struct RBMiscUpdateMessage), 1, fp);
        INIT_ERROR_CHECK(writeLen == 1, ret = -1, "Failed to write misc for reboot");
        (void)fclose(fp);
    } else {
        ret = -1;
        INIT_LOGE("Failed to open %s", path);
    }
    return ret;
}

static int RBMiscReadUpdaterMessage(const char *path, struct RBMiscUpdateMessage *boot)
{
    char *realPath = GetRealPath(path);
    INIT_CHECK_RETURN_VALUE(realPath != NULL, -1);
    int ret = 0;
    FILE *fp = fopen(realPath, "rb");
    free(realPath);
    realPath = NULL;
    if (fp != NULL) {
        size_t readLen = fread(boot, 1, sizeof(struct RBMiscUpdateMessage), fp);
        (void)fclose(fp);
        INIT_ERROR_CHECK(readLen > 0, ret = -1, "Failed to read misc for reboot");
    } else {
        ret = -1;
        INIT_LOGE("Failed to open %s", path);
    }

    return ret;
}

static int CheckAndRebootToUpdater(const char *valueData, const char *cmd,
    const char *cmdExt, const char *boot, const char *miscFile)
{
    // "updater" or "updater:"
    struct RBMiscUpdateMessage msg;
    int ret = RBMiscReadUpdaterMessage(miscFile, &msg);
    INIT_ERROR_CHECK(ret == 0, return -1, "Failed to get misc info for %s.", cmd);

    if (boot != NULL) {
        ret = snprintf_s(msg.command, MAX_COMMAND_SIZE, MAX_COMMAND_SIZE - 1, "%s", boot);
        INIT_ERROR_CHECK(ret > 0, return -1, "Failed to format cmd for %s.", cmd);
        msg.command[MAX_COMMAND_SIZE - 1] = 0;
    } else {
        ret = memset_s(msg.command, MAX_COMMAND_SIZE, 0, MAX_COMMAND_SIZE);
        INIT_ERROR_CHECK(ret == 0, return -1, "Failed to format cmd for %s.", cmd);
    }

    if ((cmdExt != NULL) && (valueData != NULL) && (strncmp(valueData, cmdExt, strlen(cmdExt)) == 0)) {
        const char *p = valueData + strlen(cmdExt);
        ret = snprintf_s(msg.update, MAX_UPDATE_SIZE, MAX_UPDATE_SIZE - 1, "%s", p);
        INIT_ERROR_CHECK(ret > 0, return -1, "Failed to format param for %s.", cmd);
        msg.update[MAX_UPDATE_SIZE - 1] = 0;
    } else {
        ret = memset_s(msg.update, MAX_UPDATE_SIZE, 0, MAX_UPDATE_SIZE);
        INIT_ERROR_CHECK(ret == 0, return -1, "Failed to format update for %s.", cmd);
    }

    ret = -1;
    if (RBMiscWriteUpdaterMessage(miscFile, &msg) == 0) {
        ret = 0;
#ifndef STARTUP_INIT_TEST
        ret = reboot(RB_AUTOBOOT);
#endif
    }
    return ret;
}

static int CheckRebootParam(const char *valueData)
{
    if (valueData == NULL) {
        return 0;
    }
    static const char *cmdParams[] = {
        "shutdown", "updater", "updater:", "flash", "flash:", "NoArgument", "loader", "bootloader"
    };
    size_t i = 0;
    for (; i < ARRAY_LENGTH(cmdParams); i++) {
        if (strncmp(valueData, cmdParams[i], strlen(cmdParams[i])) == 0) {
            break;
        }
    }
    INIT_CHECK_RETURN_VALUE(i < ARRAY_LENGTH(cmdParams), -1);
    return 0;
}

void ExecReboot(const char *value)
{
    INIT_ERROR_CHECK(value != NULL && strlen(value) <= MAX_VALUE_LENGTH, return, "Invalid arg");
    const char *valueData = NULL;
    if (strncmp(value, "reboot,", strlen("reboot,")) == 0) {
        valueData = value + strlen("reboot,");
    } else if (strcmp(value, "reboot") != 0) {
        INIT_LOGE("Reboot value = %s, must started with reboot", value);
        return;
    }
    INIT_ERROR_CHECK(CheckRebootParam(valueData) == 0, return, "Invalid arg %s for reboot.", value);
    char miscDevice[PATH_MAX] = {0};
    char *fstabFile = GetFstabFile();
    if (fstabFile != NULL) {
        Fstab *fstab = ReadFstabFromFile(fstabFile, false);
        free(fstabFile);
        if (fstab != NULL) {
            INIT_CHECK_ONLY_ELOG(GetBlockDeviceByMountPoint("/misc", fstab, miscDevice, PATH_MAX) == 0,
                "Failed to get misc device name.");
            ReleaseFstab(fstab);
        }
    }
    StopAllServices(SERVICE_ATTR_INVALID);
    sync();
    INIT_CHECK_ONLY_ELOG(GetMountStatusForMountPoint("/vendor") == 0 || umount("/vendor") == 0,
        "Failed to umount vendor. errno = %d.", errno);
    INIT_CHECK_ONLY_ELOG(GetMountStatusForMountPoint("/data") == 0 || umount("/data") == 0 ||
        umount2("/data", MNT_FORCE) == 0, "Failed umount data. errno = %d.", errno);
    int ret = 0;
    if (valueData == NULL) {
#ifndef PRODUCT_RK
        ret = CheckAndRebootToUpdater(NULL, "reboot", NULL, NULL, miscDevice);
#else
        reboot(RB_AUTOBOOT);
#endif
    } else if (strcmp(valueData, "shutdown") == 0) {
#ifndef STARTUP_INIT_TEST
        ret = reboot(RB_POWER_OFF);
    } else if (strcmp(valueData, "bootloader") == 0) {
        ret = reboot(RB_POWER_OFF);
#endif
    } else if (strncmp(valueData, "updater", strlen("updater")) == 0) {
        ret = CheckAndRebootToUpdater(valueData, "updater", "updater:", "boot_updater", miscDevice);
    } else if (strncmp(valueData, "flash", strlen("flash")) == 0) {
        ret = CheckAndRebootToUpdater(valueData, "flash", "flash:", "boot_flash", miscDevice);
#ifdef PRODUCT_RK
    } else if (strncmp(valueData, "loader", strlen("loader")) == 0) {
        syscall(__NR_reboot, REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_CMD_RESTART2, "loader");
#endif
    }
    INIT_LOGI("Reboot %s %s.", value, (ret == 0) ? "success" : "fail");
    return;
}
