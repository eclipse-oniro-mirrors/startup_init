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

#include "remount_stub.h"
#include "func_wrapper.h"
#include "init_log.h"
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <mntent.h>
#include <cerrno>
#include <csignal>
#include <sys/prctl.h>
#include <securec.h>
#include <csetjmp>

static constexpr uint64_t MOCK_DEV_SIZE = 4096 * 1024;

#ifdef __cplusplus
extern "C" {
#endif

static int g_stubResult[STUB_MAX] = {0};
static struct mntent *g_mntEntries = nullptr;
static int g_mntEntryCount = 0;
static int g_mntEntryIndex = 0;
static Fstab *g_stubFstab = nullptr;

static jmp_buf g_exitJmpBuf;
static bool g_exitJmpActive = false;

#define MAX_CMDLINE_PARAMS 8
#define MAX_SYSTEM_PARAMS 8
static struct {
    const char *paramName;
    char value[MAX_BUFFER_LEN];
    int ret;
} g_cmdlineParams[MAX_CMDLINE_PARAMS] = {{nullptr, {0}, 0}};
static int g_cmdlineParamCount = 0;

static struct {
    const char *key;
    char value[MAX_BUFFER_LEN];
    int ret;
} g_systemParams[MAX_SYSTEM_PARAMS] = {{nullptr, {0}, 0}};
static int g_systemParamCount = 0;

static char g_exactCmdlineResult[MAX_BUFFER_LEN] = {0};
static int g_exactCmdlineRet = -1;

#define MAX_SEQ_LEN 16
static int g_waitpidSeq[MAX_SEQ_LEN] = {0};
static int g_waitpidSeqLen = 0;
static int g_waitpidSeqIdx = 0;

static int g_mkdirSeq[MAX_SEQ_LEN] = {0};
static int g_mkdirSeqLen = 0;
static int g_mkdirSeqIdx = 0;

static int g_mountSeq[MAX_SEQ_LEN] = {0};
static int g_mountSeqLen = 0;
static int g_mountSeqIdx = 0;

static int g_sprintfCallCount = 0;
static int g_sprintfFailAfter = -1;

#ifdef WRAP_STRNCPY_S
static bool g_strncpyFail = false;
#endif

#ifdef WRAP_SNPRINTF_S
static int g_snprintfCallCount = 0;
static int g_snprintfFailAfter = -1;
#endif

void RemountSetStubResult(RemountStubType type, int result)
{
    if (type < STUB_MAX) {
        g_stubResult[type] = result;
    }
}

int RemountGetStubResult(RemountStubType type)
{
    if (type < STUB_MAX) {
        return g_stubResult[type];
    }
    return 0;
}

void RemountSetStubMntEntries(struct mntent *entries, int count)
{
    g_mntEntries = entries;
    g_mntEntryCount = count;
    g_mntEntryIndex = 0;
}

void RemountSetStubFstab(Fstab *fstab)
{
    g_stubFstab = fstab;
}

int MountStub(const char *source, const char *target,
    const char *filesystemtype, unsigned long mountflags, const void *data)
{
    (void)source;
    (void)target;
    (void)filesystemtype;
    (void)mountflags;
    (void)data;
    if (g_mountSeqLen > 0 && g_mountSeqIdx < g_mountSeqLen) {
        return g_mountSeq[g_mountSeqIdx++];
    }
    return g_stubResult[STUB_MOUNT];
}

int UmountStub(const char *target)
{
    (void)target;
    return g_stubResult[STUB_UMOUNT];
}

int Umount2Stub(const char *target, int flags)
{
    (void)target;
    (void)flags;
    return g_stubResult[STUB_UMOUNT2];
}

int ExecvStub(const char *pathname, char *const argv[])
{
    (void)pathname;
    (void)argv;
    return g_stubResult[STUB_EXECV];
}

int RebootStub(int cmd)
{
    (void)cmd;
    return g_stubResult[STUB_REBOOT];
}

void __wrap_exit(int status)  // NOLINT
{
    g_stubResult[STUB_EXIT] = status;
    if (g_exitJmpActive) {
        g_exitJmpActive = false;
        longjmp(g_exitJmpBuf, 1);
    }
}

void RemountSetupExitLongjmp(jmp_buf *buf)
{
    if (buf != nullptr) {
        errno_t memcpyRet = memcpy_s(g_exitJmpBuf, sizeof(g_exitJmpBuf), buf, sizeof(jmp_buf));
        if (memcpyRet != EOK) {
            return;
        }
    }
    g_exitJmpActive = true;
}

void RemountClearExitLongjmp(void)
{
    g_exitJmpActive = false;
}

FILE *SetmntentStub(const char *filename, const char *type)
{
    (void)filename;
    (void)type;
    if (g_stubResult[STUB_SETMNTENT] == 0) {
        return reinterpret_cast<FILE*>(static_cast<long>(1));
    }
    return nullptr;
}

struct mntent *GetmntentStub(FILE *stream)
{
    (void)stream;
    if (g_mntEntries != nullptr && g_mntEntryIndex < g_mntEntryCount) {
        return &g_mntEntries[g_mntEntryIndex++];
    }
    return nullptr;
}

int EndmntentStub(FILE *stream)
{
    (void)stream;
    g_mntEntryIndex = 0;
    return 1;
}

pid_t __wrap_fork(void)  // NOLINT
{
    return static_cast<pid_t>(g_stubResult[STUB_FORK]);
}

pid_t __wrap_waitpid(pid_t pid, int *status, int options)  // NOLINT
{
    if (g_waitpidSeqLen > 0 && g_waitpidSeqIdx < g_waitpidSeqLen) {
        if (status != nullptr) {
            *status = g_waitpidSeq[g_waitpidSeqIdx++];
        }
    } else {
        if (status != nullptr) {
            *status = g_stubResult[STUB_WAITPID];
        }
    }
    return pid;
}

int __wrap_mkdir(const char *pathname, mode_t mode)  // NOLINT
{
    if (g_mkdirSeqLen > 0 && g_mkdirSeqIdx < g_mkdirSeqLen) {
        int val = g_mkdirSeq[g_mkdirSeqIdx++];
        if (val > 0) {
            return 0;
        }
        if (val < 0) {
            errno = ENOENT;
            return -1;
        }
        return __real_mkdir(pathname, mode);
    }
    if (g_stubResult[STUB_MKDIR] > 0) {
        return 0;
    }
    if (g_stubResult[STUB_MKDIR] < 0) {
        errno = ENOENT;
        return -1;
    }
    return __real_mkdir(pathname, mode);
}

int __wrap_GetDevSize(const char *fsBlkDev, uint64_t *devSize)  // NOLINT
{
    (void)fsBlkDev;
    if (g_stubResult[STUB_GET_DEV_SIZE] != 0) {
        return g_stubResult[STUB_GET_DEV_SIZE];
    }
    if (devSize != nullptr) {
        *devSize = MOCK_DEV_SIZE;
    }
    return 0;
}

bool __wrap_CheckIsExt4(const char *dev, uint64_t offset)  // NOLINT
{
    (void)dev;
    (void)offset;
    return g_stubResult[STUB_CHECK_IS_EXT4] != 0;
}

int __wrap_MountExt4Device(const char *dev, const char *mnt, bool isFirstMount)  // NOLINT
{
    (void)dev;
    (void)mnt;
    (void)isFirstMount;
    return g_stubResult[STUB_MOUNT_EXT4_DEVICE];
}

void __wrap_OverlayRemountVendorPre(void)  // NOLINT
{
}

void __wrap_OverlayRemountVendorPost(void)  // NOLINT
{
}

int __wrap_InUpdaterMode(void)  // NOLINT
{
    return g_stubResult[STUB_IN_UPDATER_MODE];
}

int __wrap_GetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)  // NOLINT
{
    (void)paramName;
    if (value != nullptr && valueLen > 0) {
        errno_t snprintfRet = snprintf_s(value, valueLen, valueLen - 1, "test");
        if (snprintfRet < 0) {
            return g_stubResult[STUB_GET_PARAM_FROM_CMDLINE];
        }
    }
    return g_stubResult[STUB_GET_PARAM_FROM_CMDLINE];
}

