/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef STARTUP_FS_MANAGER_H
#define STARTUP_FS_MANAGER_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* Fs manager flags definition */
#define FS_MANAGER_CHECK  0x00000001
#define FS_MANAGER_WAIT  0x00000002
#define FS_MANAGER_REQUIRED  0x00000004
#define FS_MANAGER_NOFAIL  0x00000008
#define FS_MANAGER_HVB  0x00000010
#define NAME_SIZE 32
#define MAX_SLOT 2

#define VALID_FS_MANAGER_FLAGS (FS_MANAGER_CHECK | FS_MANAGER_WAIT | FS_MANAGER_REQUIRED)
#define FS_MANAGER_FLAGS_ENABLED(fsMgrFlags, flag) (((fsMgrFlags) & FS_MANAGER_##flag) != 0)

#define FM_MANAGER_CHECK_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), CHECK)
#define FM_MANAGER_WAIT_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), WAIT)
#define FM_MANAGER_REQUIRED_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), REQUIRED)
#define FM_MANAGER_NOFAIL_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), NOFAIL)

typedef enum MountStatus {
    MOUNT_ERROR = -1,
    MOUNT_UMOUNTED = 0,
    MOUNT_MOUNTED = 1,
} MountStatus;

typedef struct FstabItem {
    char *deviceName;  // Block device name
    char *mountPoint;  // Mount point
    char *fsType;      // File system type
    char *mountOptions;  // File system mount options. readonly, rw, remount etc.
    unsigned int fsManagerFlags;  // flags defined by fs manager.
    struct FstabItem *next;
} FstabItem;

typedef struct {
    struct FstabItem *head;
} Fstab;

typedef enum SlotFlag {
    UNBOOT = 0,
    ACTIVE = 1,
} SlotFlag;

typedef struct SlotInfo {
    int slotName;
    char *slotSuffix;
    SlotFlag slotFlag;
    unsigned int retryCount;
    unsigned int reserved;
} SlotInfo;

Fstab* LoadFstabFromCommandLine(void);
int GetBootSlots(void);
int GetCurrentSlot(void);
void ReleaseFstab(Fstab *fstab);
Fstab *ReadFstabFromFile(const char *file, bool procMounts);
FstabItem *FindFstabItemForPath(Fstab fstab, const char *path);
FstabItem* FindFstabItemForMountPoint(Fstab fstab, const char *mp);
int ParseFstabPerLine(char *str, Fstab *fstab, bool procMounts, const char *separator);

int GetBlockDeviceByMountPoint(const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen);
int GetBlockDeviceByName(const char *deviceName, const Fstab *fstab, char* miscDev, size_t size);
bool IsSupportedFilesystem(const char *fsType);
int DoFormat(const char *devPath, const char *fsType);
int MountOneItem(FstabItem *item);
MountStatus GetMountStatusForMountPoint(const char *mp);
int MountAllWithFstabFile(const char *fstabFile, bool required);
int MountAllWithFstab(const Fstab *fstab, bool required);
int UmountAllWithFstabFile(const char *file);
unsigned long GetMountFlags(char *mountFlag, char *fsSpecificFlags, size_t fsSpecificFlagSize,
    const char *mountPoint);

int GetBlockDevicePath(const char *partName, char *path, size_t size);

// Get fscrypt policy if exist
int LoadFscryptPolicy(char *buf, size_t size);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_FS_MANAGER_H
