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

#ifndef STARTUP_LIBFS_DM_SNAPSHOT_H
#define STARTUP_LIBFS_DM_SNAPSHOT_H

#include <fs_dm.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef DmVerityTarget DmSnapshotTarget;
int FsDmCreateSnapshotDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen,
                             DmSnapshotTarget *target);
int FsDmSwitchToSnapshotMerge(const char *devName, DmSnapshotTarget *target);
bool GetDmMergeProcess(const char *name, StatusInfo *processInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_LIBFS_DM_SNAPSHOT_H