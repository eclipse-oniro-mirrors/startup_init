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
#define PREFIX_OVERLAY_MERGE "/mnt/overlay_merge"
#define PREFIX_UPPER "/upper"
#define PREFIX_WORK "/work"
#define BYTE_UNIT 1024
#define ALIGN_BLOCK_SIZE (16 * BYTE_UNIT)
#define MIN_DM_SIZE (500 * BYTE_UNIT)
#define BLOCK_SIZE_UINT 4096
#define EXTHDR_MAGIC 0xFEEDBEEF
#define EXTHDR_BLKSIZE 4096

struct extheader_v1 {
    uint32_t magic_number;
    uint16_t exthdr_size;
    uint16_t bcc16;
    uint64_t part_size;
};

bool IsOverlayEnable(void);

bool CheckIsExt4(const char *dev, uint64_t offset);

bool CheckIsErofs(const char *dev);

uint64_t LookupErofsEnd(const char *dev);

uint64_t GetImgSize(const char *dev, uint64_t offset);

uint64_t GetBlockSize(const char *dev);

uint64_t GetFsSize(int fd);

uint64_t AlignTo(uint64_t base, uint64_t alignment);

int GetMapperAddr(const char *dev, uint64_t *start, uint64_t *length);

int GetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif