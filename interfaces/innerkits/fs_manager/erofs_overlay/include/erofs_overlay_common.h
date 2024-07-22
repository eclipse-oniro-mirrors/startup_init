/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#ifndef EROFS_OVERLAY_COMMON_H
#define EROFS_OVERLAY_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include "fs_manager/ext4_super_block.h"
#include "fs_manager/erofs_super_block.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MODE_MKDIR 0755
#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_SUPER_OFFSET 0x400
#define SECTOR_SIZE 512
#define PREFIX_LOWER "/mnt/lower"
#define PREFIX_OVERLAY "/mnt/overlay"
#define PREFIX_UPPER "/upper"
#define PREFIX_WORK "/work"

bool IsOverlayEnable(void);

bool CheckIsExt4(const char *dev, uint64_t offset);

bool CheckIsErofs(const char *dev);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif