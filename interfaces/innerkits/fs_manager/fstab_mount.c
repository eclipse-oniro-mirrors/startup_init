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
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>
#include "beget_ext.h"
#include "fs_manager/fs_manager.h"
#include "init_utils.h"
#include "param/init_param.h"
#include "securec.h"
#ifdef SUPPORT_HVB
#include "dm_verity.h"
#endif
#include "init_filesystems.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define FS_MANAGER_BUFFER_SIZE 512
#define BLOCK_SIZE_BUFFER (64)
#define RESIZE_BUFFER_SIZE 1024
const off_t PARTITION_ACTIVE_SLOT_OFFSET = 1024;
const off_t PARTITION_ACTIVE_SLOT_SIZE = 4;

__attribute__((weak)) void InitPostMount(const char *mountPoint, int rc, const char *fsType)
{
}

__attribute__((weak)) void InitTimerControl(bool isSuspend)
{
}

static const SUPPORTED_FILE_SYSTEM supportedFileSystems[] = {
    { "ext4", 0 },
    { "f2fs", 1 },
    { "overlay", 0 },
    { NULL, 0 }
};

static void **extendedFileSystems_ = NULL;

void InitSetExtendedFileSystems(const SUPPORTED_FILE_SYSTEM *extendedFileSystems[])
{
    extendedFileSystems_ = (void **)extendedFileSystems;
}

static const SUPPORTED_FILE_SYSTEM *GetSupportedFileSystemInfo(const char *fsType)
{
    return (const SUPPORTED_FILE_SYSTEM *)OH_ExtendableStrDictGet((void **)supportedFileSystems,
           sizeof(SUPPORTED_FILE_SYSTEM), fsType, 0, extendedFileSystems_);
}

static bool IsSupportedDataType(const char *fsType)
{
    const SUPPORTED_FILE_SYSTEM *item = GetSupportedFileSystemInfo(fsType);
    if (item == NULL) {
        return false;
    }
    if (item->for_userdata) {
        return true;
    }
    return false;
}

bool IsSupportedFilesystem(const char *fsType)
{
    const SUPPORTED_FILE_SYSTEM *item = GetSupportedFileSystemInfo(fsType);
    if (item == NULL) {
        return false;
    }
    return true;
}

