/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include "init_utils.h"
#include "remount_overlay.h"
#include "dm_merge_overlay.h"
#include "erofs_remount_overlay.h"
#include "erofs_overlay_common.h"
#include "remount_stub.h"
#include "fs_manager/fs_manager.h"
#include "fs_dm_linear.h"
#include "mntent.h"
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <securec.h>

using namespace std;
using namespace testing::ext;

#ifdef __cplusplus
extern "C" {
#endif
bool IsSkipRemount(const struct mntent mentry);
int ExecCommand(int argc, char **argv);
void OverlayRemountPre(const char *mnt);
void OverlayRemountPost(const char *mnt);
bool FormatAndMountExt4(const char *devExt4, const char *mnt);
bool DoRemount(struct mntent *mentry);
int RootOverlaySetup(void);
bool DoSystemRemount(struct mntent *mentry);
int PerPartitionRemountFallback(void);
uint64_t AlignTo(uint64_t base, uint64_t alignment);
int ConstructOverlayLowerPath(const char *mnt, char *dirLower, uint64_t dirLowerLen);
int ConstructOverlayMntOpt(const char *mnt, const char *overlayPrefix, char *mntOpt, uint64_t mntOptLen);
int DoOverlayMount(const char *mnt, const char *mntOpt);
void AllocDmName(const char *name, char *nameRofs, const uint64_t nameRofsLen,
    char *nameExt4, const uint64_t nameExt4Len);
int ConstructLinearTarget(DmVerityTarget *target, const char *dev, uint64_t mapStart, uint64_t mapLength);
void DestoryLinearTarget(DmVerityTarget *target);
int Modem2Exchange(const char *modemPath, const char *exchangePath);
int Exchange2Modem(const char *modemPath, const char *exchangePath);
int MkdirExt4OverlayDirs(const char *mnt);
int MkdirExt4UpperWorkDirs(const char *mnt);
bool __real_CheckIsExt4(const char *dev, uint64_t offset);  // NOLINT
int __real_MountExt4Device(const char *dev, const char *mnt, bool isFirstMount);  // NOLINT
bool __real_IsOverlayEnable(void);  // NOLINT
int __real_mkdir(const char *pathname, mode_t mode);  // NOLINT
int __real_GetDevSize(const char *fsBlkDev, uint64_t *devSize);  // NOLINT
uint64_t LookupErofsEnd(const char *dev);
uint64_t GetImgSize(const char *dev, uint64_t offset);
uint64_t GetBlockSize(const char *dev);
int GetMapperAddr(const char *dev, uint64_t *start, uint64_t *length);
int GetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length);
bool CheckIsErofs(const char *dev);
int DoOverlayMount(const char *mnt, const char *mntOpt);
int RemountOverlay(void);
uint64_t GetFsSize(int fd);
void NormalizeDmName(char *dmName);
int CollectOneDmMergeTarget(FstabItem *item, DmMergeCollectCtx *ctx);
int RemoveDirContent(const char *path);
void RemoveOneErofsDmDevice(FstabItem *item);
int RemountMainEntry(int argc, const char *argv[]);

static void CreateTestFile(const char *fileName, const char *data)
{
    FILE *tmpFile = fopen(fileName, "wr");
    if (tmpFile != nullptr) {
        fprintf(tmpFile, "%s", data);
        (void)fflush(tmpFile);
        fclose(tmpFile);
    }
}
#ifdef __cplusplus
}
#endif

namespace init_ut {
static constexpr int MOCK_FORK_PID = 1000;
static constexpr int MOCK_NON_ROOT_UID = 1000;
static constexpr int MOCK_MAP_LENGTH = 4096 * 512;
static constexpr int MAX_OVERLAY_PATHS = 8;

class RemountOverlayUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) { RemountResetAllStubs(); };
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(RemountOverlayUnitTest, Init_MntNeedRemountTest_001, TestSize.Level0)
{
    EXPECT_EQ(MntNeedRemount("/test"), false);
    EXPECT_EQ(MntNeedRemount("/"), true);
    EXPECT_EQ(MntNeedRemount("/vendor"), true);
    EXPECT_EQ(MntNeedRemount("/system"), false);
    EXPECT_EQ(MntNeedRemount("/usr"), true);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsSkipRemountTest_001, TestSize.Level0)
{
    struct mntent mentry;
    memset_s(&mentry, sizeof(mentry), 0, sizeof(mentry));
    EXPECT_EQ(IsSkipRemount(mentry), true);

    char ufs[] = "ufs";
    char test[] = "test";
    mentry.mnt_type = ufs;
    mentry.mnt_dir = test;
    EXPECT_EQ(IsSkipRemount(mentry), true);

    char root[] = "/";
    mentry.mnt_dir = root;
    EXPECT_EQ(IsSkipRemount(mentry), true);

    char erofs1[] = "er11ofs";
    mentry.mnt_type = erofs1;
    EXPECT_EQ(IsSkipRemount(mentry), true);

    char erofs[] = "erofs";
    char ndm[] = "/dev/block/ndm-";
    mentry.mnt_type = erofs;
    mentry.mnt_fsname = ndm;
    EXPECT_EQ(IsSkipRemount(mentry), true);

    char dm1[] = "/dev/block/dm-1";
    mentry.mnt_fsname = dm1;
    EXPECT_EQ(IsSkipRemount(mentry), false);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsSkipRemountTest_002, TestSize.Level1)
{
    struct mntent mentry;
    memset_s(&mentry, sizeof(mentry), 0, sizeof(mentry));
    char nullType[] = "";
    mentry.mnt_type = nullType;
    mentry.mnt_dir = nullptr;
    EXPECT_EQ(IsSkipRemount(mentry), true);
}

