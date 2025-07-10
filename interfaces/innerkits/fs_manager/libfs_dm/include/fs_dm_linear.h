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

#ifndef STARTUP_LIBFS_DM_LINEAR_H
#define STARTUP_LIBFS_DM_LINEAR_H

#include <fs_dm.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_TARGET_NUM 8192
typedef DmVerityTarget DmLinearTarget;
int FsDmCreateMultiTargetLinearDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen,
    DmLinearTarget *target, uint32_t targetNum);
int FsDmSwitchToLinearDevice(const char *devName, DmLinearTarget *target, uint32_t targetNum);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_LIBFS_DM_LINEAR_H