int __wrap_GetBootSlots(void)  // NOLINT
{
    return g_stubResult[STUB_GET_BOOT_SLOTS];
}

void __wrap_FsAdjustPartitionNameBySlot(FstabItem *item)  // NOLINT
{
    (void)item;
}

Fstab *__wrap_LoadFstabFromCommandLine(void)  // NOLINT
{
    return g_stubFstab;
}

Fstab *__wrap_ReadFstabFromFile(const char *file, bool procMounts)  // NOLINT
{
    (void)file;
    (void)procMounts;
    return g_stubFstab;
}

int __wrap_CollectDmMergeTargets(Fstab *fstab, DmMergeCollectCtx *ctx)  // NOLINT
{
    (void)fstab;
    if (g_stubResult[STUB_COLLECT_DM_MERGE_TARGETS] != 0) {
        return g_stubResult[STUB_COLLECT_DM_MERGE_TARGETS];
    }
    if (ctx != nullptr) {
        ctx->num = 1;
        if (ctx->mntPaths != nullptr) {
            errno_t snprintfRet = snprintf_s(ctx->mntPaths[0], MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "/vendor");
            if (snprintfRet < 0) {
                return -1;
            }
        }
    }
    return 0;
}

void __wrap_DestroyDmMergeTargets(DmLinearTarget *targets, uint32_t targetNum)  // NOLINT
{
    (void)targets;
    (void)targetNum;
}

