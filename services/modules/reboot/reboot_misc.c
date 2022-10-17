/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "reboot_adp.h"
#include "fs_manager/fs_manager.h"
#include "init_utils.h"
#include "securec.h"

#define MAX_COMMAND_SIZE 32
#define MAX_UPDATE_SIZE 1280
#define MAX_RESERVED_SIZE 736

struct RBMiscUpdateMessage {
    char command[MAX_COMMAND_SIZE];
    char update[MAX_UPDATE_SIZE];
    char reserved[MAX_RESERVED_SIZE];
};

static int RBMiscWriteUpdaterMessage(const char *path, const struct RBMiscUpdateMessage *boot)
{
    char *realPath = GetRealPath(path);
    BEGET_CHECK_RETURN_VALUE(realPath != NULL, -1);
    int ret = -1;
    FILE *fp = fopen(realPath, "rb+");
    free(realPath);
    realPath = NULL;
    if (fp != NULL) {
        size_t writeLen = fwrite(boot, sizeof(struct RBMiscUpdateMessage), 1, fp);
        BEGET_ERROR_CHECK(writeLen == 1, ret = -1, "Failed to write misc for reboot");
        (void)fclose(fp);
        ret = 0;
    }
    return ret;
}

static int RBMiscReadUpdaterMessage(const char *path, struct RBMiscUpdateMessage *boot)
{
    int ret = -1;
    FILE *fp = NULL;
    char *realPath = GetRealPath(path);
    if (realPath != NULL) {
        fp = fopen(realPath, "rb");
        free(realPath);
        realPath = NULL;
    } else {
        fp = fopen(path, "rb");
    }
    if (fp != NULL) {
        size_t readLen = fread(boot, 1, sizeof(struct RBMiscUpdateMessage), fp);
        (void)fclose(fp);
        BEGET_ERROR_CHECK(readLen > 0, ret = -1, "Failed to read misc for reboot");
        ret = 0;
    }
    return ret;
}

int GetRebootReasonFromMisc(char *reason, size_t size)
{
    char miscFile[PATH_MAX] = {0};
    int ret = GetBlockDevicePath("/misc", miscFile, PATH_MAX);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to get misc path");
    struct RBMiscUpdateMessage msg;
    ret = RBMiscReadUpdaterMessage(miscFile, &msg);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to get misc info");
    return strcpy_s(reason, size, msg.command);
}

int UpdateMiscMessage(const char *valueData, const char *cmd, const char *cmdExt, const char *boot)
{
    char miscFile[PATH_MAX] = {0};
    int ret = GetBlockDevicePath("/misc", miscFile, PATH_MAX);
    // no misc do not updater, so return ok
    BEGET_ERROR_CHECK(ret == 0, return 0, "Failed to get misc path for %s.", valueData);

    // "updater" or "updater:"
    struct RBMiscUpdateMessage msg;
    ret = RBMiscReadUpdaterMessage(miscFile, &msg);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to get misc info for %s.", cmd);

    if (boot != NULL) {
        ret = snprintf_s(msg.command, MAX_COMMAND_SIZE, MAX_COMMAND_SIZE - 1, "%s", boot);
        BEGET_ERROR_CHECK(ret > 0, return -1, "Failed to format cmd for %s.", cmd);
        msg.command[MAX_COMMAND_SIZE - 1] = 0;
    } else {
        ret = memset_s(msg.command, MAX_COMMAND_SIZE, 0, MAX_COMMAND_SIZE);
        BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to format cmd for %s.", cmd);
    }

    if (strncmp(cmd, "updater", strlen("updater")) != 0) {
        if ((cmdExt != NULL) && (valueData != NULL) && (strncmp(valueData, cmdExt, strlen(cmdExt)) == 0)) {
            const char *p = valueData + strlen(cmdExt);
            ret = snprintf_s(msg.update, MAX_UPDATE_SIZE, MAX_UPDATE_SIZE - 1, "%s", p);
            BEGET_ERROR_CHECK(ret > 0, return -1, "Failed to format param for %s.", cmd);
            msg.update[MAX_UPDATE_SIZE - 1] = 0;
        } else {
            ret = memset_s(msg.update, MAX_UPDATE_SIZE, 0, MAX_UPDATE_SIZE);
            BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to format update for %s.", cmd);
        }
    }

    if (RBMiscWriteUpdaterMessage(miscFile, &msg) == 0) {
        return 0;
    }
    return -1;
}
