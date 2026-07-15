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

#ifndef REMOUNT_DM_MERGE_UNITTEST_COMMON_H
#define REMOUNT_DM_MERGE_UNITTEST_COMMON_H

#include <gtest/gtest.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <dirent.h>
#include <linux/fs.h>
#include <mntent.h>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "func_wrapper.h"
#include "securec.h"
#include "init_utils.h"
#include "init_filesystems.h"
#include "dm_merge_overlay.h"
#include "erofs_mount_overlay.h"
#include "erofs_remount_overlay.h"
#include "remount_overlay.h"

extern "C" {
int GetDmMergeDevPath(char *dmDevPath, uint64_t dmDevPathLen);
int MountDmMergeExt4(char *dmDevPath);
int MountErofsOverlayItem(FstabItem *item);
int MountItemByFsType(FstabItem *item, MountResult *result);
void NormalizeDmName(char *dmName);
void RemoveOneErofsDmDevice(FstabItem *item);
void RemoveAllErofsDmDevices(void);
int RemoveDirContent(const char *path);
void ZeroOnePartitionExt4Superblock(FstabItem *item);
void ZeroPerPartitionExt4Superblocks(Fstab *fstab);

typedef enum {
    STUB_SPRINTF,
    STUB_MOUNT,
    STUB_MKNODE,
    STUB_EXECV,
    STUB_UMOUNT,
    STUB_MAX
} STUB_TYPE;
void SetStubResult(STUB_TYPE type, int result);
int GetStubResult(STUB_TYPE type);
void ReleaseFstabItem(FstabItem *item);
extern int g_bootSlots;
extern int g_currentSlot;

#define ASM_REAL_PREFIX "_" "_real_"
#define ASM_WRAP_PREFIX "_" "_wrap_"

ssize_t RealPread(int fd, void *buf, size_t count, off_t offset) __asm__(ASM_REAL_PREFIX "pread");
int RealStat(const char *pathname, struct stat *buf) __asm__(ASM_REAL_PREFIX "stat");
void *RealCalloc(size_t m, size_t n) __asm__(ASM_REAL_PREFIX "calloc");
char *RealStrdup(const char *string) __asm__(ASM_REAL_PREFIX "strdup");
Fstab *RealLoadFstabFromCommandLine(void) __asm__(ASM_REAL_PREFIX "LoadFstabFromCommandLine");
Fstab *WrapLoadFstabFromCommandLine(void) __asm__(ASM_WRAP_PREFIX "LoadFstabFromCommandLine");
Fstab *RealReadFstabFromFile(const char *file, bool procMounts) __asm__(ASM_REAL_PREFIX "ReadFstabFromFile");
Fstab *WrapReadFstabFromFile(const char *file, bool procMounts) __asm__(ASM_WRAP_PREFIX "ReadFstabFromFile");
int RealGetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)
    __asm__(ASM_REAL_PREFIX "GetParameterFromCmdLine");
int WrapGetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)
    __asm__(ASM_WRAP_PREFIX "GetParameterFromCmdLine");
int RealInUpdaterMode(void) __asm__(ASM_REAL_PREFIX "InUpdaterMode");
int WrapInUpdaterMode(void) __asm__(ASM_WRAP_PREFIX "InUpdaterMode");
int RealGetBootSlots(void) __asm__(ASM_REAL_PREFIX "GetBootSlots");
int WrapGetBootSlots(void) __asm__(ASM_WRAP_PREFIX "GetBootSlots");
int RealGetCurrentSlot(void) __asm__(ASM_REAL_PREFIX "GetCurrentSlot");
int WrapGetCurrentSlot(void) __asm__(ASM_WRAP_PREFIX "GetCurrentSlot");
int RealGetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length)
    __asm__(ASM_REAL_PREFIX "GetMapperAddrForMerge");
int WrapGetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length)
    __asm__(ASM_WRAP_PREFIX "GetMapperAddrForMerge");
int RealFsDmCreateLinearDevice(const char *devName, char *dmBlkName,
    uint64_t dmBlkNameLen, DmVerityTarget *target) __asm__(ASM_REAL_PREFIX "FsDmCreateLinearDevice");
int WrapFsDmCreateLinearDevice(const char *devName, char *dmBlkName,
    uint64_t dmBlkNameLen, DmVerityTarget *target) __asm__(ASM_WRAP_PREFIX "FsDmCreateLinearDevice");
int RealFsDmCreateMultiTargetLinearDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen,
    DmLinearTarget *target, uint32_t targetNum) __asm__(ASM_REAL_PREFIX "FsDmCreateMultiTargetLinearDevice");
int WrapFsDmCreateMultiTargetLinearDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen,
    DmLinearTarget *target, uint32_t targetNum) __asm__(ASM_WRAP_PREFIX "FsDmCreateMultiTargetLinearDevice");
int RealFsDmInitDmDev(char *devPath, bool useSocket) __asm__(ASM_REAL_PREFIX "FsDmInitDmDev");
int WrapFsDmInitDmDev(char *devPath, bool useSocket) __asm__(ASM_WRAP_PREFIX "FsDmInitDmDev");
int RealFsDmRemoveDevice(const char *devName) __asm__(ASM_REAL_PREFIX "FsDmRemoveDevice");
int WrapFsDmRemoveDevice(const char *devName) __asm__(ASM_WRAP_PREFIX "FsDmRemoveDevice");
int RealDmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen)
    __asm__(ASM_REAL_PREFIX "DmGetDeviceName");
int WrapDmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen)
    __asm__(ASM_WRAP_PREFIX "DmGetDeviceName");
