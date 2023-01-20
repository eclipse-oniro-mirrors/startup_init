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
#include "fs_manager/fs_manager.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct {
    uint64_t start;
    uint64_t length;
    char *paras;
    uint64_t paras_len;
} DmVerityTarget;

int FsDmInitDmDev(char *devPath);
int FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_LIBFS_DM_H
