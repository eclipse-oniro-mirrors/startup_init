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

#include <fs_dm.h>
#include "securec.h"
#include "beget_ext.h"
#include <linux/dm-ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "ueventd.h"
#include "ueventd_socket.h"
#include <limits.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

const char g_dmType[MAXNUMTYPE][MAX_WORDS_LEN] = {"verity", "linear", "snapshot", "snapshot-merge"};

int InitDmIo(struct dm_ioctl *io, const char *devName)
{
    errno_t err;

    err = memset_s(io, sizeof(*io), 0, sizeof(*io));
    if (err != EOK) {
        BEGET_LOGE("memset io, ret=%d", err);
        return -1;
    }

    io->version[0] = DM_VERSION0;
    io->version[1] = DM_VERSION1;
    io->version[2] = DM_VERSION2;
    io->data_size = sizeof(*io);
    io->data_start = 0;

    if (devName == NULL) {
        return 0;
    }

    err = strcpy_s(io->name, sizeof(io->name), devName);
    if (err != EOK) {
        BEGET_LOGE("cp devName, ret=%d", err);
        return -1;
    }

    return 0;
}

int CreateDmDev(int fd, const char *devName)
{
    int rc;
    struct dm_ioctl io;

    rc = InitDmIo(&io, devName);
    if (rc != 0) {
        return rc;
    }

    rc = ioctl(fd, DM_DEV_CREATE, &io);
    if (rc != 0) {
        BEGET_LOGE("error, DM_DEV_CREATE failed for %s, ret=%d", devName, rc);
        return rc;
    }

    return 0;
}

int LoadDmDeviceTable(int fd, const char *devName, DmVerityTarget *target, int dmTypeIdx)
{
    int rc;
    errno_t err;
    char *parasBuf = NULL;
    size_t parasTotalSize;
    struct dm_ioctl *io = NULL;
    struct dm_target_spec *ts = NULL;
    char *paras = NULL;

    FS_DM_RETURN_ERR_IF_NULL(target);
    FS_DM_RETURN_ERR_IF_NULL(target->paras);
    if (dmTypeIdx >= MAXNUMTYPE || dmTypeIdx < 0) {
        BEGET_LOGE("dmTypeIdx error dmTypeIdx %d", dmTypeIdx);
        return -1;
    }

    parasTotalSize = DM_ALIGN(sizeof(*io) + sizeof(*ts) + target->paras_len + 1);
    parasBuf = calloc(1, parasTotalSize);
    if (parasBuf == NULL) {
        BEGET_LOGE("error, calloc dm table fail");
        return -1;
    }

    io = (struct dm_ioctl *)parasBuf;
    ts = (struct dm_target_spec *)(parasBuf + sizeof(*io));
    paras = parasBuf + sizeof(*io) + sizeof((*ts));

    do {
        rc = InitDmIo(io, devName);
        if (rc != 0) {
            BEGET_LOGE("error 0x%x, init dm io", rc);
            break;
        }

        io->data_size = parasTotalSize;
        io->data_start = sizeof(*io);
        io->target_count = 1;
        if (dmTypeIdx != SNAPSHOT) {
            io->flags |= DM_READONLY_FLAG;
        }

        ts->status = 0;
        ts->sector_start = target->start;
        ts->length = target->length;
        err = strcpy_s(ts->target_type, sizeof(ts->target_type), g_dmType[dmTypeIdx]);
        if (err != EOK) {
            rc = -1;
            BEGET_LOGE("error 0x%x, cp target type", err);
            break;
        }
        BEGET_LOGI("get target type is %s", ts->target_type);
        err = strcpy_s(paras, target->paras_len + 1, target->paras);
        if (err != EOK) {
            rc = -1;
            BEGET_LOGE("error 0x%x, cp target paras", err);
            break;
        }

        ts->next = parasTotalSize - sizeof(*io);

        rc = ioctl(fd, DM_TABLE_LOAD, io);
        if (rc != 0) {
            BEGET_LOGE("error 0x%x, DM_TABLE_LOAD failed for %s", rc, devName);
            break;
        }
    } while (0);

    free(parasBuf);

    return rc;
}

int ActiveDmDevice(int fd, const char *devName)
{
    int rc;
    struct dm_ioctl io;

    rc = InitDmIo(&io, devName);
    if (rc != 0) {
        return rc;
    }

    io.flags |= DM_READONLY_FLAG;

    rc = ioctl(fd, DM_DEV_SUSPEND, &io);
    if (rc != 0) {
        BEGET_LOGE("error, DM_TABLE_SUSPEND for %s, ret=%d", devName, rc);
        return rc;
    }

    return 0;
}

int GetDmDevPath(int fd, char **dmDevPath, const char *devName)
{
    int rc;
    char *path = NULL;
    size_t path_len = strlen(DM_DEVICE_PATH_PREFIX) + 32;
    unsigned int dev_num;
    struct dm_ioctl io;

    rc = InitDmIo(&io, devName);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, init Dm io", rc);
        return rc;
    }

    rc = ioctl(fd, DM_DEV_STATUS, &io);
    if (rc != 0) {
        BEGET_LOGE("error, DM_DEV_STATUS for %s, ret=%d", devName, rc);
        return rc;
    }

    dev_num = minor(io.dev);

    path = calloc(1, path_len);
    if (path == NULL) {
        BEGET_LOGE("error, alloc dm dev path");
        return -1;
    }

    rc = snprintf_s(path, path_len, path_len - 1, "%s%u", DM_DEVICE_PATH_PREFIX, dev_num);
    if (rc < 0) {
        BEGET_LOGE("error 0x%x, cp dm dev path", rc);
        free(path);
        return -1;
    }

    *dmDevPath = path;

    return 0;
}

int FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target)
{
    int rc;
    int fd = -1;

    fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error 0x%x, open %s", errno, DEVICE_MAPPER_PATH);
        return -1;
    }

    rc = CreateDmDev(fd, devName);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, create dm device fail", rc);
        close(fd);
        return rc;
    }

    rc = LoadDmDeviceTable(fd, devName, target, VERIFY);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, load device table fail", rc);
        close(fd);
        return rc;
    }

    rc = ActiveDmDevice(fd, devName);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, active device fail", rc);
        close(fd);
        return rc;
    }

    rc = GetDmDevPath(fd, dmDevPath, devName);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, get dm dev fail", rc);
        close(fd);
        return rc;
    }
    close(fd);
    return 0;
}

int FsDmInitDmDev(char *devPath, bool useSocket)
{
    int rc;
    char dmDevPath[PATH_MAX] = {0};
    char *devName = NULL;

    if (devPath == NULL) {
        BEGET_LOGE("error, devPath is NULL");
        return -1;
    }

    devName = basename(devPath);

    rc = snprintf_s(&dmDevPath[0], sizeof(dmDevPath), sizeof(dmDevPath) - 1, "%s%s", "/sys/block/", devName);
    if (rc < 0) {
        BEGET_LOGE("error 0x%x, format dm dev", rc);
        return rc;
    }

    int ueventSockFd = -1;
    if (useSocket) {
        ueventSockFd = UeventdSocketInit();
        if (ueventSockFd < 0) {
            BEGET_LOGE("error, Failed to create uevent socket");
            return -1;
        }
    }
    char **devices = (char **)calloc(1, sizeof(char *));
    if (devices == NULL) {
        BEGET_LOGE("Failed calloc err=%d", errno);
        return -1;
    }

    devices[0] = strdup(devPath);
    if (devices[0] == NULL) {
        BEGET_LOGE("Failed to strdup devPath");
        free(devices);
        return -1;
    }
    BEGET_LOGI("FsDmInitDmDev dmDevPath %s devices %s", &dmDevPath[0], devices[0]);
    RetriggerDmUeventByPath(ueventSockFd, &dmDevPath[0], devices, 1);
    free(devices[0]);
    free(devices);

    close(ueventSockFd);

    return 0;
}

