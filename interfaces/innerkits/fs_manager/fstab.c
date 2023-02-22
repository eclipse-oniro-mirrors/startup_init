/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <sys/types.h>
#include "beget_ext.h"
#include "fs_manager/fs_manager.h"
#include "init_utils.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

struct FsManagerFlags {
    char *name;
    unsigned int flags;
};

struct MountFlags {
    char *name;
    unsigned long flags;
};

static char *g_fscryptPolicy = NULL;

static unsigned int ConvertFlags(char *flagBuffer)
{
    static struct FsManagerFlags fsFlags[] = {
        {"check", FS_MANAGER_CHECK},
        {"wait", FS_MANAGER_WAIT},
        {"required", FS_MANAGER_REQUIRED},
        {"nofail", FS_MANAGER_NOFAIL},
#ifdef SUPPORT_HVB
        {"hvb", FS_MANAGER_HVB},
#endif
    };

    BEGET_CHECK_RETURN_VALUE(flagBuffer != NULL && *flagBuffer != '\0', 0); // No valid flags.
    int flagCount = 0;
    unsigned int flags = 0;
    const int maxCount = 3;
    char **vector = SplitStringExt(flagBuffer, ",", &flagCount, maxCount);
    BEGET_CHECK_RETURN_VALUE(vector != NULL && flagCount != 0, 0);
    for (size_t i = 0; i < ARRAY_LENGTH(fsFlags); i++) {
        for (int j = 0; j < flagCount; j++) {
            if (strcmp(fsFlags[i].name, vector[j]) == 0) {
                flags |= fsFlags[i].flags;
            }
        }
    }
    FreeStringVector(vector, flagCount);
    return flags;
}

static int AddToFstab(Fstab *fstab, FstabItem *item)
{
    if (fstab == NULL || item == NULL) {
        return -1;
    }
    if (fstab->head != NULL) {
        item->next = fstab->head->next;
        fstab->head->next = item;
    } else {
        fstab->head = item;
    }
    return 0;
}

void ReleaseFstabItem(FstabItem *item)
{
    if (item != NULL) {
        if (item->deviceName != NULL) {
            free(item->deviceName);
            item->deviceName = NULL;
        }

        if (item->mountPoint != NULL) {
            free(item->mountPoint);
            item->mountPoint = NULL;
        }

        if (item->fsType != NULL) {
            free(item->fsType);
            item->fsType = NULL;
        }

        if (item->mountOptions != NULL) {
            free(item->mountOptions);
            item->mountOptions = NULL;
        }

        free(item);
    }
}

void ReleaseFstab(Fstab *fstab)
{
    if (fstab != NULL) {
        FstabItem *item = fstab->head;
        while (item != NULL) {
            FstabItem *tmp = item->next;
            ReleaseFstabItem(item);
            item = tmp;
        }
        free(fstab);
        fstab = NULL;
    }
}

int ParseFstabPerLine(char *str, Fstab *fstab, bool procMounts, const char *separator)
{
    BEGET_CHECK_RETURN_VALUE(str != NULL && fstab != NULL, -1);
    char *rest = NULL;
    FstabItem *item = NULL;
    char *p = NULL;

    if (separator == NULL || *separator == '\0') {
        BEGET_LOGE("Invalid separator for parsing fstab");
        return -1;
    }

    if ((item = (FstabItem *)calloc(1, sizeof(FstabItem))) == NULL) {
        errno = ENOMEM;
        BEGET_LOGE("Allocate memory for FS table item failed, err = %d", errno);
        return -1;
    }

    do {
        if ((p = strtok_r(str, separator, &rest)) == NULL) {
            BEGET_LOGE("Failed to parse block device.");
            break;
        }
        item->deviceName = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            BEGET_LOGE("Failed to parse mount point.");
            break;
        }
        item->mountPoint = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            BEGET_LOGE("Failed to parse fs type.");
            break;
        }
        item->fsType = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            BEGET_LOGE("Failed to parse mount options.");
            break;
        }
        item->mountOptions = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            BEGET_LOGE("Failed to parse fs manager flags.");
            break;
        }
        // @fsManagerFlags only for fstab
        // Ignore it if we read from /proc/mounts
        if (!procMounts) {
            item->fsManagerFlags = ConvertFlags(p);
        } else {
            item->fsManagerFlags = 0;
        }
        return AddToFstab(fstab, item);
    } while (0);

    ReleaseFstabItem(item);
    item = NULL;
    return -1;
}

