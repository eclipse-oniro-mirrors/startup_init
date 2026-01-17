/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "lib_fs_dm_snapshot_mock.h"

using namespace OHOS::AppSpawn;
int MockFsdmswitchtosnapshotmerge(const char *devName, DmSnapshotTarget *target)
{
    return FsDmSnapshot::fsDmSnapshotFunc->FsDmSwitchToSnapshotMerge(const char *devName, DmSnapshotTarget *target);
}

bool MockGetdmmergeprocess(const char *name, StatusInfo *processInfo)
{
    return FsDmSnapshot::fsDmSnapshotFunc->GetDmMergeProcess(const char *name, StatusInfo *processInfo);
}

bool MockGetdmsnapshotstatus(const char *name, const char *targetType, StatusInfo *processInfo)
{
    return FsDmSnapshot::fsDmSnapshotFunc->GetDmSnapshotStatus(const char *name, const char *targetType, StatusInfo *processInfo);
}

