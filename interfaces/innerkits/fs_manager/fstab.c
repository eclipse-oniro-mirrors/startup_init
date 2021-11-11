/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "fs_manager/fs_manager.h"
#include "fs_manager/fs_manager_log.h"
#include "init_log.h"
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

unsigned int ConvertFlags(char *flagBuffer)
{
    static struct FsManagerFlags fsFlags[] = {
        {"check", FS_MANAGER_CHECK},
        {"wait", FS_MANAGER_WAIT},
        {"required", FS_MANAGER_REQUIRED},
    };

    INIT_CHECK_RETURN_VALUE(flagBuffer != NULL && *flagBuffer != '\0', 0); // No valid flags.
    int flagCount = 0;
    unsigned int flags = 0;
    const int maxCount = 3;
    char **vector = SplitStringExt(flagBuffer, ",", &flagCount, maxCount);
    INIT_CHECK_RETURN_VALUE(vector != NULL && flagCount != 0, 0);
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

static void AddToFstab(Fstab *fstab, FstabItem *item)
{
    if (fstab == NULL || item == NULL) {
        return;
    }
    if (fstab->head != NULL) {
        item->next = fstab->head->next;
        fstab->head->next = item;
    } else {
        fstab->head = item;
    }
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
        item = NULL;
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

static int ParseFstabPerLine(char *str, Fstab *fstab, bool procMounts)
{
    INIT_CHECK_RETURN_VALUE(str != NULL && fstab != NULL, -1);
    const char *separator = " \t";
    char *rest = NULL;
    FstabItem *item = NULL;
    char *p = NULL;

    if ((item = (FstabItem *)calloc(1, sizeof(FstabItem))) == NULL) {
        errno = ENOMEM;
        FSMGR_LOGE("Allocate memory for FS table item failed, err = %d", errno);
        return -1;
    }

    do {
        if ((p = strtok_r(str, separator, &rest)) == NULL) {
            FSMGR_LOGE("Failed to parse block device.");
            break;
        }
        item->deviceName = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            FSMGR_LOGE("Failed to parse mount point.");
            break;
        }
        item->mountPoint = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            FSMGR_LOGE("Failed to parse fs type.");
            break;
        }
        item->fsType = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            FSMGR_LOGE("Failed to parse mount options.");
            break;
        }
        item->mountOptions = strdup(p);

        if ((p = strtok_r(NULL, separator, &rest)) == NULL) {
            FSMGR_LOGE("Failed to parse fs manager flags.");
            break;
        }
        // @fsManagerFlags only for fstab
        // Ignore it if we read from /proc/mounts
        if (!procMounts) {
            item->fsManagerFlags = ConvertFlags(p);
        } else {
            item->fsManagerFlags = 0;
        }
        AddToFstab(fstab, item);
        return 0;
    } while (0);

    free(item);
    item = NULL;
    return -1;
}

Fstab *ReadFstabFromFile(const char *file, bool procMounts)
{
    char *line = NULL;
    size_t allocn = 0;
    ssize_t readn = 0;
    Fstab *fstab = NULL;

    if (file == NULL) {
        FSMGR_LOGE("Invalid file");
        return NULL;
    }

    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        FSMGR_LOGE("Open %s failed, err = %d", file, errno);
        return NULL;
    }

    if ((fstab = (Fstab *)calloc(1, sizeof(Fstab))) == NULL) {
        FSMGR_LOGE("Allocate memory for FS table failed, err = %d", errno);
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

        if (ParseFstabPerLine(p, fstab, procMounts) < 0) {
            if (errno == ENOMEM) {
                // Ran out of memory, there is no reason to continue.
                break;
            }
            // If one line in fstab file parsed with a failure. just give a warning
            // and skip it.
            FSMGR_LOGW("Cannot parse file \" %s \" at line %zu. skip it", file, ln);
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
            if (strcmp(item->mountPoint, mp) == 0) {
                break;
            }
        }
    }
    return item;
}

FstabItem *FindFstabItemForPath(Fstab fstab, const char *path)
{
    FstabItem *item = NULL;

    if (path == NULL || *path == '\0') {
        return NULL;
    }

    char tmp[PATH_MAX] = {0};
    char *dir = NULL;
    if (strncpy_s(tmp, PATH_MAX -1,  path, strlen(path)) != EOK) {
        FSMGR_LOGE("Failed to copy path.");
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

static const struct MountFlags mountFlags[] = {
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
        for (size_t i = 0; i < ARRAY_LENGTH(mountFlags); i++) {
            if (strcmp(str, mountFlags[i].name) == 0) {
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
        for (size_t i = 0; i < ARRAY_LENGTH(mountFlags); i++) {
            if (strcmp(str, mountFlags[i].name) == 0) {
                flags = mountFlags[i].flags;
                break;
            }
        }
    }
    return flags;
}

unsigned long GetMountFlags(char *mountFlag, char *fsSpecificData, size_t fsSpecificDataSize)
{
    unsigned long flags = 0;
    INIT_CHECK_RETURN_VALUE(mountFlag != NULL && fsSpecificData != NULL, 0);
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
            if (strncat_s(fsSpecificData, fsSpecificDataSize - 1, p, strlen(p)) != EOK) {
                FSMGR_LOGW("Failed to append mount flag \" %s \", ignore it.", p);
                continue;
            }
            if (i == flagCount - 1) { // last flags, do not need to append ','
                break;
            }
            // Combined each mount flag with ','
            if (strncat_s(fsSpecificData, fsSpecificDataSize - 1, ",", 1) != EOK) {
                FSMGR_LOGW("Failed to append comma.");
                break; // If cannot add ',' to the end of flags, there is not reason to continue.
            }
        }
    }

    FreeStringVector(flagsVector, flagCount);
    return flags;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif