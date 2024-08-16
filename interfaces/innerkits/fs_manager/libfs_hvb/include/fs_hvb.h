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
#include "ext4_super_block.h"
#include "erofs_super_block.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define USE_FEC_FROM_DEVICE "use_fec_from_device"
#define FEC_ROOTS "fec_roots"
#define FEC_BLOCKS "fec_blocks"
#define FEC_START "fec_start"
#define BLOCK_SIZE_UINT 4096
#define EXTHDR_MAGIC    0xFEEDBEEF
#define EXTHDR_BLKSIZ   4096
#define SZ_1KB (0x1 << 10)
#define SZ_1MB (0x1 << 20)
#define SZ_1GB (0x1 << 30)
 
#define SZ_2KB (2 * SZ_1KB)
#define SZ_4KB (4 * SZ_1KB)
 
struct extheader_v1 {
    uint32_t magic_number;
    uint16_t exthdr_size;
    uint16_t bcc16;
    uint64_t part_size;
};

int FsHvbInit(void);
int FsHvbSetupHashtree(FstabItem *fsItem);
int FsHvbFinal(void);
struct hvb_ops *FsHvbGetOps(void);
int FsHvbGetValueFromCmdLine(char *val, size_t size, const char *key);
int FsHvbConstructVerityTarget(DmVerityTarget *target, const char *devName, struct hvb_cert *cert);
void FsHvbDestoryVerityTarget(DmVerityTarget *target);

bool CheckAndGetExt4Size(const char *headerBuf, uint64_t *imageSize, const char* image);
bool CheckAndGetErofsSize(const char *headerBuf, uint64_t *imageSize, const char* image);
bool CheckAndGetExtheaderSize(const int fd, uint64_t offset, uint64_t *imageSize, const char* image);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_LIBFS_HVB_H
