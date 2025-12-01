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
#include <hvb_cmdline.h>
#include <sys/stat.h>
#include <unistd.h>

#include "beget_ext.h"
#include "fs_dm.h"
#include "fs_hvb.h"
#include "securec.h"
#include "init_utils.h"
#include "hookmgr.h"
#include "bootstage.h"
#include "fs_hvb_image_patch.h"
#include "fs_manager.h"
#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARTITION_PATH_PREFIX "/dev/block/by-name/"
#define SNAPSHOT_PATH_PREFIX "/dev/block/"
#define MAX_EXT_HVB_PARTITION_NUM 1
#define FS_HVB_LOCK_STATE_STR_MAX 32

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

static char *HvbGetABPartitionPath(const size_t pathLen, const char *partition)
{
    /* Check if there are multiple partitions */
    char *path = NULL;
    int bootSlots = GetBootSlots();
    if (bootSlots <= 1) {
        BEGET_LOGE("invalid bootSlots: %d", bootSlots);
        goto err;
    }

    /* Confirm partition information */
    /* slot = 1 is partition a */
    /* slot = 2 is partition b */
    int slot = GetCurrentSlot();
    if (slot <= 0 || slot > MAX_SLOT) {
        BEGET_LOGW("invalid slot [%d], defaulting to 1", slot);
        slot = 1;
    }

    size_t abPathLen = pathLen + FS_HVB_AB_SUFFIX_LEN;
    path = calloc(1, abPathLen + 1);
    if (path == NULL) {
        BEGET_LOGE("error, calloc fail");
        goto err;
    }

    /* Concatenate partition names */
    const char suffix = 'a' + slot - 1;
    int rc = snprintf_s(path, abPathLen + 1, abPathLen, "%s%s_%c",
                        PARTITION_PATH_PREFIX, partition, suffix);
    if (rc < 0) {
        BEGET_LOGE("snprintf_s fail, ret = %d", rc);
        goto err;
    }
    if (access(path, F_OK) != 0) {
        BEGET_LOGE("still can not access %s, abort verify", path);
        goto err;
    }

    BEGET_LOGW("AB path generated: %s", path);
    return path;

err:
    if (path != NULL) {
        free(path);
    }

    return NULL;
}

static int HvbGetSnapshotPath(const char *partition, char *out, size_t outLen)
{
    int rc;
    HvbDeviceParam devPara = {};
    HOOK_EXEC_OPTIONS options = {};
    if (memcpy_s(devPara.partName, sizeof(devPara.partName), partition, strlen(partition)) != 0) {
        BEGET_LOGE("error, fail to copy partition");
        return HVB_IO_ERROR_IO;
    }
    devPara.reverse = 0;
    devPara.len = (int)sizeof(devPara.value);
    options.flags = TRAVERSE_STOP_WHEN_ERROR;
    options.preHook = NULL;
    options.postHook = NULL;
 
    rc = HookMgrExecute(GetBootStageHookMgr(), INIT_VAB_HVBCHECK, (void *)&devPara, &options);
    BEGET_LOGW("check snapshot path for partition %s, ret = %d", partition, rc);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, trans vab for %s", rc, partition);
        return rc;
    }
    BEGET_LOGW("snapshot path exists for partition %s", partition);
    rc = snprintf_s(out, outLen, outLen - 1, "%s%s", SNAPSHOT_PATH_PREFIX,
                    devPara.value);
    if (rc < 0) {
        BEGET_LOGE("error, snprintf_s snapshot path fail, ret = %d", rc);
        return rc;
    }
    return 0;
}

static int HvbGetImagePatchSnapshotPath(const char *partition, char *out, size_t outLen)
{
    if (QuickfixIsImagePatch(partition, QUICKFIX_COMPARE_SNAPSHOT_NAME) != 0) {
        BEGET_LOGW("not quickfix scene");
        return -1;
    }

    BEGET_LOGI("snapshot patch for image patch exists for %s", partition);
    int rc = snprintf_s(out, outLen, outLen - 1, "%s%s", SNAPSHOT_PATH_PREFIX, partition);
    if (rc < 0) {
        BEGET_LOGE("error, snprintf_s snapshot path fail, ret = %d", rc);
        return -1;
    }
    return 0;
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

    pathLen = strlen(PARTITION_PATH_PREFIX) + HVB_MAX_PARTITION_NAME_LEN - 1;
    path = calloc(1, pathLen + 1);
    if (path == NULL) {
        BEGET_LOGE("error, calloc fail");
        return NULL;
    }

    if (HvbGetSnapshotPath(partition, path, pathLen) == 0) {
        return path;
    } else if (HvbGetImagePatchSnapshotPath(partition, path, pathLen) == 0) {
        return path;
    }

    rc = snprintf_s(path, pathLen + 1, pathLen, "%s%s", PARTITION_PATH_PREFIX,
                    partition);
    if (rc < 0) {
        BEGET_LOGE("error, snprintf_s fail, ret = %d", rc);
        free(path);
        return NULL;
    }

    if (access(path, F_OK) != 0) {
        BEGET_LOGW("Adapting to AB partition");
        free(path);
        return HvbGetABPartitionPath(pathLen, partition);
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

static int FsHvbGetLockStateStr(char *str, size_t size)
{
    return FsHvbGetValueFromCmdLine(str, size, HVB_CMDLINE_DEV_STATE);
}

static enum hvb_io_errno HvbReadLockState(struct hvb_ops *ops,
                                          bool *locked)
{
    char lockState[FS_HVB_LOCK_STATE_STR_MAX] = {0};
    if (ops == NULL || locked == NULL) {
        BEGET_LOGE("Invalid lock state parameter");
        return HVB_IO_ERROR_IO;
    }

    int ret = FsHvbGetLockStateStr(&lockState[0], sizeof(lockState));
    if (ret != 0) {
        BEGET_LOGE("error 0x%x, get lock state from cmdline", ret);
        return HVB_IO_ERROR_NO_SUCH_VALUE;
    }

    if (strncmp(&lockState[0], "locked", FS_HVB_LOCK_STATE_STR_MAX) == 0) {
        *locked = true;
    } else if (strncmp(&lockState[0], "unlocked", FS_HVB_LOCK_STATE_STR_MAX) == 0) {
        *locked = false;
    } else {
        BEGET_LOGE("Invalid lock state");
        return HVB_IO_ERROR_NO_SUCH_VALUE;
    }

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