void RealWaitForFile(const char *source, unsigned int maxSecond) __asm__(ASM_REAL_PREFIX "WaitForFile");
void WrapWaitForFile(const char *source, unsigned int maxSecond) __asm__(ASM_WRAP_PREFIX "WaitForFile");
int RealFormatExt4(const char *fsBlkDev, const char *fsMntPoint) __asm__(ASM_REAL_PREFIX "FormatExt4");
int WrapFormatExt4(const char *fsBlkDev, const char *fsMntPoint) __asm__(ASM_WRAP_PREFIX "FormatExt4");
bool RealCheckIsExt4(const char *dev, uint64_t offset) __asm__(ASM_REAL_PREFIX "CheckIsExt4");
bool WrapCheckIsExt4(const char *dev, uint64_t offset) __asm__(ASM_WRAP_PREFIX "CheckIsExt4");
bool RealCheckIsErofs(const char *dev) __asm__(ASM_REAL_PREFIX "CheckIsErofs");
bool WrapCheckIsErofs(const char *dev) __asm__(ASM_WRAP_PREFIX "CheckIsErofs");
bool RealIsOverlayEnable(void) __asm__(ASM_REAL_PREFIX "IsOverlayEnable");
bool WrapIsOverlayEnable(void) __asm__(ASM_WRAP_PREFIX "IsOverlayEnable");
int RealMountOverlayOne(const char *mnt, const char *overlayPrefix) __asm__(ASM_REAL_PREFIX "MountOverlayOne");
int WrapMountOverlayOne(const char *mnt, const char *overlayPrefix) __asm__(ASM_WRAP_PREFIX "MountOverlayOne");
int RealDoMountOverlayDevice(FstabItem *item) __asm__(ASM_REAL_PREFIX "DoMountOverlayDevice");
int WrapDoMountOverlayDevice(FstabItem *item) __asm__(ASM_WRAP_PREFIX "DoMountOverlayDevice");
void RealOverlayRemountVendorPre(void) __asm__(ASM_REAL_PREFIX "OverlayRemountVendorPre");
void WrapOverlayRemountVendorPre(void) __asm__(ASM_WRAP_PREFIX "OverlayRemountVendorPre");
void RealOverlayRemountVendorPost(void) __asm__(ASM_REAL_PREFIX "OverlayRemountVendorPost");
void WrapOverlayRemountVendorPost(void) __asm__(ASM_WRAP_PREFIX "OverlayRemountVendorPost");
int RealSwitchRoot(const char *newRoot) __asm__(ASM_REAL_PREFIX "SwitchRoot");
int WrapSwitchRoot(const char *newRoot) __asm__(ASM_WRAP_PREFIX "SwitchRoot");
void RealSetRemountResultFlag(void) __asm__(ASM_REAL_PREFIX "SetRemountResultFlag");
void WrapSetRemountResultFlag(void) __asm__(ASM_WRAP_PREFIX "SetRemountResultFlag");
int RealGetRemountResult(void) __asm__(ASM_REAL_PREFIX "GetRemountResult");
int WrapGetRemountResult(void) __asm__(ASM_WRAP_PREFIX "GetRemountResult");
bool RealIsDmMergeOverlayActive(void) __asm__(ASM_REAL_PREFIX "IsDmMergeOverlayActive");
bool WrapIsDmMergeOverlayActive(void) __asm__(ASM_WRAP_PREFIX "IsDmMergeOverlayActive");
bool RealIsDmMergeRemountEnabled(void) __asm__(ASM_REAL_PREFIX "IsDmMergeRemountEnabled");
bool WrapIsDmMergeRemountEnabled(void) __asm__(ASM_WRAP_PREFIX "IsDmMergeRemountEnabled");
int RealTryDmMergeRemount(void) __asm__(ASM_REAL_PREFIX "TryDmMergeRemount");
int WrapTryDmMergeRemount(void) __asm__(ASM_WRAP_PREFIX "TryDmMergeRemount");
void RealEngFilesOverlay(const char *source, const char *target) __asm__(ASM_REAL_PREFIX "EngFilesOverlay");
void WrapEngFilesOverlay(const char *source, const char *target) __asm__(ASM_WRAP_PREFIX "EngFilesOverlay");
FILE *RealSetMntEnt(const char *filename, const char *type) __asm__(ASM_REAL_PREFIX "setmntent");
FILE *WrapSetMntEnt(const char *filename, const char *type) __asm__(ASM_WRAP_PREFIX "setmntent");
struct mntent *RealGetMntEnt(FILE *stream) __asm__(ASM_REAL_PREFIX "getmntent");
struct mntent *WrapGetMntEnt(FILE *stream) __asm__(ASM_WRAP_PREFIX "getmntent");
int RealEndMntEnt(FILE *stream) __asm__(ASM_REAL_PREFIX "endmntent");
int WrapEndMntEnt(FILE *stream) __asm__(ASM_WRAP_PREFIX "endmntent");
ssize_t RealPwrite(int fd, const void *buf, size_t count, off_t offset) __asm__(ASM_REAL_PREFIX "pwrite");
ssize_t WrapPwrite(int fd, const void *buf, size_t count, off_t offset) __asm__(ASM_WRAP_PREFIX "pwrite");
int RealUnlink(const char *pathname) __asm__(ASM_REAL_PREFIX "unlink");
int WrapUnlink(const char *pathname) __asm__(ASM_WRAP_PREFIX "unlink");
int RealRmdir(const char *pathname) __asm__(ASM_REAL_PREFIX "rmdir");
int WrapRmdir(const char *pathname) __asm__(ASM_WRAP_PREFIX "rmdir");
DIR *RealOpenDir(const char *name) __asm__(ASM_REAL_PREFIX "opendir");
DIR *WrapOpenDir(const char *name) __asm__(ASM_WRAP_PREFIX "opendir");
struct dirent *RealReadDir(DIR *dirp) __asm__(ASM_REAL_PREFIX "readdir");
struct dirent *WrapReadDir(DIR *dirp) __asm__(ASM_WRAP_PREFIX "readdir");
int RealCloseDir(DIR *dirp) __asm__(ASM_REAL_PREFIX "closedir");
int WrapCloseDir(DIR *dirp) __asm__(ASM_WRAP_PREFIX "closedir");
int RealLstat(const char *pathname, struct stat *buf) __asm__(ASM_REAL_PREFIX "lstat");
int WrapLstat(const char *pathname, struct stat *buf) __asm__(ASM_WRAP_PREFIX "lstat");
int RealUmount2(const char *target, int flags) __asm__(ASM_REAL_PREFIX "umount2");
int WrapUmount2(const char *target, int flags) __asm__(ASM_WRAP_PREFIX "umount2");
void RealSync(void) __asm__(ASM_REAL_PREFIX "sync");
void WrapSync(void) __asm__(ASM_WRAP_PREFIX "sync");
int RealSystemReadParam(const char *name, char *value, uint32_t *len) __asm__(ASM_REAL_PREFIX "SystemReadParam");
int WrapSystemReadParam(const char *name, char *value, uint32_t *len) __asm__(ASM_WRAP_PREFIX "SystemReadParam");

