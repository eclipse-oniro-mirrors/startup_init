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
#include "securec.h"
#include "init_utils.h"
#include "fs_dm.h"
#include "fs_manager/fs_manager.h"

#include "erofs_mount_overlay.h"

#define BYTE_UNIT 1024
#define ALIGN_BLOCK_SIZE (16 * BYTE_UNIT)
#define MIN_DM_SIZE (8 * BYTE_UNIT * BYTE_UNIT)
#define BLOCK_SIZE_UINT 4096
#define EXTHDR_MAGIC 0xE0F5E1E2
#define EXTHDR_BLKSIZE 4096

struct extheader_v1 {
    uint32_t magic_number;
    uint16_t exthdr_size;
    uint16_t bcc16;
    uint64_t part_size;
};

static void AllocDmName(const char *name, char *nameRofs, const uint64_t nameRofsLen,
    char *nameExt4, const uint64_t nameExt4Len)
{
    if (snprintf_s(nameRofs, nameRofsLen, nameRofsLen - 1, "%s_erofs", name) < 0) {
        BEGET_LOGE("Failed to copy nameRofs.");
        return;
    }

    if (snprintf_s(nameExt4, nameExt4Len, nameExt4Len - 1, "%s_erofs", name) < 0) {
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

static uint64_t LookupErofsEnd(const char *dev)
{
    int fd = -1;
    fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("open dev:[%s] failed.", dev);
        return 0;
    }

    if (lseek(fd, EROFS_SUPER_BLOCK_START_POSITION, SEEK_SET) < 0) {
        BEGET_LOGE("lseek dev:[%s] failed.", dev);
        close(fd);
        return 0;
    }

    struct erofs_super_block sb;
    ssize_t nbytes = read(fd, &sb, sizeof(sb));
    if (nbytes != sizeof(sb)) {
        BEGET_LOGE("read dev:[%s] failed.", dev);
        close(fd);
        return 0;
    }
    close(fd);

    if (sb.magic != EROFS_SUPER_MAGIC) {
        BEGET_LOGE("dev:[%s] is not erofs system, magic is 0x%x", dev, sb.magic);
        return 0;
    }

    uint64_t erofsSize = sb.blocks * BLOCK_SIZE_UINT;
    return erofsSize;
}

static uint64_t GetTotalSize(const char *dev, uint64_t offset)
{
    int fd = -1;
    fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("open dev:[%s] failed.", dev);
        return 0;
    }

    if (lseek(fd, offset, SEEK_SET) < 0) {
        BEGET_LOGE("lseek dev:[%s] failed, offset is %llu", dev, offset);
        close(fd);
        return 0;
    }

    struct extheader_v1 header;
    ssize_t nbytes = read(fd, &header, sizeof(header));
    if (nbytes != sizeof(header)) {
        BEGET_LOGE("read dev:[%s] failed.", dev);
        close(fd);
        return 0;
    }
    close(fd);

    if (header.magic_number != EXTHDR_MAGIC) {
        BEGET_LOGE("dev:[%s] is not have ext path, magic is 0x%x", dev, header.magic_number);
        return 0;
    }

    return header.part_size;
}

static uint64_t GetFsSize(int fd)
{
    struct stat st;
    if (fstat(fd, &st) == -1) {
        BEGET_LOGE("fstat failed. errno: %d", errno);
        return 0;
    }

    uint64_t size = 0;
    if (S_ISBLK(st.st_mode)) {
        if (ioctl(fd, BLKGETSIZE64, &size) == -1) {
            BEGET_LOGE("ioctl failed. errno: %d", errno);
            return 0;
        }
    } else if (S_ISREG(st.st_mode)) {
        size = (uint64_t)st.st_size;
    } else {
        BEGET_LOGE("unspported type st_mode:[%llu]", st.st_mode);
        errno = EACCES;
        return 0;
    }

    BEGET_LOGI("get fs size:[%llu]", size);
    return size;
}

static uint64_t GetBlockSize(const char *dev)
{
    int fd = -1;
    fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("open dev:[%s] failed.", dev);
        return 0;
    }

    uint64_t blockSize = GetFsSize(fd);
    close(fd);
    return blockSize;
}

static int GetMapperAddr(const char *dev, uint64_t *start, uint64_t *length)
{
    uint64_t totalSize;
    *start = LookupErofsEnd(dev);
    if (*start == 0) {
        BEGET_LOGE("get erofs end failed.");
        return -1;
    }

    totalSize = GetTotalSize(dev, *start);
    if (totalSize > 0) {
        *start += EXTHDR_BLKSIZE;
    } else {
        totalSize = GetBlockSize(dev);
        if (totalSize == 0) {
            BEGET_LOGE("get block size failed.");
            return -1;
        }
    }

    BEGET_LOGI("total size:[%llu], used size: [%llu], empty size:[%llu] on dev: [%s]",
        totalSize, *start, totalSize - *start, dev);

    if (totalSize > *start) {
        *length = totalSize - *start;
    } else {
        *length = 0;
    }

    if (*length < MIN_DM_SIZE) {
        BEGET_LOGE("empty size is too small, skip...");
        return -1;
    }

    return 0;
}

