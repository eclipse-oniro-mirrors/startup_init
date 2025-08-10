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
#include "fs_dm_snapshot.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int FsDmCreateSnapshotDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen, DmSnapshotTarget *target)
{
    if (devName == NULL || dmDevPath == NULL || target == NULL) {
        BEGET_LOGE("argc is null");
        return -1;
    }
    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error %d, open %s", errno, DEVICE_MAPPER_PATH);
        return -1;
    }
    int rc = 0;
    do {
        rc = CreateDmDev(fd, devName);
        if (rc != 0) {
            BEGET_LOGE("error %d, create dm device fail", rc);
            break;
        }
        rc = LoadDmDeviceTable(fd, devName, target, SNAPSHOT);
        if (rc != 0) {
            BEGET_LOGE("error %d, load device table fail", rc);
            break;
        }
        rc = ActiveDmDevice(fd, devName);
        if (rc != 0) {
            BEGET_LOGE("error %d, active device fail", rc);
            break;
        }

        rc = DmGetDeviceName(fd, devName, dmDevPath, dmDevPathLen);
        if (rc != 0) {
            BEGET_LOGE("get dm device name failed");
            break;
        }

        BEGET_LOGI("fs create snapshot device success, dev is [%s] dmDevPath is [%s]", devName, dmDevPath);
    } while (0);
    close(fd);
    return rc;
}

int FsDmSwitchToSnapshotMerge(const char *devName, DmSnapshotTarget *target)
{
    if (devName == NULL || target == NULL) {
        BEGET_LOGE("argc is null");
        return -1;
    }
    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error %d, open %s", errno, DEVICE_MAPPER_PATH);
        return -1;
    }
    int rc = 0;
    do {
        rc = LoadDmDeviceTable(fd, devName, target, SNAPSHOTMERGE);
        if (rc != 0) {
            BEGET_LOGE("error %d, load device table fail", rc);
            break;
        }
        rc = ActiveDmDevice(fd, devName);
        if (rc != 0) {
            BEGET_LOGE("error %d, active device fail", rc);
            break;
        }
        BEGET_LOGI("fs switch snapshot merge success, dev is %s", devName);
    } while (0);
    close(fd);
    return rc;
}

static bool ParseStatusText(char *data, StatusInfo *processInfo)
{
    BEGET_LOGI("ParseStatusText start \"%s\"", data);
    int args = sscanf_s(data, "%lu %lu %lu", &processInfo->sectors_allocated,
        &processInfo->total_sectors, &processInfo->metadata_sectors);
    if (args == 3) { // 3 is parameters
        BEGET_LOGI("ParseStatusText success. sectors_allocated %d total_sectors %d metadata_sectors %d",
            processInfo->sectors_allocated, processInfo->total_sectors, processInfo->metadata_sectors);
        return true;
    }
    if (strlen(data) >= sizeof(processInfo->error)) {
        BEGET_LOGE("data len err, data is = %s", data);
        return false;
    }
    if (strcpy_s(processInfo->error, sizeof(processInfo->error), data) != EOK) {
        BEGET_LOGE("strcpy_s err, errno %d", errno);
        return false;
    }
    if (strcmp(processInfo->error, "Invalid") == 0 ||
        strcmp(processInfo->error, "Overflow") == 0 ||
        strcmp(processInfo->error, "Merge failed") == 0 ||
        strcmp(processInfo->error, "Unknown") == 0 ||
        strcmp(processInfo->error, "") == 0) {
        BEGET_LOGW("processInfo->error is \"%s\"", processInfo->error);
        return true;
    }
    BEGET_LOGE("could not parse snapshot processInfo \"%s\": wrong format", processInfo->error);
    return false;
}

bool GetDmSnapshotStatus(const char *name, const char *targetType, StatusInfo *processInfo)
{
    if (name == NULL || targetType == NULL || processInfo == NULL) {
        BEGET_LOGE("argc is null");
        return false;
    }
    BEGET_LOGI("GetDmSnapshotStatus start, name %s, targetType %s", name, targetType);
    size_t bufferLen = MAX_TABLE_LEN * sizeof(char);
    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    BEGET_ERROR_CHECK(fd >= 0, return false, "open error %d", errno);
    char *buffer = (char *)calloc(1, bufferLen);
    BEGET_ERROR_CHECK(buffer != NULL, close(fd); return false, "calloc buffer err");
    struct dm_ioctl *io = NULL;
    bool ret = false;
    do {
        io = (struct dm_ioctl *)buffer;
        int rc = InitDmIo(io, name);
        BEGET_ERROR_CHECK(rc == 0, break, "error %d, init Dm io", rc);
        io->data_size = bufferLen;
        io->data_start = sizeof(*io);
        io->flags = 0;
        if (ioctl(fd, DM_TABLE_STATUS, io) != 0) {
            BEGET_LOGE("DM_TABLE_STATUS failed for %s", name);
            break;
        }
        if (io->flags & DM_BUFFER_FULL_FLAG) {
            BEGET_LOGE("io flags err");
            break;
        }
        size_t cursor = io->data_start;
        struct dm_target_spec* spec = (struct dm_target_spec*)(&buffer[cursor]);
        if (cursor + sizeof(struct dm_target_spec) > io->data_size) {
            BEGET_LOGE("len err");
            break;
        }
        size_t dataOffset = cursor + sizeof(struct dm_target_spec);
        if ((size_t)spec->next <= sizeof(struct dm_target_spec)) {
            BEGET_LOGE("spec->next = %u, len err, dataOffset = %zu", spec->next, dataOffset);
            break;
        }
        size_t dataLen = (size_t)spec->next - sizeof(struct dm_target_spec);
        char *data = (char *)calloc(1, dataLen + 1);
        BEGET_ERROR_CHECK(data != NULL, break, "calloc dataLen = %zu err", dataLen);
        rc = strncpy_s(data, dataLen, buffer + dataOffset, dataLen);
        BEGET_ERROR_CHECK(rc == EOK, free(data); break, "strncpy_s err");
        if (strcmp(spec->target_type, targetType) != 0 || !ParseStatusText(data, processInfo)) {
            BEGET_LOGE("get snapshot status fail");
            free(data);
            break;
        }
        free(data);
        ret = true;
    } while (0);
    BEGET_LOGI("GetDmSnapshotStatus end. ret %d", ret);
    close(fd);
    free(buffer);
    return ret;
}

bool GetDmMergeProcess(const char *name, StatusInfo *processInfo)
{
    return GetDmSnapshotStatus(name, "snapshot-merge", processInfo);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif