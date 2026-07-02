/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "securec.h"
#include "init_log.h"
#include "init_utils.h"
#include "fs_manager/fs_manager.h"
#include "fs_dm.h"
#include "fs_dm_linear.h"
#include "dm_merge_overlay.h"
#include "erofs_overlay_common.h"
#include "erofs_remount_overlay.h"
#include "remount_overlay.h"

#define REMOUNT_RESULT_FLAG STARTUP_INIT_UT_PATH"/data/service/el1/startup/remount/remount.result.done"

static const char *GetFstabFileForRemount(char *fileName, size_t size)
{
    if (InUpdaterMode() == 1) {
        if (strncpy_s(fileName, size, STARTUP_INIT_UT_PATH"/etc/fstab.updater",
            strlen(STARTUP_INIT_UT_PATH"/etc/fstab.updater")) != 0) {
            INIT_LOGE("Failed strncpy_s err=%d", errno);
            return NULL;
        }
    } else {
        char hardware[MAX_BUFFER_LEN] = {0};
        int ret = GetParameterFromCmdLine("hardware", hardware, MAX_BUFFER_LEN);
        if (ret != 0) {
            INIT_LOGE("Failed get hardware from cmdline");
            return NULL;
        }
        if (snprintf_s(fileName, size, size - 1, STARTUP_INIT_UT_PATH"/vendor/etc/fstab.%s",
            hardware) == -1) {
            INIT_LOGE("Failed to build fstab file, err=%d", errno);
            return NULL;
        }
    }
    INIT_LOGI("fstab file is %s", fileName);
    return fileName;
}

static Fstab *LoadFstabForRemount(void)
{
    Fstab *fstab = LoadFstabFromCommandLine();
    if (fstab == NULL) {
        INIT_LOGI("Cannot load fstab from command line, try read from fstab file");
        char fstabFile[MAX_BUFFER_LEN] = {0};
        if (GetFstabFileForRemount(fstabFile, MAX_BUFFER_LEN) == NULL) {
            INIT_LOGE("Failed to get fstab file path");
            return NULL;
        }
        if (access(fstabFile, F_OK) != 0) {
            INIT_LOGE("fstab file %s not found", fstabFile);
            return NULL;
        }
        fstab = ReadFstabFromFile(fstabFile, false);
    }
    if (fstab != NULL && GetBootSlots() > 1) {
        FstabItem *item = fstab->head;
        while (item != NULL) {
            FsAdjustPartitionNameBySlot(item);
            item = item->next;
        }
    }
    return fstab;
}

