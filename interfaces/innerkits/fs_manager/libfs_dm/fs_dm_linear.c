/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include "securec.h"
#include "beget_ext.h"
#include "fs_dm_linear.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

static int LoadDmLinearDeviceTable(int fd, const char *devName, DmLinearTarget *target, uint32_t targetNum)
{
    struct dm_ioctl *io = NULL;
    struct dm_target_spec *ts = NULL;
    size_t parasTotalSize = DM_ALIGN(sizeof(*io) + sizeof(*ts) * targetNum);
    for (uint32_t index = 0; index < targetNum; index++) {
        parasTotalSize += target[index].paras_len + 1;
    }

    char *parasBuf = calloc(1, parasTotalSize);
    BEGET_ERROR_CHECK(parasBuf != NULL, return -1, "error, calloc dm table fail");
    io = (struct dm_ioctl *)parasBuf;
    int rc = InitDmIo(io, devName);
    BEGET_ERROR_CHECK(rc == 0, free(parasBuf); return rc, "error %d, init dm io", rc);
    io->data_size = parasTotalSize;
    io->data_start = sizeof(*io);
    io->target_count = targetNum;
    uint32_t i = 0;
    uint32_t len = 0;
    char *paras = NULL;
    do {
        FS_DM_RETURN_ERR_IF_NULL(target[i].paras);
        ts = (struct dm_target_spec *)(parasBuf + sizeof(*io) + len);
        ts->status = 0;
        ts->sector_start = target[i].start;
        ts->length = target[i].length;

        rc = strcpy_s(ts->target_type, sizeof(ts->target_type), "linear");
        BEGET_ERROR_CHECK(rc == EOK, free(parasBuf); return -1, "error %d, cp target type", rc);
        len += sizeof((*ts));
        paras = parasBuf + sizeof(*io) + len;
        rc = strcpy_s(paras, target[i].paras_len + 1, target[i].paras);
        BEGET_ERROR_CHECK(rc == EOK, free(parasBuf); return -1, "error %d, cp target paras", rc);
        len += target[i].paras_len + 1;
        ts->next = target[i].paras_len + 1 + sizeof((*ts));
        i++;
    } while (i < targetNum);

    rc = ioctl(fd, DM_TABLE_LOAD, io);
    if (rc != 0) {
        BEGET_LOGE("error %d, DM_TABLE_LOAD failed for %s", rc, devName);
    }
    free(parasBuf);
    BEGET_LOGI("LoadDmLinearDeviceTable end, dev is [%s] rc is [%d]", devName, rc);
    return rc;
}

int FsDmCreateMultiTargetLinearDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen,
    DmLinearTarget *target, uint32_t targetNum)
{
    if (devName == NULL || dmDevPath == NULL || target == NULL) {
        BEGET_LOGE("argc is null");
        return -1;
    }
    if (targetNum == 0 || targetNum > MAX_TARGET_NUM) {
        BEGET_LOGE("targetNum:%u is invalid", targetNum);
        return -1;
    }

    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("open mapper path failed errno %d", errno);
        return -1;
    }

    int rc = 0;

    do {
        rc = CreateDmDev(fd, devName);
        if (rc != 0) {
            BEGET_LOGE("create dm device failed %d", rc);
            break;
        }
        rc = DmGetDeviceName(fd, devName, dmDevPath, dmDevPathLen);
        if (rc != 0) {
            BEGET_LOGE("get dm device name failed");
            break;
        }
        rc = LoadDmLinearDeviceTable(fd, devName, target, targetNum);
        if (rc != 0) {
            BEGET_LOGE("load dm device name failed");
            break;
        }

        rc = ActiveDmDevice(fd, devName);
        if (rc != 0) {
            BEGET_LOGE("active dm device name failed");
            break;
        }

        BEGET_LOGI("fs create linear device success, dev is [%s] dmDevPath is [%s]", devName, dmDevPath);
    } while (0);

    close(fd);
    return rc;
}

int FsDmSwitchToLinearDevice(const char *devName, DmLinearTarget *target, uint32_t targetNum)
{
    if (devName == NULL || target == NULL) {
        BEGET_LOGE("argc is null");
        return -1;
    }
    if (targetNum == 0 || targetNum > MAX_TARGET_NUM) {
        BEGET_LOGE("targetNum:%u is invalid", targetNum);
        return -1;
    }

    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("open mapper path failed errno %d", errno);
        return -1;
    }

    int rc = 0;

    do {
        rc = LoadDmLinearDeviceTable(fd, devName, target, targetNum);
        if (rc != 0) {
            BEGET_LOGE("load dm device name failed");
            break;
        }
        rc = ActiveDmDevice(fd, devName);
        if (rc != 0) {
            BEGET_LOGE("active dm device name failed");
            break;
        }

        BEGET_LOGI("fs switch linear device success, dev is [%s]", devName);
    } while (0);

    close(fd);
    return rc;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif