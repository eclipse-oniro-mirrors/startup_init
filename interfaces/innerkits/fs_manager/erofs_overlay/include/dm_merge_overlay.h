/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef DM_MERGE_OVERLAY_H
#define DM_MERGE_OVERLAY_H

#include "erofs_overlay_common.h"
#include "fs_manager/fs_manager.h"
#include "fs_dm_linear.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define DM_MERGE_NAME "dm_merge"
#define DM_MERGE_MARKER ".dm_merge"
#define DM_MERGE_CLEANUP_MARKER ".dm_merge_cleanup"
#define HYPERHOLD_DEVICE_PATH "/dev/block/by-name/hyperhold"
#define HYPERHOLD_REFRESH_SIZE 128
#define HYPERHOLD_SWITCH_OFFSET 0
#define HYPERHOLD_SWITCH_SIZE 128
#define HYPERHOLD_SWITCH_DISABLE "remount_disable"
#define HYPERHOLD_SWITCH_ENABLE "enable"
#define MAX_OVERLAY_PARTITIONS 10
#define BLKDISCARD_RANGE_SIZE 2

int TryDmMergeOverlay(Fstab *fstab);
bool IsDmMergeOverlayActive(void);
int CheckDmMergeCleanup(void);
int RefreshPartitionOverlay(void);
int MountDmMergeOverlayAll(void);
int CheckHyperholdDisableMarker(void);
bool IsDmMergeRemountEnabled(void);
bool IsHyperholdEnableMarkerSet(void);

const char *OverlayPathFromMnt(const char *mnt);
bool MntNeedRemount(const char *mnt);
bool IsDeviceNameSeen(const char *seenDevices[], uint32_t count, const char *deviceName);
uint64_t AlignDown(uint64_t value, uint64_t alignment);

typedef struct {
    DmLinearTarget *targets;
    char (*mntPaths)[MAX_BUFFER_LEN];
    uint32_t num;
    uint64_t sectorOffset;
    const char *seenDevices[MAX_TARGET_NUM];
    uint32_t seenCount;
} DmMergeCollectCtx;

int CollectDmMergeTargets(Fstab *fstab, DmMergeCollectCtx *ctx);
void DestroyDmMergeTargets(DmLinearTarget *targets, uint32_t targetNum);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
