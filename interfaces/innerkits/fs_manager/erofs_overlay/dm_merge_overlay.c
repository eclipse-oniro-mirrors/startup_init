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

#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include "securec.h"
#include "init_utils.h"
#include "fs_dm.h"
#include "fs_manager/fs_manager.h"

#include "dm_merge_overlay.h"
#include "erofs_remount_overlay.h"
#include "param/init_param.h"

INIT_STATIC int RemoveDirContent(const char *path);
INIT_STATIC const char *PartNameToOverlayPath(const char *partName);
INIT_STATIC int PerformDmMergeCleanup(void);

bool MntNeedRemount(const char *mnt)
{
    char *remountPath[] = {
        "/", "/usr", "/vendor", "/sys_prod", "/chip_prod", "/preload", "/cust", "/version"
    };
    for (size_t i = 0; i < ARRAY_LENGTH(remountPath); i++) {
        if (strcmp(remountPath[i], mnt) == 0) {
            return true;
        }
    }
    return false;
}

const char *OverlayPathFromMnt(const char *mnt)
{
    if (strcmp(mnt, "/") == 0 || strcmp(mnt, "/usr") == 0) {
        return "/usr";
    }
    return mnt;
}

bool IsDeviceNameSeen(const char *seenDevices[], uint32_t count, const char *deviceName)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(seenDevices[i], deviceName) == 0) {
            return true;
        }
    }
    return false;
}

uint64_t AlignDown(uint64_t value, uint64_t alignment)
{
    if (alignment == 0) {
        return value;
    }
    return (value / alignment) * alignment;
}

INIT_STATIC int CollectOneDmMergeTarget(FstabItem *item, DmMergeCollectCtx *ctx)
{
    uint64_t mapStart = 0;
    uint64_t mapLength = 0;
    if (GetMapperAddrForMerge(item->deviceName, &mapStart, &mapLength)) {
        BEGET_LOGW("get mapper addr failed for [%s], no remaining space", item->deviceName);
        return 0;
    }
    if (mapLength == 0) {
        BEGET_LOGW("no remaining space for [%s], skip", item->deviceName);
        return 0;
    }
    uint64_t alignedStart = AlignDown(mapStart, SECTOR_SIZE);
    uint64_t alignedLength = AlignDown(mapLength, SECTOR_SIZE);
    if (alignedLength == 0) {
        BEGET_LOGW("aligned length is 0 for [%s], skip", item->deviceName);
        return 0;
    }
    if (ctx->num >= MAX_TARGET_NUM) {
        BEGET_LOGE("too many targets for dm_merge");
        return -1;
    }
    ctx->targets[ctx->num].start = ctx->sectorOffset;
    ctx->targets[ctx->num].length = alignedLength / SECTOR_SIZE;
    ctx->targets[ctx->num].paras = calloc(1, MAX_BUFFER_LEN);
    if (ctx->targets[ctx->num].paras == NULL) {
        BEGET_LOGE("calloc paras failed");
        return -1;
    }
    if (snprintf_s(ctx->targets[ctx->num].paras, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
        "%s %llu", item->deviceName, alignedStart / SECTOR_SIZE) < 0) {
        BEGET_LOGE("snprintf paras failed");
        return -1;
    }
    ctx->targets[ctx->num].paras_len = strlen(ctx->targets[ctx->num].paras);
    ctx->sectorOffset += ctx->targets[ctx->num].length;
    if (snprintf_s(ctx->mntPaths[ctx->num], MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
        "%s", OverlayPathFromMnt(item->mountPoint)) < 0) {
        BEGET_LOGE("snprintf mntPath failed");
        return -1;
    }
    ctx->num++;
    ctx->seenDevices[ctx->seenCount++] = item->deviceName;
    return 0;
}

int CollectDmMergeTargets(Fstab *fstab, DmMergeCollectCtx *ctx)
{
    FstabItem *item = fstab->head;

    while (item != NULL) {
        if (!MntNeedRemount(item->mountPoint)) {
            item = item->next;
            continue;
        }
        if (IsDeviceNameSeen(ctx->seenDevices, ctx->seenCount, item->deviceName)) {
            BEGET_LOGI("duplicate device [%s], skip", item->deviceName);
            item = item->next;
            continue;
        }
        int rc = CollectOneDmMergeTarget(item, ctx);
        if (rc < 0) {
            return -1;
        }
        item = item->next;
    }
    return 0;
}

