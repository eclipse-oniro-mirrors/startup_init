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

#ifndef _EROFS_FS_H
#define _EROFS_FS_H

#define EROFS_SUPER_MAGIC  0xE0F5E1E2

typedef struct __packed erofs_super_block {
    __le32 magic;
    __le32 checksum;
    __le32 feature_compat;
    __u8 blkszbits;
    __u8 reserved;

    __le16 root_nid;
    __le64 inos;

    __le64 build_time;
    __le32 build_time_nsec;
    __le32 blocks;
    __le32 meta_blkaddr;
    __le32 xattr_blkaddr;
    __u8 uuid[16];
    __u8 volume_name[16];
    __le32 feature_incompat;
    __u8 reserved2[44];
} erofs_super_block;

#endif