/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef REMOUNT_STUB_H
#define REMOUNT_STUB_H

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <csetjmp>
#include <sys/types.h>
#include <sys/stat.h>
#include <mntent.h>
#include "init_utils.h"
#include "fs_manager/fs_manager.h"
#include "dm_merge_overlay.h"
#include "fs_dm_linear.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STUB_MOUNT,
    STUB_UMOUNT,
    STUB_UMOUNT2,
    STUB_EXECV,
    STUB_REBOOT,
    STUB_FORK,
    STUB_WAITPID,
    STUB_EXIT,
    STUB_MKDIR,
    STUB_ACCESS,
    STUB_GET_DEV_SIZE,
    STUB_CHECK_IS_EXT4,
    STUB_MOUNT_EXT4_DEVICE,
    STUB_SETMNTENT,
    STUB_IN_UPDATER_MODE,
    STUB_GET_PARAM_FROM_CMDLINE,
    STUB_GET_BOOT_SLOTS,
    STUB_FS_ADJUST_PARTITION_NAME_BY_SLOT,
    STUB_LOAD_FSTAB_FROM_CMD_LINE,
    STUB_COLLECT_DM_MERGE_TARGETS,
    STUB_DESTROY_DM_MERGE_TARGETS,
    STUB_FS_DM_CREATE_MULTI_TARGET,
    STUB_FS_DM_INIT_DM_DEV,
    STUB_FS_DM_REMOVE_DEVICE,
    STUB_WAIT_FOR_FILE,
    STUB_FS_DM_CREATE_DEVICE,
    STUB_IS_OVERLAY_ENABLE,
    STUB_CHECK_HYPERHOLD_DISABLE_MARKER,
    STUB_IS_HYPERHOLD_ENABLE_MARKER_SET,
    STUB_TRY_DM_MERGE_OVERLAY,
    STUB_CHECK_DM_MERGE_CLEANUP,
    STUB_MOUNT_DM_MERGE_OVERLAY_ALL,
    STUB_IS_DM_MERGE_OVERLAY_ACTIVE,
    STUB_GET_DM_MERGE_DEV_PATH_AND_MOUNT_EXT4,
    STUB_MOUNT_REQURIED_PARTITIONS,
    STUB_SYSTEM_GET_PARAM,
    STUB_GET_EXACT_PARAM_FROM_CMDLINE,
    STUB_IS_DM_MERGE_REMOUNT_ENABLED,
    STUB_GETUID,
    STUB_UEVENT_SOCKET_INIT,
    STUB_MAX
} RemountStubType;

void RemountSetStubResult(RemountStubType type, int result);
int RemountGetStubResult(RemountStubType type);
void RemountSetStubMntEntries(struct mntent *entries, int count);
void RemountSetStubFstab(Fstab *fstab);
void RemountSetExactCmdlineResult(const char *result, int ret);
void RemountSetCmdlineParam(const char *paramName, const char *value, int ret);
void RemountResetCmdlineParams(void);
void RemountSetSystemParam(const char *key, const char *value, int ret);
void RemountResetSystemParams(void);
void RemountSetWaitpidSequence(const int *values, int count);
void RemountSetMkdirSequence(const int *values, int count);
void RemountSetMountSequence(const int *values, int count);
void RemountSetSprintfFailAfter(int count);
void RemountResetAllStubs(void);
void RemountSetupExitLongjmp(jmp_buf *buf);
void RemountClearExitLongjmp(void);

#ifdef WRAP_STRNCPY_S
void RemountSetStrncpyFail(bool fail);
#endif

#ifdef WRAP_SNPRINTF_S
void RemountSetSnprintfFailAfter(int count);
#endif

#ifdef __cplusplus
}
#endif

#endif