void DestroyDmMergeTargets(DmLinearTarget *targets, uint32_t targetNum)
{
    for (uint32_t i = 0; i < targetNum; i++) {
        if (targets[i].paras != NULL) {
            free(targets[i].paras);
            targets[i].paras = NULL;
        }
    }
}

INIT_STATIC int CreateDmMergeDevice(DmLinearTarget *targets, uint32_t targetNum,
    char *dmDevPath, uint64_t dmDevPathLen)
{
    int rc = FsDmCreateMultiTargetLinearDevice(DM_MERGE_NAME, dmDevPath, dmDevPathLen,
        targets, targetNum);
    if (rc != 0) {
        BEGET_LOGE("create dm_merge device failed");
        return -1;
    }
    BEGET_LOGI("create dm_merge device success, path [%s]", dmDevPath);

    rc = FsDmInitDmDev(dmDevPath, true);
    if (rc != 0) {
        BEGET_LOGE("init dm_merge dm dev failed, path [%s]", dmDevPath);
        return -1;
    }

    WaitForFile(dmDevPath, WAIT_MAX_SECOND);

    if (access(dmDevPath, F_OK) != 0) {
        BEGET_LOGE("dm_merge device [%s] not ready after wait", dmDevPath);
        return -1;
    }

    return 0;
}