HWTEST_F(RemountOverlayUnitTest, Init_ExecCommand_001, TestSize.Level0)
{
    char *nullArgv[] = {nullptr};
    EXPECT_EQ(ExecCommand(0, nullArgv), -1);
    EXPECT_EQ(ExecCommand(1, nullptr), -1);
    EXPECT_EQ(ExecCommand(1, nullArgv), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_ExecCommand_002, TestSize.Level1)
{
    char *validArgv[] = {const_cast<char*>("/bin/ls"), nullptr};
    RemountSetStubResult(STUB_FORK, -1);
    EXPECT_EQ(ExecCommand(1, validArgv), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_ExecCommand_003, TestSize.Level1)
{
    char *validArgv[] = {const_cast<char*>("/bin/ls"), nullptr};
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    EXPECT_EQ(ExecCommand(1, validArgv), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_ExecCommand_004, TestSize.Level1)
{
    char *validArgv[] = {const_cast<char*>("/bin/ls"), nullptr};
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 256);
    int ret = ExecCommand(1, validArgv);
    EXPECT_NE(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_ExecCommand_005, TestSize.Level1)
{
    char *validArgv[] = {const_cast<char*>("/bin/ls"), nullptr};
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 1);
    int ret = ExecCommand(1, validArgv);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetDevSizeTest_001_fail, TestSize.Level0)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    EXPECT_EQ(GetDevSize("", nullptr), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetDevSizeTest_002_failWithPtr, TestSize.Level1)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    uint64_t devSize = 0;
    EXPECT_EQ(GetDevSize("/dev/block/test", &devSize), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetDevSizeTest_003_success, TestSize.Level1)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    uint64_t devSize = 0;
    EXPECT_EQ(GetDevSize("/dev/block/test", &devSize), 0);
    EXPECT_GT(devSize, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatExt4Test_001_devSizeFail, TestSize.Level0)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    int ret = FormatExt4("/dev/block/test", "/mnt");
    EXPECT_NE(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatExt4Test_002_mke2fsFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 256);
    int ret = FormatExt4("/dev/block/test", "/mnt");
    EXPECT_NE(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatExt4Test_003_e2fsdroidFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = FormatExt4("/dev/block/test", "/mnt");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_OverlayRemountPre_001, TestSize.Level0)
{
    OverlayRemountPre("/vendor");
    OverlayRemountPre("/system");
    OverlayRemountPre("/usr");
    EXPECT_EQ(RemountGetStubResult(STUB_REBOOT), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_OverlayRemountPost_001, TestSize.Level0)
{
    OverlayRemountPost("/vendor");
    OverlayRemountPost("/system");
    OverlayRemountPost("/usr");
    EXPECT_EQ(RemountGetStubResult(STUB_REBOOT), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatAndMountExt4_001_isExt4, TestSize.Level1)
{
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    bool ret = FormatAndMountExt4("/dev/block/dm-test", "/mnt");
    EXPECT_EQ(ret, true);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatAndMountExt4_002_formatFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    bool ret = FormatAndMountExt4("/dev/block/dm-test", "/mnt");
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatAndMountExt4_003_formatSuccMountFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_MOUNT_EXT4_DEVICE, -1);
    bool ret = FormatAndMountExt4("/dev/block/dm-test", "/mnt");
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatAndMountExt4_004_success, TestSize.Level1)
{
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_MOUNT_EXT4_DEVICE, 0);
    bool ret = FormatAndMountExt4("/dev/block/dm-test", "/mnt");
    EXPECT_EQ(ret, true);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoRemount_001_formatFail, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/vendor";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDir;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    EXPECT_EQ(DoRemount(&mentry), false);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoRemount_002_lowerPath, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDirLower[] = "/mnt/lower/vendor";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDirLower;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    bool ret = DoRemount(&mentry);
    EXPECT_EQ(ret, true);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoRemount_003_vendorOverlay, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDirVendor[] = "/vendor";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDirVendor;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    bool ret = DoRemount(&mentry);
    EXPECT_EQ(ret, true);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoRemount_004_overlayFail, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/vendor";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDir;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    RemountSetStubResult(STUB_MOUNT, -1);
    bool ret = DoRemount(&mentry);
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_001_mkdirFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_MKDIR, -1);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(RootOverlaySetup(), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_002_mkdirSuccMountFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(RootOverlaySetup(), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_003_success, TestSize.Level1)
{
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
    EXPECT_EQ(RootOverlaySetup(), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoSystemRemount_001_isExt4OverlaySucc, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDir;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
    EXPECT_EQ(DoSystemRemount(&mentry), true);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoSystemRemount_002_formatFail, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDir;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    EXPECT_EQ(DoSystemRemount(&mentry), false);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoSystemRemount_003_formatSuccMountFail, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDir;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_MOUNT_EXT4_DEVICE, -1);
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(DoSystemRemount(&mentry), false);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoSystemRemount_004_overlayFail, TestSize.Level1)
{
    struct mntent mentry;
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/";
    char mntType[] = "erofs";
    mentry.mnt_fsname = fsname;
    mentry.mnt_dir = mntDir;
    mentry.mnt_type = mntType;
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    RemountSetStubResult(STUB_MKDIR, -1);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(DoSystemRemount(&mentry), false);
}

HWTEST_F(RemountOverlayUnitTest, Init_PerPartitionRemountFallback_001_nullFp, TestSize.Level1)
{
    RemountSetStubResult(STUB_SETMNTENT, -1);
    EXPECT_EQ(PerPartitionRemountFallback(), REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_PerPartitionRemountFallback_002_skipAll, TestSize.Level1)
{
    struct mntent entries[1];
    char fsname[] = "/dev/block/sda";
    char mntDir[] = "/data";
    char mntType[] = "ext4";
    entries[0].mnt_fsname = fsname;
    entries[0].mnt_dir = mntDir;
    entries[0].mnt_type = mntType;
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(entries, 1);
    EXPECT_EQ(PerPartitionRemountFallback(), REMOUNT_SUCC);
}

HWTEST_F(RemountOverlayUnitTest, Init_PerPartitionRemountFallback_003_rootRemountFail, TestSize.Level1)
{
    struct mntent entries[1];
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/";
    char mntType[] = "erofs";
    entries[0].mnt_fsname = fsname;
    entries[0].mnt_dir = mntDir;
    entries[0].mnt_type = mntType;
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    RemountSetStubMntEntries(entries, 1);
    EXPECT_EQ(PerPartitionRemountFallback(), REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_PerPartitionRemountFallback_004_partitionRemountFail, TestSize.Level1)
{
    struct mntent entries[1];
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/vendor";
    char mntType[] = "erofs";
    entries[0].mnt_fsname = fsname;
    entries[0].mnt_dir = mntDir;
    entries[0].mnt_type = mntType;
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    RemountSetStubMntEntries(entries, 1);
    EXPECT_EQ(PerPartitionRemountFallback(), REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_PerPartitionRemountFallback_005_success, TestSize.Level1)
{
    struct mntent entries[1];
    char fsname[] = "/dev/block/dm-0";
    char mntDir[] = "/vendor";
    char mntType[] = "erofs";
    entries[0].mnt_fsname = fsname;
    entries[0].mnt_dir = mntDir;
    entries[0].mnt_type = mntType;
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubMntEntries(entries, 1);
    EXPECT_EQ(PerPartitionRemountFallback(), REMOUNT_SUCC);
}

HWTEST_F(RemountOverlayUnitTest, Init_EngFilesOverlay_001, TestSize.Level1)
{
    std::string source = std::string(STARTUP_INIT_UT_PATH) + "/data/eng_test_src";
    CheckAndCreateDir(source.c_str());
    EngFilesOverlay(source.c_str(), "/");
    EngFilesOverlay("/nonexistent_path", "/");
    remove(source.c_str());
    EXPECT_EQ(RemountGetStubResult(STUB_MOUNT), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_EngFilesOverlay_002, TestSize.Level1)
{
    std::string source = std::string(STARTUP_INIT_UT_PATH) + "/data/eng_test_dir";
    std::string subDir = source + "/subdir";
    std::string regFile = source + "/regfile";
    CheckAndCreateDir(source.c_str());
    CheckAndCreateDir(subDir.c_str());
    CreateTestFile(regFile.c_str(), "test");
    EngFilesOverlay(source.c_str(), "/");
    remove(regFile.c_str());
    remove(subDir.c_str());
    remove(source.c_str());
    EXPECT_EQ(RemountGetStubResult(STUB_MOUNT), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_001_resultNotSucc, TestSize.Level1)
{
    DeleteRemountResultFlag();
    int ret = RemountRofsOverlay();
    EXPECT_NE(ret, REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_002_dmMergeActive, TestSize.Level1)
{
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = RemountRofsOverlay();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_003_dmMergeSucc, TestSize.Level1)
{
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(nullptr, 0);
    int ret = RemountRofsOverlay();
    EXPECT_NE(ret, REMOUNT_FAIL);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_004_perPartition, TestSize.Level1)
{
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(nullptr, 0);
    int ret = RemountRofsOverlay();
    EXPECT_NE(ret, REMOUNT_FAIL);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_AlignTo_001, TestSize.Level0)
{
    EXPECT_EQ(AlignTo(0, 4096), 0);
    EXPECT_EQ(AlignTo(1, 4096), 4096);
    EXPECT_EQ(AlignTo(4096, 4096), 4096);
    EXPECT_EQ(AlignTo(4097, 4096), 8192);
    EXPECT_EQ(AlignTo(100, 0), 100);
}

HWTEST_F(RemountOverlayUnitTest, Init_ConstructOverlayLowerPath_001, TestSize.Level0)
{
    char dirLower[MAX_BUFFER_LEN] = {0};
    EXPECT_EQ(ConstructOverlayLowerPath("/vendor", dirLower, MAX_BUFFER_LEN), 0);
    EXPECT_STREQ(dirLower, "/mnt/lower/vendor");
    memset_s(dirLower, MAX_BUFFER_LEN, 0, MAX_BUFFER_LEN);
    EXPECT_EQ(ConstructOverlayLowerPath("/usr", dirLower, MAX_BUFFER_LEN), 0);
    EXPECT_STREQ(dirLower, "/system");
    memset_s(dirLower, MAX_BUFFER_LEN, 0, MAX_BUFFER_LEN);
    EXPECT_EQ(ConstructOverlayLowerPath("/", dirLower, MAX_BUFFER_LEN), 0);
    EXPECT_STREQ(dirLower, "/mnt/lower/");
}

HWTEST_F(RemountOverlayUnitTest, Init_ConstructOverlayMntOpt_001, TestSize.Level0)
{
    char mntOpt[MAX_BUFFER_LEN] = {0};
    EXPECT_EQ(ConstructOverlayMntOpt("/vendor", PREFIX_OVERLAY, mntOpt, MAX_BUFFER_LEN), 0);
    EXPECT_TRUE(strstr(mntOpt, "upperdir=") != nullptr);
    EXPECT_TRUE(strstr(mntOpt, "lowerdir=") != nullptr);
    EXPECT_TRUE(strstr(mntOpt, "workdir=") != nullptr);
}

HWTEST_F(RemountOverlayUnitTest, Init_ConstructOverlayMntOpt_002_usr, TestSize.Level1)
{
    char mntOpt[MAX_BUFFER_LEN] = {0};
    EXPECT_EQ(ConstructOverlayMntOpt("/usr", PREFIX_OVERLAY, mntOpt, MAX_BUFFER_LEN), 0);
    EXPECT_TRUE(strstr(mntOpt, "lowerdir=/system") != nullptr);
}

HWTEST_F(RemountOverlayUnitTest, Init_AllocDmName_001, TestSize.Level0)
{
    char nameRofs[MAX_BUFFER_LEN] = {0};
    char nameExt4[MAX_BUFFER_LEN] = {0};
    AllocDmName("/vendor", nameRofs, MAX_BUFFER_LEN, nameExt4, MAX_BUFFER_LEN);
    EXPECT_STREQ(nameRofs, "_vendor_erofs");
    EXPECT_STREQ(nameExt4, "_vendor_ext4");
}

HWTEST_F(RemountOverlayUnitTest, Init_AllocDmName_002_noSlash, TestSize.Level1)
{
    char nameRofs[MAX_BUFFER_LEN] = {0};
    char nameExt4[MAX_BUFFER_LEN] = {0};
    AllocDmName("vendor", nameRofs, MAX_BUFFER_LEN, nameExt4, MAX_BUFFER_LEN);
    EXPECT_STREQ(nameRofs, "vendor_erofs");
    EXPECT_STREQ(nameExt4, "vendor_ext4");
}

HWTEST_F(RemountOverlayUnitTest, Init_ConstructLinearTarget_001_null, TestSize.Level0)
{
    EXPECT_EQ(ConstructLinearTarget(nullptr, "/dev/test", 0, 4096), -1);
    EXPECT_EQ(ConstructLinearTarget(reinterpret_cast<DmVerityTarget*>(1), nullptr, 0, 4096), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_ConstructLinearTarget_002_success, TestSize.Level1)
{
    DmVerityTarget target = {0};
    EXPECT_EQ(ConstructLinearTarget(&target, "/dev/test", 0, MOCK_MAP_LENGTH), 0);
    EXPECT_TRUE(target.paras != nullptr);
    DestoryLinearTarget(&target);
}

HWTEST_F(RemountOverlayUnitTest, Init_DestoryLinearTarget_001_null, TestSize.Level0)
{
    DestoryLinearTarget(nullptr);
    DmVerityTarget target = {0};
    DestoryLinearTarget(&target);
    EXPECT_EQ(target.paras, nullptr);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsOverlayEnable_001_cmdlineFail, TestSize.Level0)
{
    RemountResetCmdlineParams();
    RemountSetStubResult(STUB_GET_EXACT_PARAM_FROM_CMDLINE, -1);
    bool ret = __real_IsOverlayEnable();
    EXPECT_EQ(ret, false);
    RemountSetStubResult(STUB_GET_EXACT_PARAM_FROM_CMDLINE, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsOverlayEnable_002_oemModeUser, TestSize.Level1)
{
    RemountResetCmdlineParams();
    RemountSetCmdlineParam("oemmode", "user", 0);
    RemountSetCmdlineParam("buildvariant", "eng", 0);
    bool ret = __real_IsOverlayEnable();
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsOverlayEnable_003_buildvariantNotEng, TestSize.Level1)
{
    RemountResetCmdlineParams();
    RemountSetCmdlineParam("oemmode", "eng", 0);
    RemountSetCmdlineParam("buildvariant", "userdebug", 0);
    bool ret = __real_IsOverlayEnable();
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsOverlayEnable_004_overlayEnable, TestSize.Level1)
{
    RemountResetCmdlineParams();
    RemountSetCmdlineParam("oemmode", "eng", 0);
    RemountSetCmdlineParam("buildvariant", "eng", 0);
    bool ret = __real_IsOverlayEnable();
    EXPECT_EQ(ret, true);
}

HWTEST_F(RemountOverlayUnitTest, Init_CheckIsExt4_real_001_fail, TestSize.Level0)
{
    bool ret = __real_CheckIsExt4("/dev/nonexistent", 0);
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_MkdirExt4OverlayDirs_001, TestSize.Level0)
{
    RemountSetStubResult(STUB_MKDIR, 0);
    int ret = MkdirExt4OverlayDirs("/vendor");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_MkdirExt4OverlayDirs_002_mkdirFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_MKDIR, -1);
    int ret = MkdirExt4OverlayDirs("/vendor");
    EXPECT_NE(ret, 0);
    RemountSetStubResult(STUB_MKDIR, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_MkdirExt4UpperWorkDirs_001, TestSize.Level0)
{
    RemountSetStubResult(STUB_MKDIR, 0);
    int ret = MkdirExt4UpperWorkDirs("/vendor");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_MkdirExt4UpperWorkDirs_002_mkdirFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_MKDIR, -1);
    int ret = MkdirExt4UpperWorkDirs("/vendor");
    EXPECT_NE(ret, 0);
    RemountSetStubResult(STUB_MKDIR, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_Modem2Exchange_001, TestSize.Level1)
{
    RemountSetStubResult(STUB_MOUNT, -1);
    std::string modemPath = std::string(STARTUP_INIT_UT_PATH) + "/vendor/modem/modem_driver";
    std::string exchangePath = std::string(STARTUP_INIT_UT_PATH) + "/mnt/driver_exchange";
    int ret = Modem2Exchange(modemPath.c_str(), exchangePath.c_str());
    EXPECT_NE(ret, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_Exchange2Modem_001_noExchange, TestSize.Level1)
{
    std::string modemPath = std::string(STARTUP_INIT_UT_PATH) + "/vendor/modem/modem_driver";
    std::string exchangePath = std::string(STARTUP_INIT_UT_PATH) + "/mnt/driver_exchange_nonexist";
    int ret = Exchange2Modem(modemPath.c_str(), exchangePath.c_str());
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_LookupErofsEnd_001_fail, TestSize.Level0)
{
    uint64_t ret = LookupErofsEnd("/dev/nonexistent_device");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetImgSize_001_fail, TestSize.Level0)
{
    uint64_t ret = GetImgSize("/dev/nonexistent_device", 0);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetBlockSize_001_fail, TestSize.Level0)
{
    uint64_t ret = GetBlockSize("/dev/nonexistent_device");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetMapperAddr_001_fail, TestSize.Level0)
{
    uint64_t start = 0;
    uint64_t length = 0;
    int ret = GetMapperAddr("/dev/nonexistent_device", &start, &length);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetMapperAddrForMerge_001_fail, TestSize.Level0)
{
    uint64_t start = 0;
    uint64_t length = 0;
    int ret = GetMapperAddrForMerge("/dev/nonexistent_device", &start, &length);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_CheckIsErofs_001_fail, TestSize.Level0)
{
    bool ret = CheckIsErofs("/dev/nonexistent_device");
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_EngFilesOverlay_003_symlink, TestSize.Level1)
{
    std::string source = std::string(STARTUP_INIT_UT_PATH) + "/data/eng_test_symlink";
    std::string linkTarget = source + "/linkfile";
    std::string regFile = source + "/realfile";
    CheckAndCreateDir(source.c_str());
    CreateTestFile(regFile.c_str(), "test");
    symlink(regFile.c_str(), linkTarget.c_str());
    EngFilesOverlay(source.c_str(), "/");
    remove(linkTarget.c_str());
    remove(regFile.c_str());
    remove(source.c_str());
    EXPECT_EQ(RemountGetStubResult(STUB_MOUNT), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_005_dmMergeEnabledSucc, TestSize.Level1)
{
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_REMOUNT_ENABLED, 1);
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_COLLECT_DM_MERGE_TARGETS, 0);
    RemountSetStubResult(STUB_FS_DM_CREATE_MULTI_TARGET, 0);
    RemountSetStubResult(STUB_FS_DM_INIT_DM_DEV, 0);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    __real_mkdir("/mnt/overlay_merge", 0755);
    int ret = RemountRofsOverlay();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_IS_DM_MERGE_REMOUNT_ENABLED, 0);
    unlink("/mnt/overlay_merge/.dm_merge");
    rmdir("/mnt/overlay_merge");
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_006_dmMergeFailFallback, TestSize.Level1)
{
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_REMOUNT_ENABLED, 1);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(nullptr, 0);
    int ret = RemountRofsOverlay();
    EXPECT_NE(ret, REMOUNT_FAIL);
    RemountSetStubResult(STUB_IS_DM_MERGE_REMOUNT_ENABLED, 0);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_DoOverlayMount_001_usr, TestSize.Level1)
{
    RemountSetStubResult(STUB_MOUNT, 0);
    char mntOpt[MAX_BUFFER_LEN] = {0};
    ConstructOverlayMntOpt("/usr", PREFIX_OVERLAY, mntOpt, MAX_BUFFER_LEN);
    int ret = DoOverlayMount("/usr", mntOpt);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoOverlayMount_002_otherMountFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_MOUNT, -1);
    char mntOpt[MAX_BUFFER_LEN] = {0};
    ConstructOverlayMntOpt("/vendor", PREFIX_OVERLAY, mntOpt, MAX_BUFFER_LEN);
    int ret = DoOverlayMount("/vendor", mntOpt);
    EXPECT_NE(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_DoOverlayMount_003_success, TestSize.Level1)
{
    RemountSetStubResult(STUB_MOUNT, 0);
    char mntOpt[MAX_BUFFER_LEN] = {0};
    ConstructOverlayMntOpt("/vendor", PREFIX_OVERLAY, mntOpt, MAX_BUFFER_LEN);
    int ret = DoOverlayMount("/vendor", mntOpt);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountOverlay_001_notActive, TestSize.Level1)
{
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    int ret = RemountOverlay();
    EXPECT_EQ(ret, 0);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountOverlay_002_overlayFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
    RemountSetStubResult(STUB_MOUNT, -1);
    int ret = RemountOverlay();
    EXPECT_EQ(ret, -1);
    RemountSetStubResult(STUB_MOUNT, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetFsSize_001_fail, TestSize.Level0)
{
    uint64_t ret = GetFsSize(-1);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetDevSize_real_001_openFail, TestSize.Level0)
{
    uint64_t devSize = 0;
    int ret = __real_GetDevSize("/nonexistent_device", &devSize);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_GetDevSize_real_002_ioctlFail, TestSize.Level1)
{
    std::string tmpFile = "/tmp/test_getdevsize_ioctl";
    CreateTestFile(tmpFile.c_str(), "test");
    uint64_t devSize = 0;
    int ret = __real_GetDevSize(tmpFile.c_str(), &devSize);
    EXPECT_EQ(ret, -1);
    unlink(tmpFile.c_str());
}

HWTEST_F(RemountOverlayUnitTest, Init_NormalizeDmName_001, TestSize.Level0)
{
    char dmName[MAX_BUFFER_LEN] = {0};
    int snprintfRet = snprintf_s(dmName, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "/vendor_erofs");
    ASSERT_GE(snprintfRet, 0);
    NormalizeDmName(dmName);
    EXPECT_STREQ(dmName, "_vendor_erofs");
    memset_s(dmName, MAX_BUFFER_LEN, 0, MAX_BUFFER_LEN);
    snprintfRet = snprintf_s(dmName, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "vendor_erofs");
    ASSERT_GE(snprintfRet, 0);
    NormalizeDmName(dmName);
    EXPECT_STREQ(dmName, "vendor_erofs");
}

HWTEST_F(RemountOverlayUnitTest, Init_RemoveDirContent_001, TestSize.Level0)
{
    std::string testDir = "/tmp/test_rmcontent_001";
    std::string subDir = testDir + "/subdir";
    std::string regFile = testDir + "/regfile";
    std::string subFile = subDir + "/subfile";
    mkdir(testDir.c_str(), 0755);
    mkdir(subDir.c_str(), 0755);
    CreateTestFile(regFile.c_str(), "test");
    CreateTestFile(subFile.c_str(), "sub");
    int ret = RemoveDirContent(testDir.c_str());
    EXPECT_EQ(ret, -1);
    rmdir(subDir.c_str());
    rmdir(testDir.c_str());
}

HWTEST_F(RemountOverlayUnitTest, Init_RemoveDirContent_002_notExist, TestSize.Level1)
{
    int ret = RemoveDirContent("/nonexistent_dir");
    EXPECT_EQ(ret, -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemoveOneErofsDmDevice_001_skip, TestSize.Level0)
{
    FstabItem item = {0};
    char mountPoint[] = "/data";
    item.mountPoint = mountPoint;
    RemoveOneErofsDmDevice(&item);
    EXPECT_EQ(RemountGetStubResult(STUB_FS_DM_REMOVE_DEVICE), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_CollectOneDmMergeTarget_001_fail, TestSize.Level0)
{
    FstabItem item = {0};
    char deviceName[] = "/dev/nonexistent";
    char mountPoint[] = "/vendor";
    item.deviceName = deviceName;
    item.mountPoint = mountPoint;
    DmLinearTarget targets[MAX_OVERLAY_PATHS] = {{0}};
    char mntPaths[MAX_OVERLAY_PATHS][MAX_BUFFER_LEN] = {{0}};
    DmMergeCollectCtx ctx = {
        .targets = targets,
        .mntPaths = mntPaths,
        .num = 0,
        .sectorOffset = 0,
        .seenDevices = {nullptr},
        .seenCount = 0,
    };
    int ret = CollectOneDmMergeTarget(&item, &ctx);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_001_remountSkip, TestSize.Level0)
{
    const char *argv[] = {"remount", nullptr};
    RemountSetStubResult(STUB_GETUID, MOCK_NON_ROOT_UID);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 0);
    int ret = RemountMainEntry(1, argv);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_002_clearSkip, TestSize.Level0)
{
    const char *argv[] = {"remount", "-c", nullptr};
    RemountSetStubResult(STUB_GETUID, MOCK_NON_ROOT_UID);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    int ret = RemountMainEntry(2, argv);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_003_clearRet1, TestSize.Level1)
{
    const char *argv[] = {"remount", "-c", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
    RemountSetStubResult(STUB_REBOOT, 0);
    RemountSetStubFstab(nullptr);
    int ret = RemountMainEntry(2, argv);
    EXPECT_GE(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_004_remountRootOverlay, TestSize.Level1)
{
    const char *argv[] = {"remount", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(nullptr, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubFstab(nullptr);
    int ret = RemountMainEntry(1, argv);
    EXPECT_NE(ret, REMOUNT_FAIL);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_005_remountNoOverlay, TestSize.Level1)
{
    const char *argv[] = {"remount", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 0);
    int ret = RemountMainEntry(1, argv);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_006_clearFailed, TestSize.Level1)
{
    const char *argv[] = {"remount", "-c", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    RemountSetStubResult(STUB_REBOOT, 0);
    RemountSetStubFstab(nullptr);
    DeleteRemountResultFlag();
    int ret = RemountMainEntry(2, argv);
    EXPECT_GE(ret, -1);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_007_clearSucc, TestSize.Level1)
{
    const char *argv[] = {"remount", "-c", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    RemountSetStubResult(STUB_REBOOT, 0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_MKDIR, 0);
    __real_mkdir("/mnt/overlay_merge", 0755);
    DeleteRemountResultFlag();
    int ret = RemountMainEntry(2, argv);
    EXPECT_EQ(ret, 0);
    unlink("/mnt/overlay_merge/.dm_merge_cleanup");
    rmdir("/mnt/overlay_merge");
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_007_dmMergeAlreadyActive, TestSize.Level1)
{
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    int ret = RemountRofsOverlay();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatExt4_005_e2fsdroidFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    int waitpidSeq[] = {0, 256};
    RemountSetWaitpidSequence(waitpidSeq, 2);
    int ret = FormatExt4("/dev/block/test", "/mnt");
    EXPECT_EQ(ret, 0);
    RemountSetWaitpidSequence(nullptr, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_004_snprintfFail, TestSize.Level1)
{
    RemountSetSprintfFailAfter(0);
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(RootOverlaySetup(), -1);
    RemountSetSprintfFailAfter(-1);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_005_upperMkdirFail, TestSize.Level1)
{
    int mkdirSeq[] = {1, -1};
    RemountSetMkdirSequence(mkdirSeq, 2);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(RootOverlaySetup(), -1);
    RemountSetMkdirSequence(nullptr, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_006_workMkdirFail, TestSize.Level1)
{
    int mkdirSeq[] = {1, 1, -1};
    RemountSetMkdirSequence(mkdirSeq, 3);
    RemountSetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(RootOverlaySetup(), -1);
    RemountSetMkdirSequence(nullptr, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_EngFilesOverlay_004_targetNotRegular, TestSize.Level1)
{
    std::string source = "/tmp/eng_340test";
    std::string target = "/tmp/eng_340tgt";
    std::string srcFile = source + "/testfile";
    std::string tgtSubPath = target + "/eng_340test/testfile";
    __real_mkdir(source.c_str(), 0755);
    __real_mkdir(target.c_str(), 0755);
    __real_mkdir((target + "/eng_340test").c_str(), 0755);
    __real_mkdir(tgtSubPath.c_str(), 0755);
    CreateTestFile(srcFile.c_str(), "test");
    RemountSetStubResult(STUB_MKDIR, 0);
    EngFilesOverlay(source.c_str(), target.c_str());
    unlink(srcFile.c_str());
    rmdir(tgtSubPath.c_str());
    rmdir((target + "/eng_340test").c_str());
    rmdir(target.c_str());
    rmdir(source.c_str());
    EXPECT_EQ(RemountGetStubResult(STUB_MOUNT), -1);
}

HWTEST_F(RemountOverlayUnitTest, Init_RootOverlaySetup_001_snprintfFail, TestSize.Level1)
{
    RemountSetSnprintfFailAfter(0);
    RemountSetStubResult(STUB_MOUNT, -1);
    RemountSetStubResult(STUB_MKDIR, 0);
    int ret = RootOverlaySetup();
    EXPECT_EQ(ret, -1);
    RemountSetSnprintfFailAfter(-1);
}

HWTEST_F(RemountOverlayUnitTest, Init_PerPartitionRemountFallback_002_systemRemount, TestSize.Level1)
{
    struct mntent entries[2];
    memset_s(entries, sizeof(entries), 0, sizeof(entries));
    entries[0].mnt_fsname = const_cast<char*>("/dev/block/dm-0");
    entries[0].mnt_dir = const_cast<char*>("/");
    entries[0].mnt_type = const_cast<char*>("erofs");
    entries[0].mnt_opts = const_cast<char*>("ro");
    entries[1].mnt_fsname = const_cast<char*>("/dev/block/dm-1");
    entries[1].mnt_dir = const_cast<char*>("/vendor");
    entries[1].mnt_type = const_cast<char*>("erofs");
    entries[1].mnt_opts = const_cast<char*>("ro");

    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(entries, 2);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 1);
    RemountSetStubResult(STUB_MOUNT_EXT4_DEVICE, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    int ret = PerPartitionRemountFallback();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubMntEntries(nullptr, 0);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_EngFilesOverlay_006_fifo, TestSize.Level1)
{
    std::string source = std::string(STARTUP_INIT_UT_PATH) + "/data/eng_test_fifo";
    std::string fifoPath = source + "/testfifo";
    CheckAndCreateDir(source.c_str());
    mkfifo(fifoPath.c_str(), 0666);
    EngFilesOverlay(source.c_str(), "/");
    unlink(fifoPath.c_str());
    remove(source.c_str());
    EXPECT_EQ(RemountGetStubResult(STUB_MOUNT), 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_008_clearDmMergeSucc, TestSize.Level1)
{
    const char *argv[] = {"remount", "-c", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    RemountSetStubResult(STUB_REBOOT, 0);
    RemountSetStubResult(STUB_MKDIR, 0);
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    __real_mkdir("/mnt/overlay_merge", 0755);
    int ret = RemountMainEntry(2, argv);
    EXPECT_EQ(ret, 0);
    unlink("/mnt/overlay_merge/.dm_merge_cleanup");
    rmdir("/mnt/overlay_merge");
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_009_invalidArgWithExtraArg, TestSize.Level1)
{
    const char *argv[] = {"remount", "-c", "a", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    int ret = RemountMainEntry(3, argv);
    EXPECT_EQ(ret, REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_010_invalidArgUnknownOption, TestSize.Level1)
{
    const char *argv[] = {"remount", "-x", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    int ret = RemountMainEntry(REMOUNT_CLEAR_ARGC, argv);
    EXPECT_EQ(ret, REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountMain_011_invalidArgMultipleArgs, TestSize.Level1)
{
    const char *argv[] = {"remount", "foo", "bar", nullptr};
    RemountSetStubResult(STUB_GETUID, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    int ret = RemountMainEntry(3, argv);
    EXPECT_EQ(ret, REMOUNT_FAIL);
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_008_dmMergeMarkerExists, TestSize.Level1)
{
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    RemountSetStubResult(STUB_ACCESS, 0);
    int ret = RemountRofsOverlay();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
    RemountSetStubResult(STUB_ACCESS, 0);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountOverlayUnitTest, Init_RemountRofsOverlay_009_dmMergeMarkerMissing, TestSize.Level1)
{
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    RemountSetStubResult(STUB_ACCESS, -1);
    RemountSetStubResult(STUB_SETMNTENT, 0);
    RemountSetStubMntEntries(nullptr, 0);
    int ret = RemountRofsOverlay();
    EXPECT_NE(ret, REMOUNT_FAIL);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
    RemountSetStubResult(STUB_ACCESS, 0);
    DeleteRemountResultFlag();
}
}
