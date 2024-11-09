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

#ifndef EXT4_H
#define EXT4_H

#include <linux/types.h>

#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_SUPER_BLOCK_START_POSITION 0x400
#define BLOCK_SIZE_UNIT 4096

typedef struct ext4_super_block {
__le32	s_inodes_count;
__le32	s_blocks_count_lo;
__le32	s_r_blocks_count_lo;
__le32	s_free_blocks_count_lo;
__le32	s_free_inodes_count;
__le32	s_first_data_block;
__le32	s_log_block_size;
__le32	s_log_cluster_size;
__le32	s_blocks_per_group;
__le32	s_clusters_per_group;
__le32	s_inodes_per_group;
__le32	s_mtime;
__le32	s_wtime;
__le16	s_mnt_count;
__le16	s_max_mnt_count;
__le16	s_magic;
__le16	s_state;
__le16	s_errors;
__le16	s_minor_rev_level;
__le32	s_lastcheck;
__le32	s_checkinterval;
__le32	s_creator_os;
__le32	s_rev_level;
__le16	s_def_resuid;
__le16	s_def_resgid;

__le32	s_first_ino;
__le16	s_inode_size;
__le16	s_block_group_nr;
__le32	s_feature_compat;
__le32	s_feature_incompat;
__le32	s_feature_ro_compat;
__u8	s_uuid[16];
char	s_volume_name[16];
char	s_last_mounted[64];
__le32	s_algorithm_usage_bitmap;

__u8	s_prealloc_blocks;
__u8	s_prealloc_dir_blocks;
__le16	s_reserved_gdt_blocks;

__u8	s_journal_uuid[16];
__le32	s_journal_inum;
__le32	s_journal_dev;
__le32	s_last_orphan;
__le32	s_hash_seed[4];
__u8	s_def_hash_version;
__u8	s_jnl_backup_type;
__le16	s_desc_size;
__le32	s_default_mount_opts;
__le32	s_first_meta_bg;
__le32	s_mkfs_time;
__le32	s_jnl_blocks[17];

__le32	s_blocks_count_hi;
__le32	s_r_blocks_count_hi;
__le32	s_free_blocks_count_hi;
__le16	s_min_extra_isize;
__le16	s_want_extra_isize;
__le32	s_flags;
__le16	s_raid_stride;
__le16	s_mmp_interval;
__le64	s_mmp_block;
__le32	s_raid_stripe_width;
__u8	s_log_groups_per_flex;
__u8	s_checksum_type;
__le16	s_reserved_pad;
__le64	s_kbytes_written;
__le32	s_snapshot_inum;
__le32	s_snapshot_id;
__le64	s_snapshot_r_blocks_count;
__le32	s_snapshot_list;
__le32	s_error_count;
__le32	s_first_error_time;
__le32	s_first_error_ino;
__le64	s_first_error_block;
__u8	s_first_error_func[32];
__le32	s_first_error_line;
__le32	s_last_error_time;
__le32	s_last_error_ino;
__le32	s_last_error_line;
__le64	s_last_error_block;
__u8	s_last_error_func[32];
__u8	s_mount_opts[64];
__le32	s_usr_quota_inum;
__le32	s_grp_quota_inum;
__le32	s_overhead_blocks;
__le32	s_backup_bgs[2];
__u8	s_encrypt_algos[4];
__u8	s_encrypt_pw_salt[16];
__le32	s_lpf_ino;
__le32	s_prj_quota_inum;
__le32	s_checksum_seed;
__le32	s_reserved[98];
__le32	s_checksum;
} ext4_super_block;  /* 1024 byte */

#endif