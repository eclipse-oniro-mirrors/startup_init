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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

#define FS_MANAGER_BUFFER_SIZE 512
#define BLOCK_SIZE_BUFFER (64)

bool IsSupportedFilesystem(const char *fsType)
{
    static const char *supportedFilesystem[] = {"ext4", "f2fs", NULL};

    bool supported = false;
    int index = 0;
    if (fsType != NULL) {
        while (supportedFilesystem[index] != NULL) {
            if (strcmp(supportedFilesystem[index++], fsType) == 0) {
                supported = true;
                break;
            }
        }
    }
    return supported;
}

static int ExecCommand(int argc, char **argv)
{
    if (argc == 0 || argv == NULL || argv[0] == NULL) {
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("Fork new process to format failed: %d", errno);
        return -1;
    }
    if (pid == 0) {
        execv(argv[0], argv);
        exit(-1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        FSMGR_LOGE("Command %s failed with status %d", argv[0], WEXITSTATUS(status));
    }
    return WEXITSTATUS(status);
}

int DoFormat(const char *devPath, const char *fsType)
{
    if (devPath == NULL || fsType == NULL) {
        return -1;
    }

    if (!IsSupportedFilesystem(fsType)) {
        FSMGR_LOGE("Do not support filesystem \" %s \"", fsType);
        return -1;
    }
    int ret = 0;
    char blockSizeBuffer[BLOCK_SIZE_BUFFER] = {0};
    if (strcmp(fsType, "ext4") == 0) {
        const unsigned int blockSize = 4096;
        if (snprintf_s(blockSizeBuffer, BLOCK_SIZE_BUFFER, BLOCK_SIZE_BUFFER - 1, "%u", blockSize) == -1) {
            FSMGR_LOGE("Failed to build block size buffer");
            return -1;
        }
        char *formatCmds[] = {
            "/bin/mke2fs", "-F", "-t", (char *)fsType, "-b", blockSizeBuffer, (char *)devPath, NULL
        };
        int argc = ARRAY_LENGTH(formatCmds);
        char **argv = (char **)formatCmds;
        ret = ExecCommand(argc, argv);
    } else if (strcmp(fsType, "f2fs") == 0) {
        char *formatCmds[] = {
            "/bin/make_f2fs", (char *)devPath, NULL
        };
        int argc = ARRAY_LENGTH(formatCmds);
        char **argv = (char **)formatCmds;
        ret = ExecCommand(argc, argv);
    }
    return ret;
}

MountStatus GetMountStatusForMountPoint(const char *mp)
{
    if (mp == NULL) {
        return MOUNT_ERROR;
    }
    char buffer[FS_MANAGER_BUFFER_SIZE] = {0};
    const int expectedItems = 6;
    int count = 0;
    char **mountItems = NULL;
    MountStatus status = MOUNT_ERROR;
    bool found = false;

    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        return status;
    }
    while (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
        size_t n = strlen(buffer);
        if (buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
        }
        mountItems = SplitStringExt(buffer, " ", &count, expectedItems);
        if (mountItems != NULL && count == expectedItems) {
            // Second item in /proc/mounts is mount point
            if (strcmp(mountItems[1], mp) == 0) {
                FreeStringVector(mountItems, count);
                found = true;
                break;
            }
            FreeStringVector(mountItems, count);
        }
    }
    if (found == true) {
        status = MOUNT_MOUNTED;
    } else if (feof(fp) > 0) {
        status = MOUNT_UMOUNTED;
    }
    (void)fclose(fp);
    fp = NULL;
    return status;
}