static int ExecCommand(int argc, char **argv)
{
    BEGET_CHECK(!(argc == 0 || argv == NULL || argv[0] == NULL), return -1);

    BEGET_LOGI("Execute %s begin", argv[0]);
    pid_t pid = fork();
    BEGET_ERROR_CHECK(pid >= 0, return -1, "Fork new process to format failed: %d", errno);

    if (pid == 0) {
        execv(argv[0], argv);
        exit(-1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        BEGET_LOGE("Command %s failed with status %d", argv[0], WEXITSTATUS(status));
    }
    BEGET_LOGI("Execute %s end", argv[0]);
    return WEXITSTATUS(status);
}

int DoFormat(const char *devPath, const char *fsType)
{
    if (devPath == NULL || fsType == NULL) {
        return -1;
    }

    if (!IsSupportedFilesystem(fsType)) {
        BEGET_LOGE("Do not support filesystem \" %s \"", fsType);
        return -1;
    }
    int ret = 0;
    if (strcmp(fsType, "ext4") == 0) {
        char blockSizeBuffer[BLOCK_SIZE_BUFFER] = {0};
        const unsigned int blockSize = 4096;
        ret = snprintf_s(blockSizeBuffer, BLOCK_SIZE_BUFFER, BLOCK_SIZE_BUFFER - 1, "%u", blockSize);
        BEGET_ERROR_CHECK(ret != -1, return -1, "Failed to build block size buffer");

        char *formatCmds[] = {
            "/bin/mke2fs", "-F", "-t", (char *)fsType, "-b", blockSizeBuffer, (char *)devPath, NULL
        };
        int argc = ARRAY_LENGTH(formatCmds);
        char **argv = (char **)formatCmds;
        ret = ExecCommand(argc, argv);
    } else if (IsSupportedDataType(fsType)) {
#ifdef __MUSL__
        char *formatCmds[] = {
            "/bin/mkfs.f2fs", "-d1", "-O", "encrypt", "-O", "quota", "-O", "verity", "-O", "project_quota,extra_attr",
            "-O", "sb_checksum", (char *)devPath, NULL
        };
#else
        char *formatCmds[] = {
            "/bin/make_f2fs", "-d1", "-O", "encrypt", "-O", "quota", "-O", "verity",  "-O", "project_quota,extra_attr",
            "-O", "sb_checksum", (char *)devPath, NULL
        };
#endif
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
    BEGET_CHECK(fp != NULL, return status);

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
    if (found) {
        status = MOUNT_MOUNTED;
    } else if (feof(fp) > 0) {
        status = MOUNT_UMOUNTED;
    }
    (void)fclose(fp);
    fp = NULL;
    return status;
}

#define MAX_RESIZE_PARAM_NUM 20
static int DoResizeF2fs(const char* device, const unsigned long long size, const unsigned int fsManagerFlags)
{
    char *file = "/system/bin/resize.f2fs";
    char sizeStr[RESIZE_BUFFER_SIZE] = {0};
    char *argv[MAX_RESIZE_PARAM_NUM] = {NULL};
    int argc = 0;

    BEGET_ERROR_CHECK(access(file, F_OK) == 0, return -1, "resize.f2fs is not exists.");

    argv[argc++] = file;
    if (fsManagerFlags & FS_MANAGER_PROJQUOTA) {
        argv[argc++] = "-O";
        argv[argc++] = "extra_attr,project_quota";
    }
    if (fsManagerFlags & FS_MANAGER_CASEFOLD) {
        argv[argc++] = "-O";
        argv[argc++] = "casefold";
        argv[argc++] = "-C";
        argv[argc++] = "utf8";
    }
    if (fsManagerFlags & FS_MANAGER_COMPRESSION) {
        argv[argc++] = "-O";
        argv[argc++] = "extra_attr,compression";
    }
    if (fsManagerFlags & FS_MANAGER_DEDUP) {
        argv[argc++] = "-O";
        argv[argc++] = "extra_attr,dedup";
    }

    if (size != 0) {
        unsigned long long realSize = size *
            ((unsigned long long)RESIZE_BUFFER_SIZE * RESIZE_BUFFER_SIZE / FS_MANAGER_BUFFER_SIZE);
        int len = sprintf_s(sizeStr, RESIZE_BUFFER_SIZE, "%llu", realSize);
        if (len <= 0) {
            BEGET_LOGE("Write buffer size failed.");
        }
        argv[argc++] = "-t";
        argv[argc++] = sizeStr;
    }

    argv[argc++] = (char*)device;
    BEGET_ERROR_CHECK(argc <= MAX_RESIZE_PARAM_NUM, return -1, "argc: %d is too big.", argc);
    return ExecCommand(argc, argv);
}

static int DoFsckF2fs(const char* device)
{
    char *file = "/system/bin/fsck.f2fs";
    BEGET_ERROR_CHECK(access(file, F_OK) == 0, return -1, "fsck.f2fs is not exists.");

    char *cmd[] = {
        file, "-p1", (char *)device, NULL
    };
    int argc = ARRAY_LENGTH(cmd);
    char **argv = (char **)cmd;
    InitTimerControl(true);
    int ret = ExecCommand(argc, argv);
    InitTimerControl(false);
    return ret;
}

static int DoResizeExt(const char* device, const unsigned long long size)
{
    char *file = "/system/bin/resize2fs";
    BEGET_ERROR_CHECK(access(file, F_OK) == 0, return -1, "resize2fs is not exists.");

    int ret = 0;
    if (size == 0) {
        char *cmd[] = {
            file, "-f", (char *)device, NULL
        };
        int argc = ARRAY_LENGTH(cmd);
        char **argv = (char **)cmd;
        ret = ExecCommand(argc, argv);
    } else {
        char sizeStr[RESIZE_BUFFER_SIZE] = {0};
        int len = sprintf_s(sizeStr, RESIZE_BUFFER_SIZE, "%lluM", size);
        if (len <= 0) {
            BEGET_LOGE("Write buffer size failed.");
        }
        char *cmd[] = {
            file, "-f", (char *)device, sizeStr, NULL
        };
        int argc = ARRAY_LENGTH(cmd);
        char **argv = (char **)cmd;
        ret = ExecCommand(argc, argv);
    }
    return ret;
}

static int DoFsckExt(const char* device)
{
    char *file = "/system/bin/e2fsck";
    BEGET_ERROR_CHECK(access(file, F_OK) == 0, return -1, "e2fsck is not exists.");

    char *cmd[] = {
        file, "-y", (char *)device, NULL
    };
    int argc = ARRAY_LENGTH(cmd);
    char **argv = (char **)cmd;
    return ExecCommand(argc, argv);
}

static int Mount(const char *source, const char *target, const char *fsType,
    unsigned long flags, const char *data)
{
    struct stat st = {};
    int rc = -1;

    bool isTrue = source == NULL || target == NULL || fsType == NULL;
    BEGET_ERROR_CHECK(!isTrue, return -1, "Invalid argument for mount.");

    isTrue = stat(target, &st) != 0 && errno != ENOENT;
    BEGET_ERROR_CHECK(!isTrue, return -1, "Cannot get stat of \" %s \", err = %d", target, errno);

    BEGET_CHECK((st.st_mode & S_IFMT) != S_IFLNK, unlink(target)); // link, delete it.

    if (mkdir(target, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
        BEGET_ERROR_CHECK(errno == EEXIST, return -1, "Failed to create dir \" %s \", err = %d", target, errno);
    }
    errno = 0;
    if ((rc = mount(source, target, fsType, flags, data)) != 0) {
        BEGET_WARNING_CHECK(errno != EBUSY, rc = 0, "Mount %s to %s busy, ignore", source, target);
    }
    return rc;
}

static int GetSlotInfoFromCmdLine(const char *slotInfoName)
{
    char value[MAX_BUFFER_LEN] = {0};
    BEGET_INFO_CHECK(GetParameterFromCmdLine(slotInfoName, value, MAX_BUFFER_LEN) == 0,
        return -1, "Failed to get %s value from cmdline", slotInfoName);
    return atoi(value);
}

static int GetSlotInfoFromBootctrl(off_t offset, off_t size)
{
    char bootctrlDev[MAX_BUFFER_LEN] = {0};
    BEGET_ERROR_CHECK(GetBlockDevicePath("/bootctrl", bootctrlDev, MAX_BUFFER_LEN) == 0,
        return -1, "Failed to get bootctrl device");
    char *realPath = GetRealPath(bootctrlDev);
    BEGET_ERROR_CHECK(realPath != NULL, return -1, "Failed to get bootctrl device real path");
    int fd = open(realPath, O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    free(realPath);
    BEGET_ERROR_CHECK(fd >= 0, return -1, "Failed to open bootctrl device, errno %d", errno);
    BEGET_ERROR_CHECK(lseek(fd, offset, SEEK_SET) >= 0, close(fd); return -1,
        "Failed to lseek bootctrl device fd, errno %d", errno);
    int slotInfo = 0;
    BEGET_INFO_CHECK(read(fd, &slotInfo, sizeof(slotInfo)) == size, close(fd); return -1,
        "Failed to read current slot from bootctrl, errno %d", errno);
    close(fd);
    return slotInfo;
}

int GetBootSlots(void)
{
    return GetSlotInfoFromCmdLine("bootslots");
}

int GetCurrentSlot(void)
{
    // get current slot from cmdline
    int currentSlot = GetSlotInfoFromCmdLine("currentslot");
    BEGET_CHECK_RETURN_VALUE(currentSlot <= 0, currentSlot);
    BEGET_LOGI("No valid slot value found from cmdline, try to get it from bootctrl");

    // get current slot from bootctrl
    return GetSlotInfoFromBootctrl(PARTITION_ACTIVE_SLOT_OFFSET, PARTITION_ACTIVE_SLOT_SIZE);
}

int MountOneItem(FstabItem *item)
{
    if (item == NULL) {
        return -1;
    }
    unsigned long mountFlags;
    char fsSpecificData[FS_MANAGER_BUFFER_SIZE] = {0};

    mountFlags = GetMountFlags(item->mountOptions, fsSpecificData, sizeof(fsSpecificData),
        item->mountPoint);
    if (!IsSupportedFilesystem(item->fsType)) {
        BEGET_LOGW("Unsupported file system \" %s \"", item->fsType);
        return 0;
    }
    if (FM_MANAGER_WAIT_ENABLED(item->fsManagerFlags)) {
        WaitForFile(item->deviceName, WAIT_MAX_SECOND);
    }

    if (strcmp(item->mountPoint, "/data") == 0 && IsSupportedDataType(item->fsType)) {
        int ret = DoResizeF2fs(item->deviceName, 0, item->fsManagerFlags);
        if (ret != 0) {
            BEGET_LOGE("Failed to resize.f2fs dir %s , ret = %d", item->deviceName, ret);
        }

        ret = DoFsckF2fs(item->deviceName);
        if (ret != 0) {
            BEGET_LOGE("Failed to fsck.f2fs dir %s , ret = %d", item->deviceName, ret);
        }
    } else if (strcmp(item->fsType, "ext4") == 0 && strcmp(item->mountPoint, "/data") == 0) {
        int ret = DoResizeExt(item->deviceName, 0);
        if (ret != 0) {
            BEGET_LOGE("Failed to resize2fs dir %s , ret = %d", item->deviceName, ret);
        }
        ret = DoFsckExt(item->deviceName);
        if (ret != 0) {
            BEGET_LOGE("Failed to e2fsck dir %s , ret = %d", item->deviceName, ret);
        }
    }

    int retryCount = 3;
    int rc = 0;
    while (retryCount-- > 0) {
        rc = Mount(item->deviceName, item->mountPoint, item->fsType, mountFlags, fsSpecificData);
        if (rc == 0) {
            break;
        }
        BEGET_LOGE("Mount device %s to %s failed, err = %d, retry", item->deviceName, item->mountPoint, errno);
    }
    InitPostMount(item->mountPoint, rc, item->fsType);
    if (rc != 0) {
        if (FM_MANAGER_NOFAIL_ENABLED(item->fsManagerFlags)) {
            BEGET_LOGE("Mount no fail device %s to %s failed, err = %d", item->deviceName, item->mountPoint, errno);
        } else {
            BEGET_LOGW("Mount %s to %s failed, err = %d. Ignore failure", item->deviceName, item->mountPoint, errno);
            rc = 0;
        }
    } else {
        BEGET_LOGI("Mount %s to %s successful", item->deviceName, item->mountPoint);
    }
    return rc;
}

static void AdjustPartitionNameByPartitionSlot(FstabItem *item)
{
    BEGET_CHECK_ONLY_RETURN(strstr(item->deviceName, "/system") != NULL ||
        strstr(item->deviceName, "/vendor") != NULL);
    char buffer[MAX_BUFFER_LEN] = {0};
    int slot = GetCurrentSlot();
    BEGET_ERROR_CHECK(slot > 0 && slot <= MAX_SLOT, slot = 1, "slot value %d is invalid, set default value", slot);
    BEGET_ERROR_CHECK(sprintf_s(buffer, sizeof(buffer), "%s_%c", item->deviceName, 'a' + slot - 1) > 0,
        return, "Failed to format partition name suffix, use default partition name");
    free(item->deviceName);
    item->deviceName = strdup(buffer);
    if (item->deviceName == NULL) {
        BEGET_LOGE("failed dup devicename");
        return;
    }
    BEGET_LOGI("partition name with slot suffix: %s", item->deviceName);
}

static int CheckRequiredAndMount(FstabItem *item, bool required)
{
    int rc = 0;
    if (item == NULL) {
        return -1;
    }
    if (required) { // Mount partition during first startup.
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            int bootSlots = GetBootSlots();
            BEGET_INFO_CHECK(bootSlots <= 1, AdjustPartitionNameByPartitionSlot(item),
                "boot slots is %d, now adjust partition name according to current slot", bootSlots);
        #ifdef SUPPORT_HVB
            rc = HvbDmVeritySetUp(item);
            if (rc != 0) {
                BEGET_LOGE("set dm_verity err, ret = 0x%x", rc);
                return rc;
            }
        #endif
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
    BEGET_CHECK(fstab != NULL, return -1);

    FstabItem *item = NULL;
    int rc = -1;

#ifdef SUPPORT_HVB
    if (required) {
        rc = HvbDmVerityinit(fstab);
        if (rc != 0) {
            BEGET_LOGE("set dm_verity init, ret = 0x%x", rc);
            return rc;
        }
    }
#endif

    for (item = fstab->head; item != NULL; item = item->next) {
        rc = CheckRequiredAndMount(item, required);
        if (required && (rc < 0)) { // Init fail to mount in the first stage and exit directly.
            break;
        }
    }

#ifdef SUPPORT_HVB
    if (required)
        HvbDmVerityFinal();
#endif

    return rc;
}

int MountAllWithFstabFile(const char *fstabFile, bool required)
{
    bool isFile = fstabFile == NULL || *fstabFile == '\0';
    BEGET_CHECK(!isFile, return -1);

    Fstab *fstab = NULL;
    if ((fstab = ReadFstabFromFile(fstabFile, false)) == NULL) {
        BEGET_LOGE("[fs_manager][error] Read fstab file \" %s \" failed\n", fstabFile);
        return -1;
    }

    int rc = MountAllWithFstab(fstab, required);
    if (rc != 0) {
        BEGET_LOGE("[startup_failed]MountAllWithFstab failed %s %d %d %d", fstabFile, FSTAB_MOUNT_FAILED, required, rc);
    }
    ReleaseFstab(fstab);
    fstab = NULL;
    return rc;
}

int UmountAllWithFstabFile(const char *fstabFile)
{
    bool isFile = fstabFile == NULL || *fstabFile == '\0';
    BEGET_CHECK(!isFile, return -1);

    Fstab *fstab = NULL;
    fstab = ReadFstabFromFile(fstabFile, false);
    BEGET_ERROR_CHECK(fstab != NULL, return -1, "Read fstab file \" %s \" failed.", fstabFile);

    FstabItem *item = NULL;
    int rc = -1;
    for (item = fstab->head; item != NULL; item = item->next) {
        BEGET_LOGI("Umount %s.", item->mountPoint);
        MountStatus status = GetMountStatusForMountPoint(item->mountPoint);
        if (status == MOUNT_ERROR) {
            BEGET_LOGW("Cannot get mount status of mount point \" %s \"", item->mountPoint);
            continue; // Cannot get mount status, just ignore it and try next one.
        } else if (status == MOUNT_UMOUNTED) {
            BEGET_LOGI("Mount point \" %s \" already unmounted. device path: %s, fs type: %s.",
                item->mountPoint, item->deviceName, item->fsType);
            continue;
        } else {
            rc = umount(item->mountPoint);
            if (rc == -1) {
                BEGET_LOGE("Umount %s failed, device path: %s, fs type: %s, err = %d.",
                    item->mountPoint, item->deviceName, item->fsType, errno);
            } else {
                BEGET_LOGE("Umount %s successfully.", item->mountPoint);
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