int __wrap_FsDmCreateMultiTargetLinearDevice(const char *devName, char *dmDevPath,  // NOLINT
    uint64_t dmDevPathLen, DmLinearTarget *target, uint32_t targetNum)
{
    (void)devName;
    (void)target;
    (void)targetNum;
    if (dmDevPath != nullptr && dmDevPathLen > 0) {
        errno_t snprintfRet = snprintf_s(dmDevPath, dmDevPathLen, dmDevPathLen - 1, "/dev/block/dm-test");
        if (snprintfRet < 0) {
            return g_stubResult[STUB_FS_DM_CREATE_MULTI_TARGET];
        }
    }
    return g_stubResult[STUB_FS_DM_CREATE_MULTI_TARGET];
}

int __wrap_FsDmInitDmDev(char *devPath, bool useSocket)  // NOLINT
{
    (void)devPath;
    (void)useSocket;
    return g_stubResult[STUB_FS_DM_INIT_DM_DEV];
}

int __wrap_FsDmRemoveDevice(const char *devName)  // NOLINT
{
    (void)devName;
    return 0;
}

void __wrap_WaitForFile(const char *source, unsigned int maxSecond)  // NOLINT
{
    (void)source;
    (void)maxSecond;
}

Fstab *__wrap_LoadRequiredFstab(void)  // NOLINT
{
    return g_stubFstab;
}

int UeventdSocketInitStub(void)
{
    if (g_stubResult[STUB_UEVENT_SOCKET_INIT] != 0) {
        return g_stubResult[STUB_UEVENT_SOCKET_INIT];
    }
    return -1;
}

void RetriggerDmUeventByPathStub(int sockFd, char *path, char **devices, int num)
{
    (void)sockFd;
    (void)path;
    (void)devices;
    (void)num;
}

int SprintfStub(char *buffer, size_t size, const char *fmt, ...)
{
    g_sprintfCallCount++;
    if (g_sprintfFailAfter >= 0 && g_sprintfCallCount > g_sprintfFailAfter) {
        return -1;
    }
    int len = -1;
    va_list vargs;
    va_start(vargs, fmt);
    len = vsnprintf_s(buffer, size, size - 1, fmt, vargs);
    va_end(vargs);
    return len;
}