#undef ASM_REAL_PREFIX
#undef ASM_WRAP_PREFIX
}

constexpr mode_t TEST_DIR_MODE = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
constexpr mode_t TEST_FILE_MODE = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
constexpr uint64_t TEST_MAPPER_START = BLOCK_SIZE_UINT;
constexpr uint64_t TEST_MAPPER_LENGTH = BLOCK_SIZE_UINT * SECTOR_SIZE / sizeof(uint16_t);
constexpr int TEST_OPEN_FD = STDERR_FILENO + sizeof(uint64_t);

struct FstabEntry {
    std::string device;
    std::string mountPoint;
    std::string fsType;
};

struct TestState {
    bool mockGetRemountResult = false;
    int remountResult = REMOUNT_FAIL;
    bool mockDmMergeActive = false;
    bool dmMergeActive = false;
    bool mockDmGetDeviceName = false;
    int dmGetDeviceNameRet = 0;
    int dmGetDeviceNameCalls = 0;
    std::vector<int> dmGetDeviceNameSeq;
    std::string dmGetDevicePath = "/dev/block/dm-merge";
    bool mockDmMergeEnabled = false;
    bool dmMergeEnabled = false;
    bool mockTryDmMergeRemount = false;
    int tryDmMergeRemountRet = REMOUNT_FAIL;
    int tryDmMergeRemountCalls = 0;

    bool mockLoadFstab = false;
    bool loadFstabNull = false;
    bool mockReadFstab = false;
    int readFstabCalls = 0;
    std::vector<FstabEntry> fstabEntries;
    bool mockGetParameterFromCmdLine = false;
    int getParameterRet = 0;
    std::string hardware = "default";
    bool mockInUpdaterMode = false;
    int updaterMode = 0;
    bool mockGetBootSlots = false;
    int bootSlots = 1;
    bool mockGetCurrentSlot = false;
    int currentSlot = 1;

    bool mockMapper = false;
    int mapperRet = 0;
    uint64_t mapperStart = TEST_MAPPER_START;
    uint64_t mapperLength = TEST_MAPPER_LENGTH;
    int mapperCalls = 0;
    std::string lastMapperDevice;

    bool mockCreateLinear = false;
    int createLinearRet = 0;
    int createLinearCalls = 0;
    std::string lastLinearDeviceName;
    bool mockCreateDm = false;
    int createDmRet = 0;
    int createDmCalls = 0;
    uint32_t lastTargetNum = 0;
    bool mockInitDm = false;
    int initDmRet = 0;
    int initDmCalls = 0;
    bool mockRemoveDm = false;
    int removeDmCalls = 0;
    std::vector<std::string> removedDmNames;
    bool mockWaitForFile = false;
    int waitForFileCalls = 0;
    bool mockFormatExt4 = false;
    int formatExt4Ret = 0;
    int formatExt4Calls = 0;
    bool mockCheckExt4 = false;
    bool checkExt4Ret = true;
    int checkExt4Calls = 0;
    bool mockCheckErofs = false;
    bool checkErofsRet = true;
    int checkErofsCalls = 0;
    bool mockOverlayEnable = false;
    bool overlayEnableRet = true;
    int overlayEnableCalls = 0;

    int openRet = TEST_OPEN_FD;
    int openCalls = 0;
    std::vector<int> openSeq;
    int accessRet = 0;
    int accessCalls = 0;
    int mkdirCalls = 0;
    int mkdirFailAt = 0;

