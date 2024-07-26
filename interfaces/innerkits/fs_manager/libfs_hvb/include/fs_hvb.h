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

#ifndef STARTUP_LIBFS_HVB_H
#define STARTUP_LIBFS_HVB_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <hvb.h>
#include <hvb_cert.h>
#include "fs_dm.h"
#include "fs_manager.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define USE_FEC_FROM_DEVICE "use_fec_from_device"
#define FEC_ROOTS "fec_roots"
#define FEC_BLOCKS "fec_blocks"
#define FEC_START "fec_start"

int FsHvbInit(void);
int FsHvbSetupHashtree(FstabItem *fsItem);
int FsHvbFinal(void);
struct hvb_ops *FsHvbGetOps(void);
int FsHvbGetValueFromCmdLine(char *val, size_t size, const char *key);
int FsHvbConstructVerityTarget(DmVerityTarget *target, const char *devName, struct hvb_cert *cert);
void FsHvbDestoryVerityTarget(DmVerityTarget *target);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_LIBFS_HVB_H