INIT_STATIC int GetDmMergeDevPath(char *dmDevPath, uint64_t dmDevPathLen)
{
    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("open device mapper failed");
        return -1;
    }
    if (DmGetDeviceName(fd, DM_MERGE_NAME, dmDevPath, dmDevPathLen) != 0) {
        BEGET_LOGE("dm_merge device not found");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

INIT_STATIC int MountDmMergeExt4(char *dmDevPath)
{
    if (FsDmInitDmDev(dmDevPath, true) != 0) {
        BEGET_LOGE("init dm_merge dm dev failed");
        return -1;
    }
    if (mkdir(PREFIX_OVERLAY_MERGE, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir %s failed", PREFIX_OVERLAY_MERGE);
        return -1;
    }
    if (mount(dmDevPath, PREFIX_OVERLAY_MERGE, "ext4", MS_NOATIME | MS_NODEV, NULL)) {
        BEGET_LOGE("mount dm_merge ext4 on %s failed", PREFIX_OVERLAY_MERGE);
        return -1;
    }
    return 0;
}

INIT_STATIC int GetDmMergeDevPathAndMountExt4(char *dmDevPath, uint64_t dmDevPathLen)
{
    if (GetDmMergeDevPath(dmDevPath, dmDevPathLen) != 0) {
        return -1;
    }
    return MountDmMergeExt4(dmDevPath);
}

INIT_STATIC void BlkDiscardDmMergeDevice(void)
{
    char discardDevPath[MAX_BUFFER_LEN] = {0};
    int discardDmFd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (discardDmFd < 0) {
        return;
    }
    if (DmGetDeviceName(discardDmFd, DM_MERGE_NAME, discardDevPath, MAX_BUFFER_LEN) != 0) {
        close(discardDmFd);
        return;
    }
    close(discardDmFd);
    int discardFd = open(discardDevPath, O_RDWR);
    if (discardFd < 0) {
        return;
    }
    uint64_t devSize = 0;
    if (ioctl(discardFd, BLKGETSIZE64, &devSize) == 0) {
        uint64_t range[BLKDISCARD_RANGE_SIZE] = {0, devSize};
        ioctl(discardFd, BLKDISCARD, range);
        BEGET_LOGI("BLKDISCARD dm_merge device, size %llu", devSize);
    }
    char zeroSuper[EXT4_SUPER_OFFSET + BLOCK_SIZE_UINT] = {0};
    pwrite(discardFd, zeroSuper, sizeof(zeroSuper), 0);
    BEGET_LOGI("ext4 superblock zeroed on dm_merge device");
    close(discardFd);
}

INIT_STATIC void NormalizeDmName(char *dmName)
{
    for (uint64_t j = 0; dmName[j] != '\0'; j++) {
        if (dmName[j] == '/') {
            dmName[j] = '_';
        }
    }
}

INIT_STATIC void RemoveOneErofsDmDevice(FstabItem *item)
{
    if (!MntNeedRemount(item->mountPoint)) {
        return;
    }
    char erofsDmName[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(erofsDmName, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
        "%s_erofs", item->mountPoint) < 0) {
        BEGET_LOGE("snprintf_s erofsDmName failed for %s", item->mountPoint);
        return;
    }
    NormalizeDmName(erofsDmName);
    FsDmRemoveDevice(erofsDmName);
}

INIT_STATIC void RemoveAllErofsDmDevices(void)
{
    Fstab *fstab = LoadFstabFromCommandLine();
    if (fstab == NULL) {
        return;
    }
    FstabItem *item = fstab->head;
    while (item != NULL) {
        RemoveOneErofsDmDevice(item);
        item = item->next;
    }
    ReleaseFstab(fstab);
}

INIT_STATIC int ReadHyperholdSwitchMarker(char *buf, ssize_t *readLen)
{
    int fd = open(HYPERHOLD_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        BEGET_LOGI("open hyperhold failed, no switch marker");
        return -1;
    }
    *readLen = pread(fd, buf, HYPERHOLD_SWITCH_SIZE, HYPERHOLD_SWITCH_OFFSET);
    close(fd);
    if (*readLen <= 0 || buf[0] == '\0') {
        BEGET_LOGI("hyperhold content empty, no switch marker");
        return -1;
    }
    return 0;
}

static void WriteHyperholdEnableMarker(void);

INIT_STATIC void ZeroOnePartitionExt4Superblock(FstabItem *item)
{
    if (!MntNeedRemount(item->mountPoint)) {
        return;
    }
    uint64_t mapStart = 0;
    uint64_t mapLength = 0;
    if (GetMapperAddrForMerge(item->deviceName, &mapStart, &mapLength) != 0 || mapStart == 0) {
        return;
    }
    int fd = open(item->deviceName, O_RDWR | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("open %s for zero superblock failed", item->deviceName);
        return;
    }
    char zeroBuf[sizeof(ext4_super_block)] = {0};
    if (pwrite(fd, zeroBuf, sizeof(zeroBuf), mapStart + EXT4_SUPER_BLOCK_START_POSITION) < 0) {
        BEGET_LOGE("zero ext4 superblock failed on %s", item->deviceName);
    } else {
        BEGET_LOGI("ext4 superblock zeroed on %s at offset %llu", item->deviceName, mapStart);
    }
    close(fd);
}

INIT_STATIC void ZeroPerPartitionExt4Superblocks(Fstab *fstab)
{
    FstabItem *item = fstab->head;
    while (item != NULL) {
        ZeroOnePartitionExt4Superblock(item);
        item = item->next;
    }
    BEGET_LOGI("per-partition ext4 superblocks zeroed");
}

int CheckHyperholdDisableMarker(Fstab *fstab)
{
    char buf[HYPERHOLD_SWITCH_SIZE] = {0};
    ssize_t readLen = 0;
    if (ReadHyperholdSwitchMarker(buf, &readLen) != 0) {
        return 0;
    }
    if (strncmp(buf, HYPERHOLD_SWITCH_DISABLE, strlen(HYPERHOLD_SWITCH_DISABLE)) != 0) {
        BEGET_LOGI("hyperhold marker is not 'remount_disable', content: %s", buf);
        return 0;
    }
    BEGET_LOGI("hyperhold 'remount_disable' marker detected, starting overlay cleanup");
    TryDmMergeOverlay(fstab);
    if (IsDmMergeOverlayActive()) {
        char dmDevPath[MAX_BUFFER_LEN] = {0};
        if (GetDmMergeDevPathAndMountExt4(dmDevPath, MAX_BUFFER_LEN) == 0) {
            PerformDmMergeCleanup();
        } else {
            BlkDiscardDmMergeDevice();
            RemoveAllErofsDmDevices();
            FsDmRemoveDevice(DM_MERGE_NAME);
        }
    }
    ZeroPerPartitionExt4Superblocks(fstab);
    WriteHyperholdEnableMarker();
    BEGET_LOGI("overlay cleanup done, ext4 superblocks zeroed, 'enable' marker written");
    return 1;
}

static void WriteHyperholdEnableMarker(void)
{
    int fd = open(HYPERHOLD_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        BEGET_LOGE("open hyperhold for write enable marker failed");
        return;
    }
    char enableBuf[HYPERHOLD_SWITCH_SIZE] = {0};
    if (memcpy_s(enableBuf, HYPERHOLD_SWITCH_SIZE, HYPERHOLD_SWITCH_ENABLE,
        strlen(HYPERHOLD_SWITCH_ENABLE)) != 0) {
        BEGET_LOGE("memcpy_s enable marker failed");
        close(fd);
        return;
    }
    pwrite(fd, enableBuf, HYPERHOLD_SWITCH_SIZE, HYPERHOLD_SWITCH_OFFSET);
    close(fd);
    BEGET_LOGI("hyperhold 'enable' marker written");
}

bool IsDmMergeRemountEnabled(void)
{
    char buf[HYPERHOLD_SWITCH_SIZE] = {0};
    int fd = open(HYPERHOLD_DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        BEGET_LOGI("open hyperhold failed, dm_merge remount not enabled");
        return false;
    }
    ssize_t readLen = pread(fd, buf, HYPERHOLD_SWITCH_SIZE, HYPERHOLD_SWITCH_OFFSET);
    close(fd);
    if (readLen <= 0 || buf[0] == '\0') {
        BEGET_LOGI("hyperhold switch marker empty, use per-partition remount");
        return false;
    }
    if (strncmp(buf, HYPERHOLD_SWITCH_ENABLE, strlen(HYPERHOLD_SWITCH_ENABLE)) == 0) {
        BEGET_LOGI("hyperhold 'enable' marker found, dm_merge remount enabled");
        return true;
    }
    BEGET_LOGI("hyperhold marker is '%s', not 'enable', use per-partition remount", buf);
    return false;
}

bool IsHyperholdEnableMarkerSet(void)
{
    char buf[HYPERHOLD_SWITCH_SIZE] = {0};
    int fd = open(HYPERHOLD_DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        BEGET_LOGI("open hyperhold failed, enable marker not set");
        return false;
    }
    ssize_t readLen = pread(fd, buf, HYPERHOLD_SWITCH_SIZE, HYPERHOLD_SWITCH_OFFSET);
    close(fd);
    if (readLen <= 0 || buf[0] == '\0') {
        BEGET_LOGI("hyperhold switch marker empty");
        return false;
    }
    if (strncmp(buf, HYPERHOLD_SWITCH_ENABLE, strlen(HYPERHOLD_SWITCH_ENABLE)) == 0) {
        BEGET_LOGI("hyperhold 'enable' marker found");
        return true;
    }
    return false;
}

INIT_STATIC bool HasCleanupMarker(void)
{
    char cleanupPath[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(cleanupPath, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
        "%s/%s", PREFIX_OVERLAY_MERGE, DM_MERGE_CLEANUP_MARKER) < 0) {
        BEGET_LOGE("construct cleanup marker path failed");
        return false;
    }
    struct stat cleanupStat;
    return stat(cleanupPath, &cleanupStat) == 0;
}

INIT_STATIC int PerformDmMergeCleanup(void)
{
    BEGET_LOGI("dm_merge cleanup marker found, starting cleanup");
    sync();
    umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
    BlkDiscardDmMergeDevice();
    RemoveAllErofsDmDevices();
    FsDmRemoveDevice(DM_MERGE_NAME);
    BEGET_LOGI("dm_merge cleanup completed");
    return 1;
}

INIT_STATIC int ParseRefreshPartNames(const char *buf, const char *overlayPaths[], int *overlayCount)
{
    char refreshPartStr[HYPERHOLD_REFRESH_SIZE] = {0};
    char *semicolon = strchr(buf, ';');
    if (semicolon != NULL) {
        size_t partLen = semicolon - buf;
        if (partLen >= HYPERHOLD_REFRESH_SIZE) {
            partLen = HYPERHOLD_REFRESH_SIZE - 1;
        }
        if (memcpy_s(refreshPartStr, HYPERHOLD_REFRESH_SIZE, buf, partLen) != 0) {
            BEGET_LOGE("memcpy_s refreshPartStr (semicolon) failed");
            *overlayCount = 0;
            return -1;
        }
    } else {
        size_t bufLen = strlen(buf);
        if (bufLen >= HYPERHOLD_REFRESH_SIZE) {
            bufLen = HYPERHOLD_REFRESH_SIZE - 1;
        }
        if (memcpy_s(refreshPartStr, HYPERHOLD_REFRESH_SIZE, buf, bufLen) != 0) {
            BEGET_LOGE("memcpy_s refreshPartStr (no semicolon) failed");
            *overlayCount = 0;
            return -1;
        }
    }
    int count = 0;
    char *savePtr = NULL;
    char refreshCopy[HYPERHOLD_REFRESH_SIZE] = {0};
    if (memcpy_s(refreshCopy, HYPERHOLD_REFRESH_SIZE, refreshPartStr, HYPERHOLD_REFRESH_SIZE) != 0) {
        BEGET_LOGE("memcpy_s refreshPartStr failed");
        *overlayCount = 0;
        return -1;
    }
    char *token = strtok_r(refreshCopy, ",", &savePtr);
    while (token != NULL && count < MAX_OVERLAY_PARTITIONS) {
        const char *path = PartNameToOverlayPath(token);
        if (path != NULL) {
            overlayPaths[count++] = path;
        }
        token = strtok_r(NULL, ",", &savePtr);
    }
    *overlayCount = count;
    return 0;
}

INIT_STATIC int ReadHyperholdRefreshString(char *buf, ssize_t *readLen)
{
    int fd = open(HYPERHOLD_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        BEGET_LOGI("open hyperhold failed, no refresh needed");
        return -1;
    }
    *readLen = pread(fd, buf, HYPERHOLD_REFRESH_SIZE, 0);
    close(fd);
    if (*readLen <= 0 || buf[0] == '\0') {
        BEGET_LOGI("hyperhold refresh string empty, no refresh needed");
        return -1;
    }
    return 0;
}

INIT_STATIC int RemoveOverlayContent(const char *overlayPaths[], int overlayCount)
{
    for (int i = 0; i < overlayCount; i++) {
        char upperPath[MAX_BUFFER_LEN] = {0};
        char workPath[MAX_BUFFER_LEN] = {0};
        if (snprintf_s(upperPath, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
            "%s%s%s", PREFIX_OVERLAY_MERGE, overlayPaths[i], PREFIX_UPPER) < 0) {
            continue;
        }
        if (snprintf_s(workPath, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
            "%s%s%s", PREFIX_OVERLAY_MERGE, overlayPaths[i], PREFIX_WORK) < 0) {
            continue;
        }
        RemoveDirContent(upperPath);
        RemoveDirContent(workPath);
    }
    sync();
    umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
    BEGET_LOGI("partition refresh completed");
    return 0;
}

INIT_STATIC int MountAllDmMergeOverlayPaths(void)
{
    char *remountPaths[] = {
        "/", "/usr", "/vendor", "/sys_prod", "/chip_prod", "/preload", "/cust", "/version"
    };
    const char *seenOverlayPaths[MAX_OVERLAY_PARTITIONS] = {0};
    int seenCount = 0;
    for (size_t i = 0; i < ARRAY_LENGTH(remountPaths); i++) {
        const char *overlayPath = OverlayPathFromMnt(remountPaths[i]);
        bool seen = false;
        for (int j = 0; j < seenCount; j++) {
            if (strcmp(seenOverlayPaths[j], overlayPath) == 0) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }
        seenOverlayPaths[seenCount++] = overlayPath;
        if (strcmp(overlayPath, "/vendor") == 0) {
            OverlayRemountVendorPre();
        }
        if (MountOverlayOne(overlayPath, PREFIX_OVERLAY_MERGE) != 0) {
            BEGET_LOGE("mount dm_merge overlay failed on mnt [%s]", overlayPath);
        }
        if (strcmp(overlayPath, "/vendor") == 0) {
            OverlayRemountVendorPost();
        }
    }
    BEGET_LOGI("dm_merge overlay mount all completed");
    return 0;
}

INIT_STATIC int GetOrCreateDmMergeDevice(Fstab *fstab, char *dmDevPath, uint64_t dmDevPathLen)
{
    if (IsDmMergeOverlayActive()) {
        if (GetDmMergeDevPath(dmDevPath, dmDevPathLen) != 0) {
            BEGET_LOGE("dm_merge device exists but cannot get path");
            return -1;
        }
        BEGET_LOGI("dm_merge device already exists");
        return 0;
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
        BEGET_LOGE("collect dm_merge targets failed or empty");
        return -1;
    }
    rc = CreateDmMergeDevice(targets, ctx.num, dmDevPath, dmDevPathLen);
    DestroyDmMergeTargets(targets, ctx.num);
    return rc;
}

int TryDmMergeOverlay(Fstab *fstab)
{
    if (fstab == NULL) {
        BEGET_LOGE("fstab is NULL");
        return -1;
    }

    if (GetBootSlots() > 1) {
        FstabItem *item = fstab->head;
        while (item != NULL) {
            FsAdjustPartitionNameBySlot(item);
            item = item->next;
        }
    }

    char dmDevPath[MAX_BUFFER_LEN] = {0};
    if (GetOrCreateDmMergeDevice(fstab, dmDevPath, MAX_BUFFER_LEN) != 0) {
        BEGET_LOGE("get or create dm_merge device failed");
        return -1;
    }

    if (!CheckIsExt4(dmDevPath, 0)) {
        BEGET_LOGI("dm_merge not formatted as ext4, remove device and skip");
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }

    BEGET_LOGI("dm_merge device ready, ext4 detected, will mount after SwitchRoot");
    return 0;
}

bool IsDmMergeOverlayActive(void)
{
    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    char dmDevPath[MAX_BUFFER_LEN] = {0};
    bool active = false;
    if (DmGetDeviceName(fd, DM_MERGE_NAME, dmDevPath, MAX_BUFFER_LEN) == 0) {
        active = true;
    }
    close(fd);
    return active;
}

INIT_STATIC int RemoveDirContent(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        BEGET_LOGE("opendir %s failed", path);
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char childPath[MAX_BUFFER_LEN] = {0};
        if (snprintf_s(childPath, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s/%s", path, entry->d_name) < 0) {
            continue;
        }
        if (entry->d_type == DT_DIR) {
            RemoveDirContent(childPath);
            rmdir(childPath);
        } else {
            unlink(childPath);
        }
    }
    closedir(dir);
    return 0;
}

INIT_STATIC const char *PartNameToOverlayPath(const char *partName)
{
    struct {
        const char *name;
        const char *mnt;
    } partMap[] = {
        {"system", "/"},
        {"vendor", "/vendor"},
        {"sys_prod", "/sys_prod"},
        {"chip_prod", "/chip_prod"},
        {"preload", "/preload"},
        {"cust", "/cust"},
        {"version", "/version"},
    };
    for (size_t i = 0; i < ARRAY_LENGTH(partMap); i++) {
        if (strcmp(partName, partMap[i].name) == 0) {
            return OverlayPathFromMnt(partMap[i].mnt);
        }
    }
    return NULL;
}

int CheckDmMergeCleanup(void)
{
    if (!IsDmMergeOverlayActive()) {
        return 0;
    }
    char dmDevPath[MAX_BUFFER_LEN] = {0};
    if (GetDmMergeDevPathAndMountExt4(dmDevPath, MAX_BUFFER_LEN) != 0) {
        return -1;
    }
    if (!HasCleanupMarker()) {
        BEGET_LOGI("no cleanup marker, skip cleanup");
        umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
        return 0;
    }
    return PerformDmMergeCleanup();
}

int RefreshPartitionOverlay(void)
{
    char buf[HYPERHOLD_REFRESH_SIZE] = {0};
    ssize_t readLen = 0;
    if (ReadHyperholdRefreshString(buf, &readLen) != 0) {
        return 0;
    }
    const char *overlayPaths[MAX_OVERLAY_PARTITIONS] = {0};
    int overlayCount = 0;
    ParseRefreshPartNames(buf, overlayPaths, &overlayCount);
    if (!IsDmMergeOverlayActive()) {
        BEGET_LOGI("dm_merge overlay not active, per-partition mode does not support refresh");
        return 0;
    }
    if (overlayCount == 0) {
        BEGET_LOGI("no valid partition names parsed, skip refresh");
        return 0;
    }
    char dmDevPath[MAX_BUFFER_LEN] = {0};
    if (GetDmMergeDevPathAndMountExt4(dmDevPath, MAX_BUFFER_LEN) != 0) {
        return -1;
    }
    BEGET_LOGI("partition refresh: %d partitions to refresh", overlayCount);
    return RemoveOverlayContent(overlayPaths, overlayCount);
}

int MountDmMergeOverlayAll(void)
{
    if (!IsDmMergeOverlayActive()) {
        BEGET_LOGI("dm_merge overlay not active, skip mount all");
        return 0;
    }
    char dmDevPath[MAX_BUFFER_LEN] = {0};
    if (GetDmMergeDevPathAndMountExt4(dmDevPath, MAX_BUFFER_LEN) != 0) {
        return -1;
    }
    char markerPath[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(markerPath, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
        "%s/%s", PREFIX_OVERLAY_MERGE, DM_MERGE_MARKER) < 0) {
        BEGET_LOGE("construct marker path failed");
        umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    if (access(markerPath, F_OK) != 0) {
        BEGET_LOGI("dm_merge marker not found, umount and skip");
        umount2(PREFIX_OVERLAY_MERGE, MNT_DETACH);
        FsDmRemoveDevice(DM_MERGE_NAME);
        return -1;
    }
    return MountAllDmMergeOverlayPaths();
}