static int Mount(const char *source, const char *target, const char *fsType,
    unsigned long flags, const char *data)
{
    struct stat st = {};
    int rc = -1;

    if (source == NULL || target == NULL || fsType == NULL) {
        FSMGR_LOGE("Invalid argment for mount.");
        return -1;
    }
    if (stat(target, &st) != 0 && errno != ENOENT) {
        FSMGR_LOGE("Cannot get stat of \" %s \", err = %d", target, errno);
        return -1;
    }
    if ((st.st_mode & S_IFMT) == S_IFLNK) { // link, delete it.
        unlink(target);
    }
    if (mkdir(target, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
        if (errno != EEXIST) {
            FSMGR_LOGE("Failed to create dir \" %s \", err = %d", target, errno);
            return -1;
        }
    }
    errno = 0;
    while ((rc = mount(source, target, fsType, flags, data)) != 0) {
        if (errno == EAGAIN) {
            FSMGR_LOGE("Mount %s to %s failed. try again", source, target);
            continue;
        } else {
            break;
        }
    }
    return rc;
}

int MountOneItem(FstabItem *item)
{
    if (item == NULL) {
        return -1;
    }
    unsigned long mountFlags;
    char fsSpecificData[FS_MANAGER_BUFFER_SIZE] = {0};

    mountFlags = GetMountFlags(item->mountOptions, fsSpecificData, sizeof(fsSpecificData));
    if (!IsSupportedFilesystem(item->fsType)) {
        FSMGR_LOGE("Unsupported file system \" %s \"", item->fsType);
        return -1;
    }
    if (FM_MANAGER_WAIT_ENABLED(item->fsManagerFlags)) {
        WaitForFile(item->deviceName, WAIT_MAX_COUNT);
    }
    int rc = Mount(item->deviceName, item->mountPoint, item->fsType, mountFlags, fsSpecificData);
    if (rc != 0) {
        FSMGR_LOGE("Mount %s to %s failed %d", item->deviceName, item->mountPoint, errno);
    } else {
        FSMGR_LOGI("Mount %s to %s successful", item->deviceName, item->mountPoint);
    }
    return rc;
}

int CheckRequiredAndMount(FstabItem *item, bool required)
{
    int rc = 0;
    if (item == NULL) {
        return -1;
    }
    if (required) { // Mount partition during first startup.
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            rc = MountOneItem(item);
        }
    } else { // Mount partition during second startup.
        if (!FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            rc = MountOneItem(item);
        }
    }
    return rc;
}

int MountAllWithFstab(const Fstab *fstab, bool required)
{
    if (fstab == NULL) {
        return -1;
    }

    FstabItem *item = NULL;
    int rc = -1;
    for (item = fstab->head; item != NULL; item = item->next) {
        rc = CheckRequiredAndMount(item, required);
        if (required && (rc < 0)) { // Init fail to mount in the first stage and exit directly.
            break;
        }
    }
    return rc;
}

int MountAllWithFstabFile(const char *fstabFile, bool required)
{
    if (fstabFile == NULL || *fstabFile == '\0') {
        return -1;
    }
    Fstab *fstab = NULL;
    if ((fstab = ReadFstabFromFile(fstabFile, false)) == NULL) {
        FSMGR_LOGE("[fs_manager][error] Read fstab file \" %s \" failed\n", fstabFile);
        return -1;
    }

    int rc = MountAllWithFstab(fstab, required);
    ReleaseFstab(fstab);
    fstab = NULL;
    return rc;
}

int UmountAllWithFstabFile(const char *fstabFile)
{
    if (fstabFile == NULL || *fstabFile == '\0') {
        return -1;
    }
    Fstab *fstab = NULL;
    if ((fstab = ReadFstabFromFile(fstabFile, false)) == NULL) {
        FSMGR_LOGE("Read fstab file \" %s \" failed.", fstabFile);
        return -1;
    }

    FstabItem *item = NULL;
    int rc = -1;
    for (item = fstab->head; item != NULL; item = item->next) {
        FSMGR_LOGI("Umount %s.", item->mountPoint);
        MountStatus status = GetMountStatusForMountPoint(item->mountPoint);
        if (status == MOUNT_ERROR) {
            FSMGR_LOGW("Cannot get mount status of mount point \" %s \"", item->mountPoint);
            continue; // Cannot get mount status, just ignore it and try next one.
        } else if (status == MOUNT_UMOUNTED) {
            FSMGR_LOGI("Mount point \" %s \" already unmounted. device path: %s, fs type: %s.",
                item->mountPoint, item->deviceName, item->fsType);
            continue;
        } else {
            rc = umount(item->mountPoint);
            if (rc == -1) {
                FSMGR_LOGE("Umount %s failed, device path: %s, fs type: %s, err = %d.",
                    item->mountPoint, item->deviceName, item->fsType, errno);
            } else {
                FSMGR_LOGE("Umount %s successfully.", item->mountPoint);
            }
        }
    }
    ReleaseFstab(fstab);
    fstab = NULL;
    return rc;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