int PrctlStub(int option, ...)
{
    (void)option;
    return 0;
}

int LchownStub(const char *pathname, uid_t owner, gid_t group)
{
    (void)pathname;
    (void)owner;
    (void)group;
    return 0;
}

int KillStub(pid_t pid, int signal)
{
    (void)pid;
    (void)signal;
    return 0;
}

int AccessStub(const char *pathname, int mode)
{
    (void)pathname;
    (void)mode;
    return g_stubResult[STUB_ACCESS];
}

void __wrap_CloseStdio(void)  // NOLINT
{
}

void EnableInitLogStub(InitLogLevel level)
{
    (void)level;
}

void OpenLogDeviceStub(void)
{
}

void CreateFsAndDeviceNodeStub(void)
{
}

int MountRequriedPartitionsStub(const Fstab *fstab)
{
    (void)fstab;
    return g_stubResult[STUB_MOUNT_REQURIED_PARTITIONS];
}

int SystemGetParameterStub(const char *key, char *value, uint32_t *len)
{
    for (int i = 0; i < g_systemParamCount; i++) {
        if (strcmp(g_systemParams[i].key, key) != 0) {
            continue;
        }
        if (value != nullptr && len != nullptr && *len > 0) {
            errno_t snprintfRet = snprintf_s(value, *len, *len - 1, "%s", g_systemParams[i].value);
            if (snprintfRet < 0) {
                *len = 0;
                return -1;
            }
            *len = strlen(value);
        }
        return g_systemParams[i].ret;
    }
    if (g_stubResult[STUB_SYSTEM_GET_PARAM] != 0) {
        return g_stubResult[STUB_SYSTEM_GET_PARAM];
    }
    if (value != nullptr && len != nullptr && *len > 0) {
        errno_t snprintfRet = snprintf_s(value, *len, *len - 1, "false");
        if (snprintfRet < 0) {
            *len = 0;
            return -1;
        }
        *len = strlen(value);
    }
    return 0;
}

