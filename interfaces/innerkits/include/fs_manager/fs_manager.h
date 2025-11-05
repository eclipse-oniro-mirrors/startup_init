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

#ifndef STARTUP_INIT_TEST
#ifndef INIT_STATIC
#define INIT_STATIC static
#endif
#else
#ifndef INIT_STATIC
#define INIT_STATIC
#endif
#endif

/* Fs manager flags definition */
#define FS_MANAGER_CHECK  0x00000001
#define FS_MANAGER_WAIT  0x00000002
#define FS_MANAGER_REQUIRED  0x00000004
#define FS_MANAGER_NOFAIL  0x00000008
#define FS_MANAGER_HVB  0x00000010
#define FS_MANAGER_PROJQUOTA  0x00000020
#define FS_MANAGER_CASEFOLD  0x00000040
#define FS_MANAGER_COMPRESSION  0x00000080
#define FS_MANAGER_DEDUP  0x00000100
#define FS_MANAGER_FORMATTABLE  0x00000200
#define NAME_SIZE 32
#define MAX_SLOT 2
#define HVB_DEV_NAME_SIZE 64
#define EXTHDR_MAGIC 0xFEEDBEEF

#define VALID_FS_MANAGER_FLAGS (FS_MANAGER_CHECK | FS_MANAGER_WAIT | FS_MANAGER_REQUIRED)
#define FS_MANAGER_FLAGS_ENABLED(fsMgrFlags, flag) (((fsMgrFlags) & FS_MANAGER_##flag) != 0)

#define FM_MANAGER_CHECK_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), CHECK)
#define FM_MANAGER_WAIT_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), WAIT)
#define FM_MANAGER_REQUIRED_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), REQUIRED)
#define FM_MANAGER_NOFAIL_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), NOFAIL)
#define FM_MANAGER_FORMATTABLE_ENABLED(fsMgrFlags) FS_MANAGER_FLAGS_ENABLED((fsMgrFlags), FORMATTABLE)

typedef enum MountStatus {
    MOUNT_ERROR = -1,
    MOUNT_UMOUNTED = 0,
    MOUNT_MOUNTED = 1,
} MountStatus;

typedef enum SlotStatus {
    SLOTSTATUS_INIT = 0,
    SLOTSTATUS_VAB,
    SLOTSTATUS_OTHER,
} SlotStatus;

typedef struct VabMountInfo {
    char *deviceName;
    char *fsType;
    int result;
    int checkpointMountCounter;
} VabMountInfo;

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
    struct FstabItem *tail;
} Fstab;

typedef struct {
    char partName[HVB_DEV_NAME_SIZE];
    int reverse; // 0 :system->dm0  1:dm0->system
    char value[HVB_DEV_NAME_SIZE];
    int len;
} HvbDeviceParam;

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

typedef struct MountResult {
    // mount result code
    int rc;
    int checkpointMountCounter;
} MountResult;

struct extheader_v1 {
    uint32_t magic_number;
    uint16_t exthdr_size;
    uint16_t bcc16;
    uint64_t part_size;
    bool remount_enable;
};

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
int MountOneWithFstabFile(const char *fstabFile, const char *devName, bool required);
int FsManagerDmRemoveDevice(const char *devName);
unsigned long GetMountFlags(char *mountFlag, char *fsSpecificFlags, size_t fsSpecificFlagSize,
    const char *mountPoint);

int GetBlockDevicePath(const char *partName, char *path, size_t size);

uint64_t LookupErofsEnd(const char *dev);
void SetRemountFlag(bool flag);
bool GetRemountFlag(void);

// Get fscrypt policy if exist
int LoadFscryptPolicy(char *buf, size_t size);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_FS_MANAGER_H
