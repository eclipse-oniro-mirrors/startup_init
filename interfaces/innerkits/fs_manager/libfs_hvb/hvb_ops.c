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

static int64_t GetImageSizeForHVB(const int fd, const char* image)
{
    if (fd < 0) {
        BEGET_LOGE("param is error");
        return -1;
    }

    return lseek64(fd, 0, SEEK_END);
}

static enum hvb_io_errno HvbReadFromPartition(struct hvb_ops* ops,
                                              const char* partition,
                                              int64_t offset, uint64_t numBytes,
                                              void* buf, uint64_t* outNumRead)
{
    int rc;
    int fd = -1;
    char* path = NULL;
    size_t pathLen = 0;
    enum hvb_io_errno ret = HVB_IO_ERROR_IO;

    if (partition == NULL) {
        BEGET_LOGE("error, partition is NULL");
        return HVB_IO_ERROR_IO;
    }

    if (buf == NULL) {
        BEGET_LOGE("error, buf is NULL");
        return HVB_IO_ERROR_IO;
    }

    pathLen = strlen(PARTITION_PATH_PREFIX) + strlen(partition);
    path = calloc(1, pathLen + 1);
    if (path == NULL) {
        BEGET_LOGE("error, calloc fail");
        return HVB_IO_ERROR_OOM;
    }

    rc = snprintf_s(path, pathLen + 1, pathLen, "%s%s", PARTITION_PATH_PREFIX,
                    partition);
    if (rc < 0) {
        BEGET_LOGE("error, snprintf_s fail, ret = %d", rc);
        ret = HVB_IO_ERROR_IO;
        goto exit;
    }

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error, Failed to open %s, errno = %d", path, errno);
        fd = open("/dev/block/by-name/rvt_system", O_RDONLY | O_CLOEXEC);
        BEGET_LOGE("open %s, fd=%d errno = %d", "/dev/block/by-name/rvt_system",
                   fd, errno);
        if (fd >= 0) {
            close(fd);
        }
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

    if (outNumRead != NULL) {
        *outNumRead = numRead;
    }

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
