/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "beget_ext.h"
#include "fs_dm.h"
#include "fs_hvb.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARTITION_PATH_PREFIX "/dev/block/by-name/"
#define MAX_EXT_HVB_PARTITION_NUM 1

static const ExtHvbVerifiedDev g_extVerifiedDev[] = {
    {
        .partitionName = "module_update",
        .pathName = "/data/module_update/active/ModuleTrain/module.img"
    },
};

static int64_t GetImageSizeForHVB(const int fd, const char* image)
{
    char headerBuf[SZ_2KB] = {0};
    ssize_t nbytes;
    uint64_t fileSysSize;
    uint64_t realFileSize;

    if (fd < 0) {
        BEGET_LOGE("param is error");
        return -1;
    }

    lseek64(fd, 0, SEEK_SET);
    nbytes = read(fd, &headerBuf, sizeof(headerBuf));
    if (nbytes != sizeof(headerBuf)) {
        BEGET_LOGE("read file system header fail");
        return -1;
    }

    if (CheckAndGetErofsSize(&headerBuf[SZ_1KB], &fileSysSize, image) &&
        CheckAndGetExtheaderSize(fd, fileSysSize, &realFileSize, image)) {
        BEGET_LOGI("get %s size from erofs and extheader, size=0x%lx", image, realFileSize);
        return realFileSize;
    } else if (CheckAndGetExt4Size(&headerBuf[SZ_1KB], &fileSysSize, image) &&
        CheckAndGetExtheaderSize(fd, fileSysSize, &realFileSize, image)) {
        BEGET_LOGI("get %s size from ext4 and extheader, size=0x%lx", image, realFileSize);
        return realFileSize;
    } else {
        BEGET_LOGI("read %s file extheader fail, use the actual img size", image);
    }

    return lseek64(fd, 0, SEEK_END);
}

static bool IsExtHvbVerifiedPtn(const char *ptn, size_t ptnLen, size_t *outIndex)
{
    int index;
    if (ptn == NULL || outIndex == NULL) {
        BEGET_LOGE("error, invalid ptn");
        return false;
    }

    for (index = 0; index < MAX_EXT_HVB_PARTITION_NUM; index++) {
        if (strncmp(g_extVerifiedDev[index].partitionName, ptn, ptnLen) == 0) {
            *outIndex = index;
            return true;
        }
    }

    return false;
}

static char *GetExtHvbVerifiedPath(size_t index)
{
    char *tmpPath = NULL;

    if (index >= MAX_EXT_HVB_PARTITION_NUM) {
        BEGET_LOGE("error, invalid index");
        return NULL;
    }

    size_t pathLen = strnlen(g_extVerifiedDev[index].pathName, FS_HVB_MAX_PATH_LEN);
    if (pathLen >= FS_HVB_MAX_PATH_LEN) {
        BEGET_LOGE("error, invalid path");
        return tmpPath;
    }
    tmpPath = malloc(pathLen + 1);
    if (tmpPath == NULL) {
        BEGET_LOGE("error, fail to malloc");
        return NULL;
    }

    (void)memset_s(tmpPath, pathLen + 1, 0, pathLen + 1);
    if (memcpy_s(tmpPath, pathLen + 1, g_extVerifiedDev[index].pathName, pathLen) != 0) {
        free(tmpPath);
        BEGET_LOGE("error, fail to copy");
        return NULL;
    }
    return tmpPath;
}

static char *HvbGetPartitionPath(const char *partition)
{
    int rc;
    size_t pathLen;
    char *path = NULL;
    size_t index;
    size_t ptnLen = strnlen(partition, HVB_MAX_PARTITION_NAME_LEN);
    if (ptnLen >= HVB_MAX_PARTITION_NAME_LEN) {
        BEGET_LOGE("error, invalid ptn name");
        return NULL;
    }
    if (IsExtHvbVerifiedPtn(partition, ptnLen, &index)) {
        return GetExtHvbVerifiedPath(index);
    }

    pathLen = strlen(PARTITION_PATH_PREFIX) + ptnLen;
    path = calloc(1, pathLen + 1);
    if (path == NULL) {
        BEGET_LOGE("error, calloc fail");
        return NULL;
    }
    rc = snprintf_s(path, pathLen + 1, pathLen, "%s%s", PARTITION_PATH_PREFIX,
                    partition);
    if (rc < 0) {
        BEGET_LOGE("error, snprintf_s fail, ret = %d", rc);
        free(path);
        return NULL;
    }
    return path;
}

