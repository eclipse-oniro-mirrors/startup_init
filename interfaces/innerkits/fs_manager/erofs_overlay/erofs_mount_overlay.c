/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "securec.h"
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif
#include "init_utils.h"
#include "fs_dm.h"
#include "switch_root.h"
#include "fs_manager/fs_manager.h"

#include "erofs_mount_overlay.h"
#include "erofs_remount_overlay.h"
#include "dm_merge_overlay.h"

INIT_STATIC void AllocDmName(const char *name, char *nameRofs, const uint64_t nameRofsLen,
    char *nameExt4, const uint64_t nameExt4Len)
{
    if (snprintf_s(nameRofs, nameRofsLen, nameRofsLen - 1, "%s_erofs", name) < 0) {
        BEGET_LOGE("Failed to copy nameRofs.");
        return;
    }

    if (snprintf_s(nameExt4, nameExt4Len, nameExt4Len - 1, "%s_ext4", name) < 0) {
        BEGET_LOGE("Failed to copy nameExt4.");
        return;
    }

    uint64_t i = 0;
    while (nameRofs[i] != '\0') {
        if (nameRofs[i] == '/') {
            nameRofs[i] = '_';
        }
        i++;
    }
    i = 0;
    while (nameExt4[i] != '\0') {
        if (nameExt4[i] == '/') {
            nameExt4[i] = '_';
        }
        i++;
    }

    BEGET_LOGI("alloc dm namerofs:[%s], nameext4:[%s]", nameRofs, nameExt4);
}

INIT_STATIC int ConstructLinearTarget(DmVerityTarget *target, const char *dev, uint64_t mapStart, uint64_t mapLength)
{
    if (target == NULL || dev == NULL) {
        return -1;
    }

    target->start = 0;
    target->length = mapLength / SECTOR_SIZE;
    target->paras = calloc(1, MAX_BUFFER_LEN);
    if (target->paras == NULL) {
        BEGET_LOGE("Failed to calloc target paras");
        return -1;
    }

    if (snprintf_s(target->paras, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s %llu", dev, mapStart / SECTOR_SIZE) < 0) {
        BEGET_LOGE("Failed to copy target paras.");
        return -1;
    }
    target->paras_len = strlen(target->paras);
    return 0;
}

INIT_STATIC void DestoryLinearTarget(DmVerityTarget *target)
{
    if (target != NULL && target->paras != NULL) {
        free(target->paras);
        target->paras = NULL;
    }
}

INIT_STATIC int GetOverlayDevice(FstabItem *item, char *devRofs, const uint32_t devRofsLen,
    char *devExt4, const uint32_t devExt4Len)
{
    uint64_t mapStart;
    uint64_t mapLength;
    char nameExt4[MAX_BUFFER_LEN] = {0};
    char nameRofs[MAX_BUFFER_LEN] = {0};

    if (access(item->deviceName, 0) < 0) {
        BEGET_LOGE("connot access dev [%s]", item->deviceName);
        return -1;
    }

    AllocDmName(item->mountPoint, nameRofs, MAX_BUFFER_LEN, nameExt4, MAX_BUFFER_LEN);

    if (GetMapperAddr(item->deviceName, &mapStart, &mapLength)) {
        BEGET_LOGE("get mapper addr failed, dev is [%s]", item->deviceName);
        return -1;
    }

    DmVerityTarget dmRofsTarget = {0};
    DmVerityTarget dmExt4Target = {0};

    int rc = ConstructLinearTarget(&dmRofsTarget, item->deviceName, 0, mapStart);
    if (rc != 0) {
        BEGET_LOGE("fs construct erofs linear target failed, dev is [%s]", item->deviceName);
        goto exit;
    }
    rc = FsDmCreateLinearDevice(nameRofs, devRofs, devRofsLen, &dmRofsTarget);
    if (rc != 0) {
        BEGET_LOGE("fs create erofs linear device failed, dev is [%s]", item->deviceName);
        goto exit;
    }

    rc = ConstructLinearTarget(&dmExt4Target, item->deviceName, mapStart, mapLength);
    if (rc != 0) {
        BEGET_LOGE("fs construct ext4 linear target failed, dev is [%s]", item->deviceName);
        goto exit;
    }
    rc = FsDmCreateLinearDevice(nameExt4, devExt4, devExt4Len, &dmExt4Target);
    if (rc != 0) {
        BEGET_LOGE("fs create ext4 linear device failed, dev is [%s]", item->deviceName);
        goto exit;
    }
    BEGET_LOGI("get overlay device success , dev is [%s]", item->deviceName);
exit:
    DestoryLinearTarget(&dmRofsTarget);
    DestoryLinearTarget(&dmExt4Target);
    return rc;
}

INIT_STATIC int MountRofsDevice(const char *dev, const char *mnt)
{
    int rc = 0;
    int retryCount = 3;
    while (retryCount-- > 0) {
        rc = mount(dev, mnt, "erofs", MS_RDONLY, NULL);
        if (rc && (errno != EBUSY)) {
            BEGET_LOGI("mount erofs dev [%s] on mnt [%s] failed, retry", dev, mnt);
            sleep(1);
            continue;
        }
        break;
    }

    return 0;
}

void SetSelinuxContext(const char *mnt)
{
#ifdef WITH_SELINUX
    const char* fsFileContext = "u:object_r:system_file:s0";
    const char* vendorFileContext = "u:object_r:vendor_file:s0";
    BEGET_LOGI("start to set selinux. mnt:%s", mnt);
    if (strcmp(mnt, "/vendor") == 0) {
        setfscreatecon(vendorFileContext);
    } else {
        setfscreatecon(fsFileContext);
    }
#endif
}

void ClearSelinuxContext(void)
{
#ifdef WITH_SELINUX
    setfscreatecon(NULL);
#endif
}

INIT_STATIC int MkdirExt4OverlayDirs(const char *mnt)
{
    char dirExt4[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(dirExt4, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s", mnt) < 0) {
        BEGET_LOGE("dirExt4 copy failed errno %d.", errno);
        return -1;
    }
    if (mkdir(dirExt4, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir %s failed.", dirExt4);
        return -1;
    }
    return 0;
}

INIT_STATIC int MkdirExt4UpperWorkDirs(const char *mnt)
{
    char dirUpper[MAX_BUFFER_LEN] = {0};
    char dirWork[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(dirUpper, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s"PREFIX_UPPER, mnt) < 0) {
        BEGET_LOGE("dirUpper copy failed errno %d.", errno);
        return -1;
    }
    if (snprintf_s(dirWork, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s"PREFIX_WORK, mnt) < 0) {
        BEGET_LOGE("dirWork copy failed errno %d.", errno);
        return -1;
    }
    if (mkdir(dirUpper, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir dirUpper:%s failed.", dirUpper);
        return -1;
    }
    if (mkdir(dirWork, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir dirWork:%s failed.", dirWork);
        return -1;
    }
    return 0;
}

int MountExt4Device(const char *dev, const char *mnt, bool isFirstMount)
{
    if (isFirstMount) {
        SetSelinuxContext(mnt);
    }

    if (MkdirExt4OverlayDirs(mnt) != 0) {
        goto exit;
    }

    char dirExt4[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(dirExt4, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s", mnt) < 0) {
        BEGET_LOGE("dirExt4 copy failed errno %d.", errno);
        goto exit;
    }

    int retryCount = 3;
    while (retryCount-- > 0) {
        int ret = mount(dev, dirExt4, "ext4", MS_NOATIME | MS_NODEV, NULL);
        if (ret && (errno != EBUSY)) {
            BEGET_LOGI("mount ext4 dev [%s] on mnt [%s] failed, retry", dev, dirExt4);
            sleep(1);
            continue;
        }
        break;
    }

    if (isFirstMount && MkdirExt4UpperWorkDirs(mnt) != 0) {
        BEGET_LOGE("mkdir upper/work dirs failed for mnt %s", mnt);
    }

exit:
    if (isFirstMount) {
        ClearSelinuxContext();
    }
    return 0;
}

INIT_STATIC void UnlinkMountPoint(const char *mountPoint)
{
    struct stat statInfo;
    if (!lstat(mountPoint, &statInfo)) {
        if ((statInfo.st_mode & S_IFMT) == S_IFLNK) {
            unlink(mountPoint);
        }
    }
}

INIT_STATIC int PrepareMountPoint(FstabItem *item)
{
    UnlinkMountPoint(item->mountPoint);
    if (mkdir(item->mountPoint, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir mountPoint:%s failed.errno %d", item->mountPoint, errno);
        return -1;
    }
    return 0;
}

INIT_STATIC int MountLowerDir(const char *devRofs, const char *overlayPath)
{
    if (mkdir(PREFIX_LOWER, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir /lower failed. errno: %d", errno);
        return -1;
    }
    char dirLower[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(dirLower, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s%s", PREFIX_LOWER, overlayPath) < 0) {
        BEGET_LOGE("dirLower copy failed errno %d.", errno);
        return -1;
    }
    if (mkdir(dirLower, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir dirLower[%s] failed. errno: %d", dirLower, errno);
        return -1;
    }
    if (MountRofsDevice(devRofs, dirLower)) {
        BEGET_LOGE("mount erofs dev [%s] on mnt [%s] failed", devRofs, dirLower);
        return -1;
    }
    return 0;
}

INIT_STATIC int MountPartitionDevice(FstabItem *item, const char *devRofs, const char *devExt4)
{
    if (PrepareMountPoint(item) != 0) {
        return -1;
    }
    WaitForFile(devRofs, WAIT_MAX_SECOND);
    WaitForFile(devExt4, WAIT_MAX_SECOND);

    if (MountRofsDevice(devRofs, item->mountPoint)) {
        BEGET_LOGE("mount erofs dev [%s] on mnt [%s] failed", devRofs, item->mountPoint);
        return -1;
    }
    if (strcmp(item->mountPoint, "/usr") == 0) {
        SwitchRoot("/usr");
    }
    if (mkdir(PREFIX_OVERLAY, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir /overlay failed. errno: %d", errno);
        return -1;
    }
    if (MountLowerDir(devRofs, item->mountPoint) != 0) {
        return -1;
    }
    if (!CheckIsExt4(devExt4, 0)) {
        BEGET_LOGI("is not ext4 devExt4 [%s] on mnt [%s]", devExt4, item->mountPoint);
        return 0;
    }
    BEGET_LOGI("is ext4 devExt4 [%s] on mnt [%s]", devExt4, item->mountPoint);
    if (MountExt4Device(devExt4, item->mountPoint, false)) {
        BEGET_LOGW("mount ext4 dev [%s] on mnt [%s] failed, skip ext4 overlay", devExt4, item->mountPoint);
        return 0;
    }
    return 0;
}

int DoMountOverlayDevice(FstabItem *item)
{
    char devRofs[MAX_BUFFER_LEN] = {0};
    char devExt4[MAX_BUFFER_LEN] = {0};
    int rc = GetOverlayDevice(item, devRofs, MAX_BUFFER_LEN, devExt4, MAX_BUFFER_LEN);
    if (rc) {
        BEGET_LOGE("get overlay device failed, source [%s] target [%s]", item->deviceName, item->mountPoint);
        return -1;
    }

    rc = FsDmInitDmDev(devRofs, true);
    if (rc) {
        BEGET_LOGE("init erofs dm dev failed");
        return -1;
    }

    rc = FsDmInitDmDev(devExt4, true);
    if (rc) {
        BEGET_LOGE("init ext4 dm dev failed");
        return -1;
    }

    rc = MountPartitionDevice(item, devRofs, devExt4);
    if (rc) {
        BEGET_LOGE("mount partition device failed");
        return -1;
    }
    BEGET_LOGI("mount overlay device %s on %s success", item->deviceName, item->mountPoint);
    return 0;
}

INIT_STATIC int GetErofsDmDevice(FstabItem *item, char *devRofs, const uint32_t devRofsLen)
{
    uint64_t mapStart = 0;
    uint64_t mapLength = 0;
    char nameRofs[MAX_BUFFER_LEN] = {0};
    char nameExt4[MAX_BUFFER_LEN] = {0};

    if (access(item->deviceName, 0) < 0) {
        BEGET_LOGE("cannot access dev [%s]", item->deviceName);
        return -1;
    }

    AllocDmName(item->mountPoint, nameRofs, MAX_BUFFER_LEN, nameExt4, MAX_BUFFER_LEN);

    if (GetMapperAddrForMerge(item->deviceName, &mapStart, &mapLength)) {
        BEGET_LOGE("get mapper addr for merge failed, dev is [%s]", item->deviceName);
        return -1;
    }

    if (mapStart == 0) {
        BEGET_LOGE("erofs data size is 0, dev is [%s]", item->deviceName);
        return -1;
    }

    DmVerityTarget dmRofsTarget = {0};
    int rc = ConstructLinearTarget(&dmRofsTarget, item->deviceName, 0, mapStart);
    if (rc != 0) {
        BEGET_LOGE("construct erofs linear target failed, dev is [%s]", item->deviceName);
        DestoryLinearTarget(&dmRofsTarget);
        return -1;
    }
    rc = FsDmCreateLinearDevice(nameRofs, devRofs, devRofsLen, &dmRofsTarget);
    DestoryLinearTarget(&dmRofsTarget);
    if (rc != 0) {
        BEGET_LOGE("create erofs linear device failed, dev is [%s]", item->deviceName);
        return -1;
    }

    BEGET_LOGI("get erofs dm device success, dev is [%s]", item->deviceName);
    return 0;
}

INIT_STATIC int MountDmMergeErofsOnly(FstabItem *item)
{
    const char *overlayPath = OverlayPathFromMnt(item->mountPoint);
    char devRofs[MAX_BUFFER_LEN] = {0};
    int rc = GetErofsDmDevice(item, devRofs, MAX_BUFFER_LEN);
    if (rc != 0) {
        BEGET_LOGE("get erofs dm device failed, source [%s] target [%s]", item->deviceName, item->mountPoint);
        return -1;
    }
    rc = FsDmInitDmDev(devRofs, true);
    if (rc != 0) {
        BEGET_LOGE("init erofs dm dev failed");
        return -1;
    }
    if (PrepareMountPoint(item) != 0) {
        return -1;
    }
    WaitForFile(devRofs, WAIT_MAX_SECOND);
    if (MountRofsDevice(devRofs, item->mountPoint)) {
        BEGET_LOGE("mount erofs dev [%s] on mnt [%s] failed", devRofs, item->mountPoint);
        return -1;
    }
    if (strcmp(item->mountPoint, "/usr") == 0) {
        SwitchRoot("/usr");
    }
    if (MountLowerDir(devRofs, overlayPath) != 0) {
        return -1;
    }
    BEGET_LOGI("mount dm_merge erofs-only device %s on %s success", item->deviceName, item->mountPoint);
    return 0;
}

int DoMountDmMergeErofsOnly(FstabItem *item)
{
    if (!IsDmMergeOverlayActive()) {
        BEGET_LOGE("dm_merge overlay not active");
        return -1;
    }
    return MountDmMergeErofsOnly(item);
}
