/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "init_mount.h"

#include <errno.h>
#include <stdbool.h>
#include "fs_manager/fs_manager.h"
#include "init_cmds.h"
#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

int MountRequriedPartitions(const Fstab *fstab)
{
    INIT_ERROR_CHECK(fstab != NULL, return -1, "fstab is NULL");
    int rc;
    INIT_LOGI("Mount required partitions");
    rc = MountAllWithFstab(fstab, 1);
    return rc;
}

#define OHOS_REQUIRED_MOUNT_PREFIX "ohos.required_mount."
/*
 * Fstab includes block device node, mount point, file system type, MNT_ Flags and options.
 * We separate them by spaces in fstab.required file, but the separator is '@' in CmdLine.
 * The prefix "ohos.required_mount." is the flag of required fstab information in CmdLine.
 * Format as shown below:
 * <block device>@<mount point>@<fstype>@<mount options>@<fstab options>
 * e.g.
 * ohos.required_mount.system=/dev/block/xxx/by-name/system@/usr@ext4@ro,barrier=1@wait,required
 */
static int ParseRequiredMountInfo(const char *item, Fstab *fstab)
{
    char mountOptions[MAX_BUFFER_LEN] = {};
    char partName[PARTITION_NAME_SIZE] = {};
    // Sanity checks
    INIT_CHECK(!(item == NULL || *item == '\0' || fstab == NULL), return -1);

    char *p = NULL;
    const char *q = item;
    if ((p = strstr(item, "=")) != NULL) {
        q = item + strlen(OHOS_REQUIRED_MOUNT_PREFIX); // Get partition name
        INIT_CHECK(!(q == NULL || *q == '\0' || (p - q) <= 0), return -1);
        INIT_ERROR_CHECK(strncpy_s(partName, PARTITION_NAME_SIZE -1, q, p - q) == EOK,
            return -1, "Failed to copy requried partition name");
        p++; // skip '='
        INIT_ERROR_CHECK(strncpy_s(mountOptions, MAX_BUFFER_LEN -1, p, strlen(p)) == EOK,
            return -1, "Failed to copy requried mount info: %s", item);
    }
    INIT_LOGV("Mount option of partition %s is [%s]", partName, mountOptions);
    if (ParseFstabPerLine(mountOptions, fstab, false, "@") < 0) {
        INIT_LOGE("Failed to parse mount options of partition \' %s \', options: %s", partName, mountOptions);
        return -1;
    }
    return 0;
}

static Fstab* LoadFstabFromCommandLine(void)
{
    Fstab *fstab = NULL;
    char *cmdline = ReadFileData(BOOT_CMD_LINE);
    bool isDone = false;

    INIT_ERROR_CHECK(cmdline != NULL, return NULL, "Read from \'/proc/cmdline\' failed, err = %d", errno);
    INIT_ERROR_CHECK((fstab = (Fstab *)calloc(1, sizeof(Fstab))) != NULL, return NULL,
        "Allocate memory for FS table failed, err = %d", errno);
    char *start = cmdline;
    char *end = start + strlen(cmdline);
    while (start < end) {
        char *token = strstr(start, " ");
        if (token == NULL) {
            break;
        }

        // Startswith " "
        if (token == start) {
            start++;
            continue;
        }
        *token = '\0';
        if (strncmp(start, OHOS_REQUIRED_MOUNT_PREFIX,
            strlen(OHOS_REQUIRED_MOUNT_PREFIX)) != 0) {
            start = token + 1;
            continue;
        }
        isDone = true;
        if (ParseRequiredMountInfo(start, fstab) < 0) {
            INIT_LOGE("Failed to parse \' %s \'", start);
            isDone = false;
            break;
        }
        start = token + 1;
    }

    // handle last one
    if (start < end) {
        if (strncmp(start, OHOS_REQUIRED_MOUNT_PREFIX,
            strlen(OHOS_REQUIRED_MOUNT_PREFIX)) == 0 &&
            ParseRequiredMountInfo(start, fstab) < 0) {
            INIT_LOGE("Failed to parse \' %s \'", start);
            isDone = false;
        }
    }

    if (!isDone) {
        ReleaseFstab(fstab);
        fstab = NULL;
    }
    free(cmdline);
    return fstab;
}

Fstab* LoadRequiredFstab(void)
{
    Fstab *fstab = NULL;
    fstab = LoadFstabFromCommandLine();
    if (fstab == NULL) {
        INIT_LOGI("Cannot load fstab from command line, try read from fstab.required");
        const char *fstabFile = "/etc/fstab.required";
        INIT_CHECK(access(fstabFile, F_OK) == 0, fstabFile = "/system/etc/fstab.required");
#ifndef STARTUP_INIT_TEST
        INIT_ERROR_CHECK(access(fstabFile, F_OK) == 0, abort(), "Failed get fstab.required");
#endif
        fstab = ReadFstabFromFile(fstabFile, false);
    }
    return fstab;
}
