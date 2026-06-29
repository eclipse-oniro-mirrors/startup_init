/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <sys/wait.h>
#include "securec.h"
#include "init_utils.h"

#include "erofs_overlay_common.h"
#define BLOCK_SIZE_ULL 4096ULL

bool IsOverlayEnable(void)
{
    char oemMode[MAX_BUFFER_LEN] = {0};
    int ret = GetExactParameterFromCmdLine("oemmode", oemMode, MAX_BUFFER_LEN);
    if (ret) {
        BEGET_LOGE("Failed get oenmode from cmdline.");
        return false;
    }

    char buildvariant[MAX_BUFFER_LEN] = {0};
    ret = GetExactParameterFromCmdLine("buildvariant", buildvariant, MAX_BUFFER_LEN);
    if (ret) {
        BEGET_LOGE("Failed get buildvariant from cmdline.");
        return false;
    }

    if (strcmp(oemMode, "user") == 0 || strcmp(buildvariant, "eng") != 0) {
        BEGET_LOGI("not support overlay, oemMode:[%s] buildvariant:[%s]", oemMode, buildvariant);
        return false;
    }
    BEGET_LOGI("overlay enable.");
    return true;
}

bool CheckIsExt4(const char *dev, uint64_t offset)
{
    int fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("cannot open [dev]:%s", dev);
        return false;
    }

    if (lseek(fd, offset + EXT4_SUPER_BLOCK_START_POSITION, SEEK_SET) < 0) {
        BEGET_LOGE("cannot seek [dev]:%s", dev);
        close(fd);
        return false;
    }
    ext4_super_block superBlock;
    ssize_t nbytes = read(fd, &superBlock, sizeof(superBlock));
    if (nbytes != sizeof(superBlock)) {
        BEGET_LOGE("read ext4 super block fail");
        close(fd);
        return false;
    }

    if (superBlock.s_magic == EXT4_SUPER_MAGIC) {
        BEGET_LOGI("this [dev] %s  is ext4:[block cout]: %d, [size]: %llu", dev,
            superBlock.s_blocks_count_lo, (superBlock.s_blocks_count_lo * BLOCK_SIZE_ULL));
        close(fd);
        return true;
    }
    close(fd);
    return false;
}

uint64_t LookupErofsEnd(const char *dev)
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

    uint64_t erofsSize = (uint64_t)sb.blocks * BLOCK_SIZE_UINT;
    return erofsSize;
}

uint64_t GetImgSize(const char *dev, uint64_t offset)
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
        BEGET_LOGI("dev:[%s] is not have ext path, magic is 0x%x", dev, header.magic_number);
        return 0;
    }
    BEGET_LOGI("get img size [%llu]", header.part_size);
    return header.part_size;
}

uint64_t GetFsSize(int fd)
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
        if (st.st_size < 0) {
            BEGET_LOGE("st_size is not right. st_size: %lld", st.st_size);
            return 0;
        }
        size = (uint64_t)st.st_size;
    } else {
        BEGET_LOGE("unspported type st_mode:[%llu]", st.st_mode);
        errno = EACCES;
        return 0;
    }

    BEGET_LOGI("get fs size:[%llu]", size);
    return size;
}

uint64_t GetBlockSize(const char *dev)
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

uint64_t AlignTo(uint64_t base, uint64_t alignment)
{
    if (alignment == 0) {
        return base;
    }
    return (((base - 1) / alignment + 1) * alignment);
}

int GetMapperAddr(const char *dev, uint64_t *start, uint64_t *length)
{
    *start = LookupErofsEnd(dev);
    if (*start == 0) {
        BEGET_LOGE("get erofs end failed.");
        return -1;
    }

    uint64_t imgSize = GetImgSize(dev, *start);
    if (imgSize > 0) {
        *start = AlignTo(imgSize, ALIGN_BLOCK_SIZE);
    }

    uint64_t totalSize = GetBlockSize(dev);
    if (totalSize == 0) {
        BEGET_LOGE("get block size failed.");
        return -1;
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

int GetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length)
{
    BEGET_LOGI("getmapper form dev: [%s]", dev);
    *start = LookupErofsEnd(dev);
    if (*start == 0) {
        BEGET_LOGE("get erofs end failed for merge.");
        return -1;
    }

    uint64_t imgSize = GetImgSize(dev, *start);
    if (imgSize > 0) {
        *start = AlignTo(imgSize, ALIGN_BLOCK_SIZE);
    }

    uint64_t totalSize = GetBlockSize(dev);
    if (totalSize == 0) {
        BEGET_LOGE("get block size failed for merge.");
        return -1;
    }

    BEGET_LOGI("merge: total size:[%llu], used size: [%llu], empty size:[%llu] on dev: [%s]",
        totalSize, *start, totalSize - *start, dev);

    if (totalSize > *start) {
        *length = totalSize - *start;
    } else {
        *length = 0;
    }

    return 0;
}

bool CheckIsErofs(const char *dev)
{
    int fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("cannot open [dev]:%s", dev);
        return false;
    }

    if (lseek(fd, EROFS_SUPER_BLOCK_START_POSITION, SEEK_SET) < 0) {
        BEGET_LOGE("cannot seek [dev]:%s", dev);
        close(fd);
        return false;
    }
    struct erofs_super_block superBlock;
    ssize_t nbytes = read(fd, &superBlock, sizeof(superBlock));
    if (nbytes != sizeof(superBlock)) {
        BEGET_LOGE("read erofs super block fail");
        close(fd);
        return false;
    }

    BEGET_LOGI("the [dev] %s magic [%u]", dev, superBlock.magic);
    if (superBlock.magic == EROFS_SUPER_MAGIC) {
        BEGET_LOGI("this [dev] %s is erofs", dev);
        close(fd);
        return true;
    }
    close(fd);
    return false;
}