static enum hvb_io_errno HvbReadFromPartition(struct hvb_ops* ops,
                                              const char* partition,
                                              int64_t offset, uint64_t numBytes,
                                              void* buf, uint64_t* outNumRead)
{
    int fd = -1;
    char *path = NULL;
    enum hvb_io_errno ret = HVB_IO_ERROR_IO;

    if (ops == NULL || partition == NULL ||
        buf == NULL || outNumRead == NULL) {
        BEGET_LOGE("error, invalid parameter");
        return HVB_IO_ERROR_IO;
    }

    /* get and malloc img path */
    path = HvbGetPartitionPath(partition);
    if (path == NULL) {
        BEGET_LOGE("error, get partition path");
        ret = HVB_IO_ERROR_OOM;
        goto exit;
    }

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error, Failed to open %s, errno = %d", path, errno);
        ret = HVB_IO_ERROR_IO;
        goto exit;
    }

    if (offset < 0) {
        int64_t total_size = GetImageSizeForHVB(fd, partition);
        if (total_size == -1) {
            BEGET_LOGE("Failed to get the size of the partition %s", partition);
            ret = HVB_IO_ERROR_IO;
            goto exit;
        }
        offset = total_size + offset;
    }

    lseek64(fd, offset, SEEK_SET);

    ssize_t numRead = read(fd, buf, numBytes);
    if (numRead < 0 || (size_t)numRead != numBytes) {
        BEGET_LOGE("Failed to read %lld bytes from %s offset %lld", numBytes,
                   path, offset);
        ret = HVB_IO_ERROR_IO;
        goto exit;
    }

    *outNumRead = numRead;

    ret = HVB_IO_OK;

exit:

    if (path != NULL) {
        free(path);
    }

    if (fd >= 0) {
        close(fd);
    }
    return ret;
}

static enum hvb_io_errno HvbWriteToPartition(struct hvb_ops* ops,
                                             const char* partition,
                                             int64_t offset, uint64_t numBytes,
                                             const void* buf)
{
    return HVB_IO_OK;
}

static enum hvb_io_errno HvbInvaldateKey(struct hvb_ops* ops,
                                         const uint8_t* publicKeyData,
                                         uint64_t publicKeyLength,
                                         const uint8_t* publicKeyMetadata,
                                         uint64_t publicKeyMetadataLength,
                                         bool* outIsTrusted)
{
    if (outIsTrusted == NULL) {
        return HVB_IO_ERROR_IO;
    }

    *outIsTrusted = true;

    return HVB_IO_OK;
}

static enum hvb_io_errno HvbReadRollbackIdx(struct hvb_ops* ops,
                                            uint64_t rollBackIndexLocation,
                                            uint64_t* outRollbackIndex)
{
    if (outRollbackIndex == NULL) {
        return HVB_IO_ERROR_IO;
    }

    // return 0 as we only need to set up HVB HASHTREE partitions
    *outRollbackIndex = 0;

    return HVB_IO_OK;
}

static enum hvb_io_errno HvbWriteRollbackIdx(struct hvb_ops* ops,
                                             uint64_t rollBackIndexLocation,
                                             uint64_t rollbackIndex)
{
    return HVB_IO_OK;
}

static enum hvb_io_errno HvbReadLockState(struct hvb_ops* ops,
                                          bool* lock_state)
{
    return HVB_IO_OK;
}

static enum hvb_io_errno HvbGetSizeOfPartition(struct hvb_ops* ops,
                                               const char* partition,
                                               uint64_t* size)
{
    if (size == NULL) {
        return HVB_IO_ERROR_IO;
    }

    // The function is for bootloader to load entire content of HVB HASH
    // partitions. In user-space, return 0 as we only need to set up HVB
    // HASHTREE partitions.
    *size = 0;
    return HVB_IO_OK;
}

static struct hvb_ops g_hvb_ops = {
    .user_data = &g_hvb_ops,
    .read_partition = HvbReadFromPartition,
    .write_partition = HvbWriteToPartition,
    .valid_rvt_key = HvbInvaldateKey,
    .read_rollback = HvbReadRollbackIdx,
    .write_rollback = HvbWriteRollbackIdx,
    .read_lock_state = HvbReadLockState,
    .get_partiton_size = HvbGetSizeOfPartition,
};

struct hvb_ops* FsHvbGetOps(void)
{
    return &g_hvb_ops;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
