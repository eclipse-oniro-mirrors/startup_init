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

#include "switch_root.h"
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "init_log.h"
#include "fs_manager/fs_manager.h"
#include "securec.h"
#include "init_utils.h"

static void FreeOldRoot(DIR *dir, dev_t dev)
{
    if (dir == NULL) {
        return;
    }
    int dfd = dirfd(dir);
    bool isDir = false;
    struct dirent *de = NULL;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        isDir = false;
        if (de->d_type == DT_DIR || de->d_type == DT_UNKNOWN) {
            struct stat st = {};
            if (fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
                INIT_LOGE("Failed to get stat of %s", de->d_name);
                continue;
            }

            if (st.st_dev != dev) {
                continue; // Not the same device, ignore.
            }
            if (!S_ISDIR(st.st_mode)) {
                continue;
            }
            int fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
            isDir = true;
            if (fd < 0) {
                continue;
            }
            DIR *subDir = fdopendir(fd);
            if (subDir != NULL) {
                FreeOldRoot(subDir, dev);
                closedir(subDir);
            } else {
                close(fd);
            }
        }
        if (unlinkat(dfd, de->d_name, isDir ? AT_REMOVEDIR : 0) < 0) {
            INIT_LOGE("Failed to unlink %s, err = %d", de->d_name, errno);
        }
    }
}

// For sub mountpoint under /dev, /sys, /proc
// There is no need to move them individually
// We will find a better solution to take care of
// all sub mount tree in the future.
static bool UnderBasicMountPoint(const char *path)
{
    unsigned int i;
    if (path == NULL || *path == '\0') {
        return false;
    }
    const char *basicMountPoint[] = {"/dev/", "/sys/", "/proc/"};
    for (i = 0; i < ARRAY_LENGTH(basicMountPoint); i++) {
        if (strncmp(path, basicMountPoint[i], strlen(basicMountPoint[i])) == 0) {
            return true;
        }
    }
    return false;
}

static int MountToNewTarget(const char *target)
{
    if (target == NULL || *target == '\0') {
        return -1;
    }
    Fstab *fstab = ReadFstabFromFile("/proc/mounts", true);
    if (fstab == NULL) {
        INIT_LOGE("Fatal error. Read mounts info from \" /proc/mounts \" failed");
        return -1;
    }

    for (FstabItem *item = fstab->head; item != NULL; item = item->next) {
        const char *mountPoint = item->mountPoint;
        if (mountPoint == NULL || strcmp(mountPoint, "/") == 0 ||
            strcmp(mountPoint, target) == 0) {
            continue;
        }
        char newMountPoint[PATH_MAX] = {0};
        if (snprintf_s(newMountPoint, PATH_MAX, PATH_MAX - 1, "%s%s", target, mountPoint) == -1) {
            INIT_LOGW("Cannot build new mount point for old mount point \" %s \"", mountPoint);
            // Just ignore this one or return error?
            continue;
        }
        INIT_LOGV("new mount point is: %s", newMountPoint);
        if (!UnderBasicMountPoint(mountPoint)) {
            INIT_LOGV("Move mount %s to %s", mountPoint, newMountPoint);
            if (mount(mountPoint, newMountPoint, NULL, MS_MOVE, NULL) < 0) {
                INIT_LOGE("Failed to mount moving %s to %s, err = %d", mountPoint, newMountPoint, errno);
                // If one mount entry cannot move to new mountpoint, umount it.
                umount2(mountPoint, MNT_FORCE);
                continue;
            }
        }
    }
    ReleaseFstab(fstab);
    fstab = NULL;
    return 0;
}

static void FreeRootDir(DIR *oldRoot, dev_t dev)
{
    if (oldRoot != NULL) {
        FreeOldRoot(oldRoot, dev);
        closedir(oldRoot);
    }
}

// Switch root from ramdisk to system
int SwitchRoot(const char *newRoot)
{
    if (newRoot == NULL || *newRoot == '\0') {
        errno = EINVAL;
        INIT_LOGE("Fatal error. Try to switch to new root with invalid new mount point");
        return -1;
    }

    struct stat oldRootStat = {};
    INIT_ERROR_CHECK(stat("/", &oldRootStat) == 0, return -1, "Failed to get old root \"/\" stat");
    DIR *oldRoot = opendir("/");
    INIT_ERROR_CHECK(oldRoot != NULL, return -1, "Failed to open root dir \"/\"");
    struct stat newRootStat = {};
    if (stat(newRoot, &newRootStat) != 0) {
        INIT_LOGE("Failed to get new root \" %s \" stat", newRoot);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    if (oldRootStat.st_dev == newRootStat.st_dev) {
        INIT_LOGW("Try to switch root in same device, skip switching root");
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return 0;
    }
    if (MountToNewTarget(newRoot) < 0) {
        INIT_LOGE("Failed to move mount to new root \" %s \" stat", newRoot);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }
    // OK, we've done move mount.
    // Now mount new root.
    if (chdir(newRoot) < 0) {
        INIT_LOGE("Failed to change directory to %s, err = %d", newRoot, errno);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    if (mount(newRoot, "/", NULL, MS_MOVE, NULL) < 0) {
        INIT_LOGE("Failed to mount moving %s to %s, err = %d", newRoot, "/", errno);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    if (chroot(".") < 0) {
        INIT_LOGE("Failed to change root directory");
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }
    FreeRootDir(oldRoot, oldRootStat.st_dev);
    INIT_LOGI("SwitchRoot to %s finish", newRoot);
    return 0;
}