static void ConstructLinearTarget(DmVerityTarget *target, const char *dev, uint64_t mapStart, uint64_t mapLength)
{
    if (target == NULL || dev == NULL) {
        return;
    }

    target->start = 0;
    target->length = mapLength / SECTOR_SIZE;
    target->paras = calloc(1, MAX_BUFFER_LEN);

    if (snprintf_s(target->paras, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s %lld", dev, mapStart / SECTOR_SIZE) < 0) {
        BEGET_LOGE("Failed to  copy target paras.");
        return;
    }
    target->paras_len = strlen(target->paras);
    BEGET_LOGI("dev [%s], linearparas [%s], length [%s]", dev, target->paras, target->paras_len);
}

static int GetOverlayDevice(FstabItem *item, char *devRofs, const uint32_t devRofsLen,
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
    ConstructLinearTarget(&dmRofsTarget, item->deviceName, 0, mapStart);
    if (FsDmCreateLinearDevice(nameRofs, devRofs, devRofsLen, &dmRofsTarget)) {
        BEGET_LOGE("fs create rofs linear device failed, dev is [%s]", item->deviceName);
        return -1;
    }

    DmVerityTarget dmExt4Target = {0};
    ConstructLinearTarget(&dmExt4Target, item->deviceName, mapStart, mapLength);
    if (FsDmCreateLinearDevice(nameExt4, devExt4, devExt4Len, &dmExt4Target)) {
        BEGET_LOGE("fs create ext4 linear device failed, dev is [%s]", item->deviceName);
        return -1;
    }
    BEGET_LOGI("get overlay device success , dev is [%s]", item->deviceName);
    return 0;
}

static int MountRofsDevice(const char *dev, const char *mnt)
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
    }

    return rc;
}

int MountExt4Device(const char *dev, const char *mnt, bool isFirstMount)
{
    int ret = 0;
    char dirExt4[MAX_BUFFER_LEN] = {0};
    char dirUpper[MAX_BUFFER_LEN] = {0};
    char dirWork[MAX_BUFFER_LEN] = {0};
    ret = snprintf_s(dirExt4, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s", mnt);
    if (ret < 0) {
        BEGET_LOGE("dirExt4 copy failed errno %d.", errno);
        return -1;
    }

    ret = snprintf_s(dirUpper, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s"PREFIX_UPPER, mnt);
    if (ret < 0) {
        BEGET_LOGE("dirUpper copy failed errno %d.", errno);
        return -1;
    }

    ret = snprintf_s(dirWork, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, PREFIX_OVERLAY"%s"PREFIX_WORK, mnt);
    if (ret < 0) {
        BEGET_LOGE("dirWork copy failed errno %d.", errno);
        return -1;
    }

    if (mkdir(dirExt4, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir %s failed.", dirExt4);
        return -1;
    }

    int retryCount = 3;
    while (retryCount-- > 0) {
        ret = mount(dev, dirExt4, "ext4", MS_NOATIME | MS_NODEV, NULL);
        if (ret && (errno != EBUSY)) {
            BEGET_LOGI("mount ext4 dev [%s] on mnt [%s] failed, retry", dev, dirExt4);
            sleep(1);
            continue;
        }
    }

    if (isFirstMount && mkdir(dirUpper, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir dirUpper:%s failed.", dirUpper);
        return -1;
    }

    if (isFirstMount && mkdir(dirWork, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir dirWork:%s failed.", dirWork);
        return -1;
    }

    return ret;
}

static int MountPartitionDevice(FstabItem *item, const char *devRofs, const char *devExt4)
{
    struct stat statInfo;
    if (!lstat(item->mountPoint, &statInfo)) {
        if ((statInfo.st_mode & S_IFMT) == S_IFLNK) {
            unlink(item->mountPoint);
        }
    }

    if (mkdir(item->mountPoint, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir mountPoint:%s failed.errno %d", item->mountPoint, errno);
        return -1;
    }

    WaitForFile(devRofs, WAIT_MAX_SECOND);
    WaitForFile(devExt4, WAIT_MAX_SECOND);

    if (mkdir(PREFIX_LOWER, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir /lower failed. errno: %d", errno);
        return -1;
    }

    if (mkdir(PREFIX_OVERLAY, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir /overlay failed. errno: %d", errno);
        return -1;
    }

    char dirLower[MAX_BUFFER_LEN] = {0};
    if (snprintf_s(dirLower, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s%s", PREFIX_LOWER, item->mountPoint) < 0) {
        BEGET_LOGE("dirLower[%s]copy failed errno %d.", dirLower, errno);
        return -1;
    }

    if (mkdir(dirLower, MODE_MKDIR) && (errno != EEXIST)) {
        BEGET_LOGE("mkdir dirLower[%s] failed. errno: %d", dirLower, errno);
        return -1;
    }

    if (MountRofsDevice(devRofs, item->mountPoint)) {
        BEGET_LOGE("mount erofs dev [%s] on mnt [%s] failed", devRofs, item->mountPoint);
        return -1;
    }

    if (MountRofsDevice(devRofs, dirLower)) {
        BEGET_LOGE("mount erofs dev [%s] on mnt [%s] failed", devRofs, dirLower);
        return -1;
    }

    if (!CheckIsExt4(devExt4, 0)) {
        BEGET_LOGI("is not ext4 devExt4 [%s] on mnt [%s]", devExt4, item->mountPoint);
        return 0;
    }

    BEGET_LOGI("is ext4 devExt4 [%s] on mnt [%s]", devExt4, item->mountPoint);
    if (MountExt4Device(devExt4, item->mountPoint, false)) {
        BEGET_LOGE("mount ext4 dev [%s] on mnt [%s] failed", devExt4, item->mountPoint);
        return -1;
    }

    return 0;
}

int DoMountOverlayDevice(FstabItem *item)
{
    char devRofs[MAX_BUFFER_LEN] = {0};
    char devExt4[MAX_BUFFER_LEN] = {0};
    int rc = 0;
    rc = GetOverlayDevice(item, devRofs, MAX_BUFFER_LEN, devExt4, MAX_BUFFER_LEN);
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
        BEGET_LOGE("init ext4 dm dev failed");
        return -1;
    }
    return rc;
}