Fstab *ReadFstabFromFile(const char *file, bool procMounts)
{
    char *line = NULL;
    size_t allocn = 0;
    ssize_t readn = 0;
    Fstab *fstab = NULL;

    FILE *fp = NULL;
    char *realPath = GetRealPath(file);
    if (realPath != NULL) {
        fp = fopen(realPath, "r");
        free(realPath);
    } else {
        fp = fopen(file, "r"); // no file system, can not get real path
    }
    if (fp == NULL) {
        BEGET_LOGE("Open %s failed, err = %d", file, errno);
        return NULL;
    }

    if ((fstab = (Fstab *)calloc(1, sizeof(Fstab))) == NULL) {
        BEGET_LOGE("Allocate memory for FS table failed, err = %d", errno);
        fclose(fp);
        fp = NULL;
        return NULL;
    }

    // Record line number of fstab file
    size_t ln = 0;
    while ((readn = getline(&line, &allocn, fp)) != -1) {
        char *p = NULL;
        ln++;
        if (line[readn - 1] == '\n') {
            line[readn - 1] = '\0';
        }
        p = line;
        while (isspace(*p)) {
            p++;
        }

        if (*p == '\0' || *p == '#') {
            continue;
        }

        if (ParseFstabPerLine(p, fstab, procMounts, " \t") < 0) {
            if (errno == ENOMEM) {
                // Ran out of memory, there is no reason to continue.
                break;
            }
            // If one line in fstab file parsed with a failure. just give a warning
            // and skip it.
            BEGET_LOGW("Cannot parse file \" %s \" at line %zu. skip it", file, ln);
            continue;
        }
    }
    if (line != NULL) {
        free(line);
    }
    (void)fclose(fp);
    fp = NULL;
    return fstab;
}

FstabItem *FindFstabItemForMountPoint(Fstab fstab, const char *mp)
{
    FstabItem *item = NULL;
    if (mp != NULL) {
        for (item = fstab.head; item != NULL; item = item->next) {
            if ((item->mountPoint != NULL) && (strcmp(item->mountPoint, mp) == 0)) {
                break;
            }
        }
    }
    return item;
}

FstabItem *FindFstabItemForPath(Fstab fstab, const char *path)
{
    FstabItem *item = NULL;

    if (path == NULL || *path != '/') {
        return NULL;
    }

    char tmp[PATH_MAX] = {0};
    char *dir = NULL;
    if (strncpy_s(tmp, PATH_MAX - 1,  path, strlen(path)) != EOK) {
        BEGET_LOGE("Failed to copy path.");
        return NULL;
    }

    dir = tmp;
    while (true) {
        item = FindFstabItemForMountPoint(fstab, dir);
        if (item != NULL) {
            break;
        }
        dir = dirname(dir);
        // Reverse walk through path and met "/", just quit.
        if (dir == NULL || strcmp(dir, "/") == 0) {
            break;
        }
    }
    return item;
}

static char *GetFstabFile(char *fileName, size_t size)
{
    if (InUpdaterMode() == 1) {
        if (strncpy_s(fileName, size, "/etc/fstab.updater", strlen("/etc/fstab.updater")) != 0) {
            BEGET_LOGE("Failed strncpy_s err=%d", errno);
            return NULL;
        }
    } else {
        char hardware[MAX_BUFFER_LEN] = {0};
        char *buffer = ReadFileData("/proc/cmdline");
        if (buffer == NULL) {
            BEGET_LOGE("Failed to read \"/proc/cmdline\"");
            return NULL;
        }
        int ret = GetProcCmdlineValue("hardware", buffer, hardware, MAX_BUFFER_LEN);
        free(buffer);
        if (ret != 0) {
            BEGET_LOGE("Failed get hardware from cmdline");
            return NULL;
        }
        if (snprintf_s(fileName, size, size - 1, "/vendor/etc/fstab.%s", hardware) == -1) {
            BEGET_LOGE("Failed to build fstab file, err=%d", errno);
            return NULL;
        }
    }
    BEGET_LOGI("fstab file is %s", fileName);
    return fileName;
}