static int CreateDmMergeDeviceForRemount(DmLinearTarget *targets, uint32_t targetNum,
    char *dmDevPath)
{
    int rc = FsDmCreateMultiTargetLinearDevice(DM_MERGE_NAME, dmDevPath, MAX_BUFFER_LEN,
        targets, targetNum);
    if (rc != 0) {
        INIT_LOGE("create dm_merge device failed");
        return -1;
    }
    rc = FsDmInitDmDev(dmDevPath, true);
    if (rc != 0) {
        INIT_LOGE("init dm_merge dm dev failed, path [%s]", dmDevPath);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    WaitForFile(dmDevPath, WAIT_MAX_SECOND);
    if (access(dmDevPath, F_OK) != 0) {
        INIT_LOGE("dm_merge device [%s] not ready after wait", dmDevPath);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    return 0;
}

static int FormatAndMountDmMerge(const char *dmDevPath)
{
    int rc = FormatExt4(dmDevPath, "/");
    if (rc != 0) {
        INIT_LOGE("format dm_merge as ext4 failed");
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    if (mkdir(PREFIX_OVERLAY_MERGE, MODE_MKDIR) && (errno != EEXIST)) {
        INIT_LOGE("mkdir %s failed", PREFIX_OVERLAY_MERGE);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    if (mount(dmDevPath, PREFIX_OVERLAY_MERGE, "ext4", MS_NOATIME | MS_NODEV, NULL)) {
        INIT_LOGE("mount dm_merge on %s failed", PREFIX_OVERLAY_MERGE);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    return 0;
}

static int MkdirOverlayMergeDirs(char mntPaths[][MAX_BUFFER_LEN], uint32_t targetNum)
{
    for (uint32_t i = 0; i < targetNum; i++) {
        char dirPartition[MAX_BUFFER_LEN] = {0};
        char dirUpper[MAX_BUFFER_LEN] = {0};
        char dirWork[MAX_BUFFER_LEN] = {0};

        if (snprintf_s(dirPartition, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
            "%s%s", PREFIX_OVERLAY_MERGE, mntPaths[i]) < 0) {
            INIT_LOGE("snprintf dirPartition failed");
            return -1;
        }
        if (mkdir(dirPartition, MODE_MKDIR) && (errno != EEXIST)) {
            INIT_LOGE("mkdir %s failed", dirPartition);
            return -1;
        }
        if (snprintf_s(dirUpper, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
            "%s%s%s", PREFIX_OVERLAY_MERGE, mntPaths[i], PREFIX_UPPER) < 0) {
            INIT_LOGE("snprintf dirUpper failed");
            return -1;
        }
        if (mkdir(dirUpper, MODE_MKDIR) && (errno != EEXIST)) {
            INIT_LOGE("mkdir %s failed", dirUpper);
            return -1;
        }
        if (snprintf_s(dirWork, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
            "%s%s%s", PREFIX_OVERLAY_MERGE, mntPaths[i], PREFIX_WORK) < 0) {
            INIT_LOGE("snprintf dirWork failed");
            return -1;
        }
        if (mkdir(dirWork, MODE_MKDIR) && (errno != EEXIST)) {
            INIT_LOGE("mkdir %s failed", dirWork);
            return -1;
        }
    }
    return 0;
}

static void RollbackOverlayMounts(char mntPaths[][MAX_BUFFER_LEN], uint32_t targetNum)
{
    for (uint32_t j = 0; j < targetNum; j++) {
        char overlayMount[MAX_BUFFER_LEN];
        if (snprintf_s(overlayMount, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
            "%s%s", PREFIX_OVERLAY_MERGE, mntPaths[j]) < 0) {
            INIT_LOGE("snprintf_s overlayMount failed for %s", mntPaths[j]);
            continue;
        }
        umount2(overlayMount, MNT_DETACH);
    }
    umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
    FsDmRemoveDevice(DM_MERGE_NAME);
}

static int MountAllOverlayForRemount(char mntPaths[][MAX_BUFFER_LEN], uint32_t targetNum)
{
    for (uint32_t i = 0; i < targetNum; i++) {
        if (strcmp(mntPaths[i], "/vendor") == 0) {
            OverlayRemountVendorPre();
        }
        if (MountOverlayOne(mntPaths[i], PREFIX_OVERLAY_MERGE)) {
            INIT_LOGE("mount overlay failed on mnt [%s]", mntPaths[i]);
            RollbackOverlayMounts(mntPaths, targetNum);
            return -1;
        }
        if (strcmp(mntPaths[i], "/vendor") == 0) {
            OverlayRemountVendorPost();
        }
    }
    return 0;
}

static void CreateDmMergeMarker(void)
{
    int markerFd = open(PREFIX_OVERLAY_MERGE"/.dm_merge", O_CREAT | O_WRONLY, 0644);
    if (markerFd >= 0) {
        close(markerFd);
        INIT_LOGI("dm_merge marker created");
    }
}

static int SetupDmMergeOverlay(const char *dmDevPath, char mntPaths[][MAX_BUFFER_LEN], uint32_t targetNum)
{
    if (FormatAndMountDmMerge(dmDevPath) != 0) {
        return -1;
    }
    if (MkdirOverlayMergeDirs(mntPaths, targetNum) != 0) {
        umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    if (MountAllOverlayForRemount(mntPaths, targetNum) != 0) {
        return -1;
    }
    return 0;
}

int TryDmMergeRemount(void)
{
    Fstab *fstab = LoadFstabForRemount();
    if (fstab == NULL) {
        INIT_LOGE("load fstab for remount failed");
        return REMOUNT_FAIL;
    }

    DmLinearTarget targets[MAX_TARGET_NUM];
    char mntPaths[MAX_TARGET_NUM][MAX_BUFFER_LEN];
    DmMergeCollectCtx ctx = {
        .targets = targets,
        .mntPaths = mntPaths,
        .num = 0,
        .sectorOffset = 0,
        .seenCount = 0,
    };

    int rc = CollectDmMergeTargets(fstab, &ctx);
    if (rc != 0 || ctx.num == 0) {
        INIT_LOGE("collect dm_merge targets failed or empty");
        ReleaseFstab(fstab);
        return REMOUNT_FAIL;
    }

    char dmDevPath[MAX_BUFFER_LEN] = {0};
    rc = CreateDmMergeDeviceForRemount(targets, ctx.num, dmDevPath);
    if (rc != 0) {
        DestroyDmMergeTargets(targets, ctx.num);
        ReleaseFstab(fstab);
        return REMOUNT_FAIL;
    }

    DestroyDmMergeTargets(targets, ctx.num);
    ReleaseFstab(fstab);

    if (SetupDmMergeOverlay(dmDevPath, mntPaths, ctx.num) != 0) {
        return REMOUNT_FAIL;
    }

    SetRemountResultFlag();
    CreateDmMergeMarker();
    EngFilesOverlay("/eng_system", "/");
    EngFilesOverlay("/eng_chipset", "/chipset");
    INIT_LOGI("dm_merge remount success");
    return REMOUNT_SUCC;
}

int ClearDmMerge(void)
{
    if (!IsDmMergeOverlayActive()) {
        INIT_LOGI("legacy remount does not support remount -c");
        printf("legacy remount does not support remount -c, skip\n");
        return 1;
    }
    if (GetRemountResult() != REMOUNT_SUCC) {
        INIT_LOGI("remount not executed, remount -c skip");
        printf("remount not executed, skip\n");
        return 1;
    }
    int fd = open(PREFIX_OVERLAY_MERGE"/.dm_merge_cleanup", O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        INIT_LOGE("Failed to create .dm_merge_cleanup marker, errno %d", errno);
        return -1;
    }
    write(fd, "1", 1);
    close(fd);
    unlink(REMOUNT_RESULT_FLAG);
    INIT_LOGI(".dm_merge_cleanup marker created, will reboot");
    sync();
    reboot(RB_AUTOBOOT);
    return 0;
}