    bool mockMountOverlay = false;
    int mountOverlayRet = 0;
    int mountOverlayCalls = 0;
    std::vector<std::string> mountedOverlayPaths;
    bool mockDoMountOverlayDevice = false;
    int doMountOverlayDeviceRet = 0;
    int doMountOverlayDeviceCalls = 0;
    int vendorPreCalls = 0;
    int vendorPostCalls = 0;
    bool mockSwitchRoot = false;
    int switchRootRet = 0;
    int switchRootCalls = 0;
    bool mockSetRemountResultFlag = false;
    int setRemountResultFlagCalls = 0;
    bool mockEngFilesOverlay = false;
    int engFilesOverlayCalls = 0;

    bool mockMountTable = false;
    bool setmntentFails = false;
    int setmntentCalls = 0;
    int getmntentCalls = 0;
    int endmntentCalls = 0;

    bool mockPread = false;
    ssize_t preadRet = -1;
    std::string preadContent;
    int preadCalls = 0;
    bool mockPwrite = false;
    int pwriteCalls = 0;
    int closeCalls = 0;
    bool mockUnlink = false;
    int unlinkRet = 0;
    int unlinkCalls = 0;
    bool mockRmdir = false;
    int rmdirCalls = 0;
    bool mockDirOps = false;
    bool dirOpsOpenSucceeds = false;
    std::vector<std::pair<std::string, unsigned char>> dirEntries;
    size_t dirEntryIndex = 0;
    int opendirCalls = 0;
    int readdirCalls = 0;
    int closedirCalls = 0;
    bool mockStat = false;
    int statRet = 0;
    int statCalls = 0;
    bool mockLstat = false;
    int lstatRet = -1;
    int lstatCalls = 0;
    int ioctlCalls = 0;
    uint64_t ioctlDeviceSize = TEST_MAPPER_LENGTH;
    int umount2Calls = 0;
    int syncCalls = 0;
    bool mockSystemReadParam = false;
    int systemReadParamRet = -1;
    std::string systemParamValue;
    int systemReadParamCalls = 0;
    int systemMountCalls = 0;
    std::string lastMountSource;
    std::string lastMountTarget;
    std::string lastMountFsType;
    std::string lastMountData;
    int callocCalls = 0;
    int callocFailAt = 0;
    int strdupCalls = 0;
    int strdupFailAt = 0;
};

extern TestState g_state;
extern int g_fakeMountFile;
extern int g_mountStubCalls;
extern int g_umount2StubCalls;
extern char g_mountStubTarget[MAX_BUFFER_LEN];
extern char g_mountStubData[MAX_BUFFER_LEN];
extern const SUPPORTED_FILE_SYSTEM g_erofsFileSystem;
extern const SUPPORTED_FILE_SYSTEM *g_extendedFileSystems[];

FstabItem *MakeFstabItem(const FstabEntry &entry);
Fstab *BuildFstab(const std::vector<FstabEntry> &entries);
DmMergeCollectCtx MakeCollectCtx(DmLinearTarget *targets, char mntPaths[][MAX_BUFFER_LEN]);
void ExpectUniqueMountedOverlayPaths(void);

int AccessMock(const char *pathname, int mode);
int MkdirMock(const char *path, mode_t mode);
int OpenMock(const char *pathname, int flag);
int CloseMock(int fd);
ssize_t PreadMock(int fd, void *const buf, size_t count, off_t offset);
int StatMock(const char *pathname, struct stat *buf);
int IoctlMock(int fd, int req, va_list args);
int MountRecordMock(const char *source, const char *target, const char *fsType, unsigned long flags, const void *data);
void *CallocMock(size_t m, size_t n);
char *StrdupMock(const char *string);

namespace init_ut {
class RemountDmMergeUnitTest : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
    void UseDefaultDmMergeMocks();
    void UseFirstStageDmMergeMocks();
    void UseActiveDmMergeDeviceMocks();
};
}

#endif // REMOUNT_DM_MERGE_UNITTEST_COMMON_H
