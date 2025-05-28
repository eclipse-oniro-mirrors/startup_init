/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef STARTUP_LIBFS_DM_H
#define STARTUP_LIBFS_DM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/dm-ioctl.h>
#include <sys/ioctl.h>
#include "fs_manager.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
#define MAX_PATH_LEN 256
#define MAX_TABLE_LEN 4096
#define MAX_WORDS_LEN 20
#define DM_VERSION0 (4)
#define DM_VERSION1 (0)
#define DM_VERSION2 (0)
#define DM_ALIGN_MASK (7)
#define DM_ALIGN(x) (((x) + DM_ALIGN_MASK) & ~DM_ALIGN_MASK)
#define DEVICE_MAPPER_PATH "/dev/mapper/control"
#define DM_DEVICE_PATH_PREFIX "/dev/block/dm-"

#define FS_DM_RETURN_ERR_IF_NULL(__ptr)                   \
    do {                                                  \
        if ((__ptr) == NULL) {                            \
            BEGET_LOGE("error, %s is NULL\n", #__ptr);    \
            return -1;                                    \
        }                                                 \
    } while (0)

typedef struct {
    uint64_t start;
    uint64_t length;
    char *paras;
    uint64_t paras_len;
} DmVerityTarget;

enum DmDeviceType {
    VERIFY,
    LINEAR,
    SNAPSHOT,
    SNAPSHOTMERGE,
    MAXNUMTYPE
};

typedef struct {
    uint64_t sectors_allocated;
    uint64_t total_sectors;
    uint64_t metadata_sectors;
    char error[MAX_WORDS_LEN];
} StatusInfo;

int FsDmInitDmDev(char *devPath, bool useSocket);
int FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target);
int FsDmRemoveDevice(const char *devName);
int FsDmCreateLinearDevice(const char *devName, char *dmBlkName,
                           uint64_t dmBlkNameLen, DmVerityTarget *target);
int LoadDmDeviceTable(int fd, const char *devName, DmVerityTarget *target, int dmType);
int DmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen);
int ActiveDmDevice(int fd, const char *devName);
int InitDmIo(struct dm_ioctl *io, const char *devName);
int CreateDmDev(int fd, const char *devName);
int GetDmDevPath(int fd, char **dmDevPath, const char *devName);

bool GetDmStatusInfo(const char *name, struct dm_ioctl *io);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_LIBFS_DM_H