int FsDmRemoveDevice(const char *devName)
{
    int rc;
    int fd = -1;
    struct dm_ioctl io;

    fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error 0x%x, open %s", errno, DEVICE_MAPPER_PATH);
        return -1;
    }

    rc = InitDmIo(&io, devName);
    if (rc != 0) {
        close(fd);
        BEGET_LOGE("error 0x%x, init dm io", rc);
        return -1;
    }

    io.flags |= DM_READONLY_FLAG;

    rc = ioctl(fd, DM_DEV_REMOVE, &io);
    if (rc != 0) {
        close(fd);
        BEGET_LOGE("error, DM_DEV_REMOVE failed for %s, ret=%d", devName, rc);
        return -1;
    }

    close(fd);
    return 0;
}

int DmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen)
{
    int rc;
    char *path = NULL;
    size_t pathLen = strlen(DM_DEVICE_PATH_PREFIX) + 32;
    struct dm_ioctl io;
    rc = InitDmIo(&io, devName);
    if (rc != 0) {
        BEGET_LOGE("init dm io failed");
        return rc;
    }

    rc = ioctl(fd, DM_DEV_STATUS, &io);
    if (rc != 0) {
        BEGET_LOGE("get  DM_DEV_STATUS failed");
        return rc;
    }

    int devNum = (io.dev & 0xff) | ((io.dev >> 12) & 0xff00);
    path = calloc(1, pathLen);
    if (path == NULL) {
        BEGET_LOGE("calloc path failed");
        return rc;
    }

    rc = snprintf_s(path, pathLen, pathLen - 1, "%s%d", DM_DEVICE_PATH_PREFIX, devNum);
    if (rc < 0) {
        BEGET_LOGE("copy dm path failed");
        free(path);
        path = NULL;
        return rc;
    }

    rc = memcpy_s(outDevName, outDevNameLen, path, pathLen);
    if (rc < 0) {
        BEGET_LOGE("copy dm path failed");
    }
    BEGET_LOGI("dm get device outDevName %s, devNum %d", path, devNum);
    free(path);
    path = NULL;
    return rc;
}

int FsDmCreateLinearDevice(const char *devName, char *dmBlkName, uint64_t dmBlkNameLen, DmVerityTarget *target)
{
    int rc;
    int fd = -1;
    fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("open mapper path failed");
        return -1;
    }

    rc = CreateDmDev(fd, devName);
    if (rc) {
        BEGET_LOGE("create dm device failed");
        close(fd);
        return -1;
    }

    rc = DmGetDeviceName(fd, devName, dmBlkName, dmBlkNameLen);
    if (rc) {
        BEGET_LOGE("get dm device name failed");
        close(fd);
        return -1;
    }

    rc = LoadDmDeviceTable(fd, devName, target, LINEAR);
    if (rc) {
        BEGET_LOGE("load dm device name failed");
        close(fd);
        return -1;
    }

    rc = ActiveDmDevice(fd, devName);
    if (rc) {
        BEGET_LOGE("active dm device name failed");
        close(fd);
        return -1;
    }
    close(fd);
    BEGET_LOGI("fs create rofs linear device success, dev is [%s]", devName);
    return 0;
}

bool GetDmStatusInfo(const char *name, struct dm_ioctl *io)
{
    int fd = open(DEVICE_MAPPER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        BEGET_LOGE("error 0x%x, open %s", errno, DEVICE_MAPPER_PATH);
        return false;
    }
    int rc = InitDmIo(io, name);
    if (rc != 0) {
        close(fd);
        return false;
    }
    if (ioctl(fd, DM_DEV_STATUS, io) != 0) {
        BEGET_LOGE("DM_DEV_STATUS failed for %s\n", name);
        close(fd);
        return false;
    }
    close(fd);
    return true;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