int GetBlockDeviceByMountPoint(const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen)
{
    if (fstab == NULL || mountPoint == NULL || *mountPoint == '\0' || deviceName == NULL) {
        return -1;
    }
    FstabItem *item = FindFstabItemForMountPoint(*fstab, mountPoint);
    if (item == NULL) {
        BEGET_LOGE("Failed to get fstab item from mount point \" %s \"", mountPoint);
        return -1;
    }
    if (strncpy_s(deviceName, nameLen, item->deviceName, strlen(item->deviceName)) != 0) {
        BEGET_LOGE("Failed to copy block device name, err=%d", errno);
        return -1;
    }
    return 0;
}

int GetBlockDeviceByName(const char *deviceName, const Fstab *fstab, char* miscDev, size_t size)
{
    for (FstabItem *item = fstab->head; item != NULL; item = item->next) {
        if (strstr(item->deviceName, deviceName) != NULL) {
            BEGET_CHECK_RETURN_VALUE(strcpy_s(miscDev, size, item->deviceName) != 0, 0);
        }
    }
    return -1;
}

static const struct MountFlags MOUNT_FLAGS[] = {
    { "noatime", MS_NOATIME },
    { "noexec", MS_NOEXEC },
    { "nosuid", MS_NOSUID },
    { "nodev", MS_NODEV },
    { "nodiratime", MS_NODIRATIME },
    { "ro", MS_RDONLY },
    { "rw", 0 },
    { "sync", MS_SYNCHRONOUS },
    { "remount", MS_REMOUNT },
    { "bind", MS_BIND },
    { "rec", MS_REC },
    { "unbindable", MS_UNBINDABLE },
    { "private", MS_PRIVATE },
    { "slave", MS_SLAVE },
    { "shared", MS_SHARED },
    { "defaults", 0 },
};

static bool IsDefaultMountFlags(const char *str)
{
    bool isDefault = false;

    if (str != NULL) {
        for (size_t i = 0; i < ARRAY_LENGTH(MOUNT_FLAGS); i++) {
            if (strcmp(str, MOUNT_FLAGS[i].name) == 0) {
                isDefault = true;
            }
        }
    }
    return isDefault;
}

static unsigned long ParseDefaultMountFlag(const char *str)
{
    unsigned long flags = 0;

    if (str != NULL) {
        for (size_t i = 0; i < ARRAY_LENGTH(MOUNT_FLAGS); i++) {
            if (strcmp(str, MOUNT_FLAGS[i].name) == 0) {
                flags = MOUNT_FLAGS[i].flags;
                break;
            }
        }
    }
    return flags;
}

static bool IsFscryptOption(const char *option)
{
    if (!option) {
        return false;
    }
    char *fscryptPre = "fscrypt=";
    if (strncmp(option, fscryptPre, strlen(fscryptPre)) == 0) {
        return true;
    }
    return false;
}

static void StoreFscryptPolicy(const char *option)
{
    if (option == NULL) {
        return;
    }
    if (g_fscryptPolicy != NULL) {
        BEGET_LOGW("StoreFscryptPolicy:inited policy is not empty");
        free(g_fscryptPolicy);
    }
    g_fscryptPolicy = strdup(option);
    if (g_fscryptPolicy == NULL) {
        BEGET_LOGE("StoreFscryptPolicy:no memory");
        return;
    }
    BEGET_LOGI("StoreFscryptPolicy:store fscrypt policy, %s", option);
}

int LoadFscryptPolicy(char *buf, size_t size)
{
    BEGET_LOGI("LoadFscryptPolicy start");
    if (buf == NULL || g_fscryptPolicy == NULL) {
        BEGET_LOGE("LoadFscryptPolicy:buf or fscrypt policy is empty");
        return -ENOMEM;
    }
    if (size == 0) {
        BEGET_LOGE("LoadFscryptPloicy:size is invalid");
        return -EINVAL;
    }
    if (strcpy_s(buf, size, g_fscryptPolicy) != 0) {
        BEGET_LOGE("loadFscryptPolicy:strcmp failed, error = %d", errno);
        return -EFAULT;
    }
    free(g_fscryptPolicy);
    g_fscryptPolicy = NULL;
    BEGET_LOGI("LoadFscryptPolicy success");

    return 0;
}

unsigned long GetMountFlags(char *mountFlag, char *fsSpecificData, size_t fsSpecificDataSize,
    const char *mountPoint)
{
    unsigned long flags = 0;
    BEGET_CHECK_RETURN_VALUE(mountFlag != NULL && fsSpecificData != NULL, 0);
    int flagCount = 0;
    // Why max count of mount flags is 15?
    // There are lots for mount flags defined in sys/mount.h
    // But we only support to parse 15 in @ParseDefaultMountFlags() function
    // So set default mount flag number to 15.
    // If the item configured in fstab contains flag over than 15,
    // @SplitStringExt can handle it and parse them all. but the parse function will drop it.
    const int maxCount = 15;
    char **flagsVector = SplitStringExt(mountFlag, ",", &flagCount, maxCount);

    if (flagsVector == NULL || flagCount == 0) {
        // No flags or something wrong in SplitStringExt，just return.
        return 0;
    }

    for (int i = 0; i < flagCount; i++) {
        char *p = flagsVector[i];
        if (IsDefaultMountFlags(p)) {
            flags |= ParseDefaultMountFlag(p);
        } else {
            if (IsFscryptOption(p) &&
                !strncmp(mountPoint, "/data", strlen("/data"))) {
                StoreFscryptPolicy(p + strlen("fscrypt="));
                continue;
            }
            if (strncat_s(fsSpecificData, fsSpecificDataSize - 1, p, strlen(p)) != EOK) {
                BEGET_LOGW("Failed to append mount flag \" %s \", ignore it.", p);
                continue;
            }
            if (i == flagCount - 1) { // last flags, do not need to append ','
                break;
            }
            // Combined each mount flag with ','
            if (strncat_s(fsSpecificData, fsSpecificDataSize - 1, ",", 1) != EOK) {
                BEGET_LOGW("Failed to append comma");
                break; // If cannot add ',' to the end of flags, there is not reason to continue.
            }
        }
    }

    FreeStringVector(flagsVector, flagCount);
    return flags;
}

int GetBlockDevicePath(const char *partName, char *path, size_t size)
{
    BEGET_CHECK_RETURN_VALUE(partName != NULL && path != NULL, -1);
    Fstab *fstab = LoadFstabFromCommandLine();
    if (fstab == NULL) {
        BEGET_LOGI("fstab not found from cmdline, try to get it from file");
        char *fstabFile = GetFstabFile(path, size);
        BEGET_CHECK_RETURN_VALUE(fstabFile != NULL, -1);
        fstab = ReadFstabFromFile(fstabFile, false);
    }
    BEGET_CHECK_RETURN_VALUE(fstab != NULL, -1);
    int ret = GetBlockDeviceByMountPoint(partName, fstab, path, size);
    BEGET_INFO_CHECK(ret == 0, ret = GetBlockDeviceByName(partName, fstab, path, size),
        "Mount point not found, try to get path by device name.");
    ReleaseFstab(fstab);
    return ret;
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
    char partName[NAME_SIZE] = {};
    // Sanity checks
    BEGET_CHECK(!(item == NULL || *item == '\0' || fstab == NULL), return -1);

    char *p = NULL;
    if ((p = strstr(item, "=")) != NULL) {
        const char *q = item + strlen(OHOS_REQUIRED_MOUNT_PREFIX); // Get partition name
        BEGET_CHECK(!(q == NULL || *q == '\0' || (p - q) <= 0), return -1);
        BEGET_ERROR_CHECK(strncpy_s(partName, NAME_SIZE -1, q, p - q) == EOK,
            return -1, "Failed to copy required partition name");
        p++; // skip '='
        BEGET_ERROR_CHECK(strncpy_s(mountOptions, MAX_BUFFER_LEN -1, p, strlen(p)) == EOK,
            return -1, "Failed to copy required mount info: %s", item);
    }
    BEGET_LOGV("Mount option of partition %s is [%s]", partName, mountOptions);
    if (ParseFstabPerLine(mountOptions, fstab, false, "@") < 0) {
        BEGET_LOGE("Failed to parse mount options of partition \' %s \', options: %s", partName, mountOptions);
        return -1;
    }
    return 0;
}

Fstab* LoadFstabFromCommandLine(void)
{
    Fstab *fstab = NULL;
    char *cmdline = ReadFileData(BOOT_CMD_LINE);
    bool isDone = false;

    BEGET_ERROR_CHECK(cmdline != NULL, return NULL, "Read from \'/proc/cmdline\' failed, err = %d", errno);
    TrimTail(cmdline, '\n');
    BEGET_ERROR_CHECK((fstab = (Fstab *)calloc(1, sizeof(Fstab))) != NULL, return NULL,
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
            BEGET_LOGE("Failed to parse \' %s \'", start);
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
            BEGET_LOGE("Failed to parse \' %s \'", start);
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
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