void RemountSetSystemParam(const char *key, const char *value, int ret)
{
    if (g_systemParamCount >= MAX_SYSTEM_PARAMS) {
        return;
    }
    g_systemParams[g_systemParamCount].key = key;
    if (value != nullptr) {
        errno_t snprintfRet = snprintf_s(g_systemParams[g_systemParamCount].value,
            MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s", value);
        if (snprintfRet < 0) {
            return;
        }
    } else {
        memset_s(g_systemParams[g_systemParamCount].value, MAX_BUFFER_LEN, 0, MAX_BUFFER_LEN);
    }
    g_systemParams[g_systemParamCount].ret = ret;
    g_systemParamCount++;
}

void RemountResetSystemParams(void)
{
    g_systemParamCount = 0;
    memset_s(g_systemParams, sizeof(g_systemParams), 0, sizeof(g_systemParams));
}

void RetriggerUeventStub(int sockFd, char **devices, int num)
{
    (void)sockFd;
    (void)devices;
    (void)num;
}

bool __wrap_IsOverlayEnable(void)  // NOLINT
{
    return g_stubResult[STUB_IS_OVERLAY_ENABLE] != 0;
}

int __wrap_CheckHyperholdDisableMarker(void)  // NOLINT
{
    return g_stubResult[STUB_CHECK_HYPERHOLD_DISABLE_MARKER];
}

bool __wrap_IsHyperholdEnableMarkerSet(void)  // NOLINT
{
    return g_stubResult[STUB_IS_HYPERHOLD_ENABLE_MARKER_SET] != 0;
}

int __wrap_TryDmMergeOverlay(Fstab *fstab)  // NOLINT
{
    (void)fstab;
    return g_stubResult[STUB_TRY_DM_MERGE_OVERLAY];
}

int __wrap_CheckDmMergeCleanup(void)  // NOLINT
{
    return g_stubResult[STUB_CHECK_DM_MERGE_CLEANUP];
}

int __wrap_MountDmMergeOverlayAll(void)  // NOLINT
{
    return g_stubResult[STUB_MOUNT_DM_MERGE_OVERLAY_ALL];
}

void RemountSetExactCmdlineResult(const char *result, int ret)
{
    if (result != nullptr) {
        errno_t snprintfRet = snprintf_s(g_exactCmdlineResult, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s", result);
        if (snprintfRet < 0) {
            return;
        }
    } else {
        memset_s(g_exactCmdlineResult, MAX_BUFFER_LEN, 0, MAX_BUFFER_LEN);
    }
    g_exactCmdlineRet = ret;
}

void RemountSetCmdlineParam(const char *paramName, const char *value, int ret)
{
    if (g_cmdlineParamCount >= MAX_CMDLINE_PARAMS) {
        return;
    }
    g_cmdlineParams[g_cmdlineParamCount].paramName = paramName;
    if (value != nullptr) {
        errno_t snprintfRet = snprintf_s(g_cmdlineParams[g_cmdlineParamCount].value,
            MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s", value);
        if (snprintfRet < 0) {
            return;
        }
    } else {
        memset_s(g_cmdlineParams[g_cmdlineParamCount].value, MAX_BUFFER_LEN, 0, MAX_BUFFER_LEN);
    }
    g_cmdlineParams[g_cmdlineParamCount].ret = ret;
    g_cmdlineParamCount++;
}

void RemountResetCmdlineParams(void)
{
    g_cmdlineParamCount = 0;
    memset_s(g_cmdlineParams, sizeof(g_cmdlineParams), 0, sizeof(g_cmdlineParams));
}

int __wrap_GetExactParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)  // NOLINT
{
    if (g_stubResult[STUB_GET_EXACT_PARAM_FROM_CMDLINE] != 0) {
        return g_stubResult[STUB_GET_EXACT_PARAM_FROM_CMDLINE];
    }
    for (int i = 0; i < g_cmdlineParamCount; i++) {
        if (strcmp(g_cmdlineParams[i].paramName, paramName) != 0) {
            continue;
        }
        if (value != nullptr && valueLen > 0) {
            errno_t snprintfRet = snprintf_s(value, valueLen, valueLen - 1, "%s",
                g_cmdlineParams[i].value);
            if (snprintfRet < 0) {
                return -1;
            }
        }
        return g_cmdlineParams[i].ret;
    }
    if (value != nullptr && valueLen > 0) {
        errno_t snprintfRet = snprintf_s(value, valueLen, valueLen - 1, "%s", g_exactCmdlineResult);
        if (snprintfRet < 0) {
            return -1;
        }
    }
    return g_exactCmdlineRet;
}

uid_t GetuidStub(void)
{
    return static_cast<uid_t>(g_stubResult[STUB_GETUID]);
}

bool __wrap_IsDmMergeRemountEnabled(void)  // NOLINT
{
    return g_stubResult[STUB_IS_DM_MERGE_REMOUNT_ENABLED] != 0;
}

bool __wrap_IsDmMergeOverlayActive(void)  // NOLINT
{
    return g_stubResult[STUB_IS_DM_MERGE_OVERLAY_ACTIVE] != 0;
}

void RemountSetSprintfFailAfter(int count)
{
    g_sprintfFailAfter = count;
    g_sprintfCallCount = 0;
}

void RemountSetWaitpidSequence(const int *values, int count)
{
    if (count > MAX_SEQ_LEN) {
        count = MAX_SEQ_LEN;
    }
    g_waitpidSeqLen = count;
    g_waitpidSeqIdx = 0;
    if (count > 0 && values != nullptr) {
        errno_t memcpyRet = memcpy_s(g_waitpidSeq, sizeof(g_waitpidSeq), values, count * sizeof(int));
        if (memcpyRet != EOK) {
            return;
        }
    }
}

void RemountSetMkdirSequence(const int *values, int count)
{
    if (count > MAX_SEQ_LEN) {
        count = MAX_SEQ_LEN;
    }
    g_mkdirSeqLen = count;
    g_mkdirSeqIdx = 0;
    if (count > 0 && values != nullptr) {
        errno_t memcpyRet = memcpy_s(g_mkdirSeq, sizeof(g_mkdirSeq), values, count * sizeof(int));
        if (memcpyRet != EOK) {
            return;
        }
    }
}

void RemountSetMountSequence(const int *values, int count)
{
    if (count > MAX_SEQ_LEN) {
        count = MAX_SEQ_LEN;
    }
    g_mountSeqLen = count;
    g_mountSeqIdx = 0;
    if (count > 0 && values != nullptr) {
        errno_t memcpyRet = memcpy_s(g_mountSeq, sizeof(g_mountSeq), values, count * sizeof(int));
        if (memcpyRet != EOK) {
            return;
        }
    }
}

void RemountResetAllStubs(void)
{
    memset_s(g_stubResult, sizeof(g_stubResult), 0, sizeof(g_stubResult));
    g_mntEntries = nullptr;
    g_mntEntryCount = 0;
    g_mntEntryIndex = 0;
    g_stubFstab = nullptr;
    g_cmdlineParamCount = 0;
    memset_s(g_cmdlineParams, sizeof(g_cmdlineParams), 0, sizeof(g_cmdlineParams));
    g_systemParamCount = 0;
    memset_s(g_systemParams, sizeof(g_systemParams), 0, sizeof(g_systemParams));
    memset_s(g_exactCmdlineResult, sizeof(g_exactCmdlineResult), 0, sizeof(g_exactCmdlineResult));
    g_exactCmdlineRet = -1;
    g_waitpidSeqLen = 0;
    g_waitpidSeqIdx = 0;
    memset_s(g_waitpidSeq, sizeof(g_waitpidSeq), 0, sizeof(g_waitpidSeq));
    g_mkdirSeqLen = 0;
    g_mkdirSeqIdx = 0;
    memset_s(g_mkdirSeq, sizeof(g_mkdirSeq), 0, sizeof(g_mkdirSeq));
    g_mountSeqLen = 0;
    g_mountSeqIdx = 0;
    memset_s(g_mountSeq, sizeof(g_mountSeq), 0, sizeof(g_mountSeq));
    g_sprintfCallCount = 0;
    g_sprintfFailAfter = -1;
#ifdef WRAP_STRNCPY_S
    g_strncpyFail = false;
#endif
#ifdef WRAP_SNPRINTF_S
    g_snprintfCallCount = 0;
    g_snprintfFailAfter = -1;
#endif
    g_exitJmpActive = false;
}

#ifdef WRAP_STRNCPY_S
extern errno_t __real_strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count);  // NOLINT
errno_t __wrap_strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count)  // NOLINT
{
    if (g_strncpyFail) {
        return ERANGE;
    }
    return __real_strncpy_s(strDest, destMax, strSrc, count);
}

void RemountSetStrncpyFail(bool fail)
{
    g_strncpyFail = fail;
}
#endif

#ifdef WRAP_SNPRINTF_S
int __wrap_snprintf_s(char *strDest, size_t destMax, size_t count, const char *strFormat, ...)  // NOLINT
{
    g_snprintfCallCount++;
    if (g_snprintfFailAfter >= 0 && g_snprintfCallCount > g_snprintfFailAfter) {
        return -1;
    }
    va_list vargs;
    va_start(vargs, strFormat);
    int len = vsnprintf_s(strDest, destMax, count, strFormat, vargs);
    va_end(vargs);
    return len;
}

void RemountSetSnprintfFailAfter(int count)
{
    g_snprintfFailAfter = count;
    g_snprintfCallCount = 0;
}
#endif

#ifdef __cplusplus
}
#endif
