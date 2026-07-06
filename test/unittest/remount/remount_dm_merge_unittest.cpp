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

#include <gtest/gtest.h>
#include "init_utils.h"
#include "remount_overlay.h"
#include "dm_merge_overlay.h"
#include "erofs_remount_overlay.h"
#include "erofs_overlay_common.h"
#include "remount_stub.h"
#include "fs_manager/fs_manager.h"
#include "fs_dm_linear.h"
#include <sys/stat.h>
#include <string>

using namespace std;
using namespace testing::ext;

#ifdef __cplusplus
extern "C" {
#endif
const char *PartNameToOverlayPath(const char *partName);
int ParseRefreshPartNames(const char *buf, const char *overlayPaths[], int *overlayCount);
bool HasCleanupMarker(void);
void CleanupPerPartitionOverlay(void);
int __real_mkdir(const char *pathname, mode_t mode);  // NOLINT
#ifdef __cplusplus
}
#endif

namespace init_ut {
static constexpr int MOCK_FORK_PID = 1000;
static constexpr int MAX_OVERLAY_PATHS = 8;

class RemountDmMergeUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) { RemountResetAllStubs(); };
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_001_fstabNull, TestSize.Level0)
{
    RemountSetStubResult(STUB_MOUNT, -1);
    RemountSetStubFstab(nullptr);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_002_collectFail, TestSize.Level1)
{
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = TryDmMergeRemount();
    EXPECT_NE(ret, REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_003_fstabNotNull, TestSize.Level1)
{
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_COLLECT_DM_MERGE_TARGETS, -1);
    int ret = TryDmMergeRemount();
    EXPECT_NE(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_004_createDeviceFail, TestSize.Level1)
{
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_COLLECT_DM_MERGE_TARGETS, 0);
    RemountSetStubResult(STUB_FS_DM_CREATE_MULTI_TARGET, -1);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_005_setupFail, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_DEV_SIZE, -1);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_006_success, TestSize.Level1)
{
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
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
    unlink("/mnt/overlay_merge/.dm_merge");
    rmdir("/mnt/overlay_merge");
    DeleteRemountResultFlag();
}

HWTEST_F(RemountDmMergeUnitTest, Init_ClearDmMerge_001_notActive, TestSize.Level0)
{
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_REBOOT, 0);
    int ret = ClearDmMerge();
    EXPECT_GE(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_ClearDmMerge_002_resultFlagSet_openFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_ACCESS, -1);
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    RemountSetStubResult(STUB_REBOOT, 0);
    int ret = ClearDmMerge();
    EXPECT_EQ(ret, 1);
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_ACCESS, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_ClearDmMerge_003_openSucc, TestSize.Level1)
{
    RemountSetStubResult(STUB_ACCESS, -1);
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 1);
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_REBOOT, 0);
    CheckAndCreateDir("/mnt/overlay_merge/");
    int ret = ClearDmMerge();
    EXPECT_EQ(ret, 0);
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_IS_DM_MERGE_OVERLAY_ACTIVE, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_IsDmMergeOverlayActive_001, TestSize.Level0)
{
    bool ret = IsDmMergeOverlayActive();
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountDmMergeUnitTest, Init_IsDmMergeRemountEnabled_001, TestSize.Level0)
{
    bool ret = IsDmMergeRemountEnabled();
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountDmMergeUnitTest, Init_IsHyperholdEnableMarkerSet_001, TestSize.Level0)
{
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 1);
    bool ret = IsHyperholdEnableMarkerSet();
    EXPECT_EQ(ret, true);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_CheckHyperholdDisableMarker_001, TestSize.Level0)
{
    int ret = CheckHyperholdDisableMarker();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_CheckDmMergeCleanup_001, TestSize.Level0)
{
    int ret = CheckDmMergeCleanup();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_MountDmMergeOverlayAll_001, TestSize.Level0)
{
    RemountSetStubResult(STUB_MOUNT, -1);
    int ret = MountDmMergeOverlayAll();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_MountDmMergeOverlayAll_002, TestSize.Level1)
{
    int ret = MountDmMergeOverlayAll();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_OverlayPathFromMnt_001, TestSize.Level0)
{
    const char *path = OverlayPathFromMnt("/vendor");
    EXPECT_NE(path, nullptr);
    path = OverlayPathFromMnt("/system");
    EXPECT_NE(path, nullptr);
    path = OverlayPathFromMnt("/usr");
    EXPECT_NE(path, nullptr);
    path = OverlayPathFromMnt("/test");
    EXPECT_NE(path, nullptr);
}

HWTEST_F(RemountDmMergeUnitTest, Init_AlignDown_001, TestSize.Level0)
{
    EXPECT_EQ(AlignDown(4096, 4096), 4096);
    EXPECT_EQ(AlignDown(4097, 4096), 4096);
    EXPECT_EQ(AlignDown(0, 4096), 0);
    EXPECT_EQ(AlignDown(8191, 4096), 4096);
}

HWTEST_F(RemountDmMergeUnitTest, Init_IsDeviceNameSeen_001, TestSize.Level0)
{
    const char *seenDevices[] = {"dm-0", "dm-1"};
    EXPECT_EQ(IsDeviceNameSeen(seenDevices, 2, "dm-0"), true);
    EXPECT_EQ(IsDeviceNameSeen(seenDevices, 2, "dm-2"), false);
    EXPECT_EQ(IsDeviceNameSeen(seenDevices, 0, "dm-0"), false);
}

HWTEST_F(RemountDmMergeUnitTest, Init_GetRemountResult_001, TestSize.Level0)
{
    DeleteRemountResultFlag();
    int ret = GetRemountResult();
    EXPECT_NE(ret, REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, Init_SetRemountResultFlag_001, TestSize.Level1)
{
    RemountSetStubResult(STUB_ACCESS, -1);
    CheckAndCreateDir((std::string(STARTUP_INIT_UT_PATH) + "/data/service/el1/startup/remount/").c_str());
    SetRemountResultFlag();
    int ret = GetRemountResult();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    DeleteRemountResultFlag();
    RemountSetStubResult(STUB_ACCESS, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_MountOverlayOne_001, TestSize.Level1)
{
    RemountSetStubResult(STUB_MOUNT, -1);
    int ret = MountOverlayOne("/vendor", PREFIX_OVERLAY);
    EXPECT_NE(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_MountOverlayOne_002, TestSize.Level1)
{
    RemountSetStubResult(STUB_MOUNT, 0);
    int ret = MountOverlayOne("/vendor", PREFIX_OVERLAY);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_PartNameToOverlayPath_001, TestSize.Level0)
{
    EXPECT_STREQ(PartNameToOverlayPath("system"), "/usr");
    EXPECT_STREQ(PartNameToOverlayPath("vendor"), "/vendor");
    EXPECT_STREQ(PartNameToOverlayPath("sys_prod"), "/sys_prod");
    EXPECT_STREQ(PartNameToOverlayPath("chip_prod"), "/chip_prod");
    EXPECT_EQ(PartNameToOverlayPath("unknown"), nullptr);
}

HWTEST_F(RemountDmMergeUnitTest, Init_ParseRefreshPartNames_001, TestSize.Level0)
{
    const char *overlayPaths[MAX_OVERLAY_PATHS] = {nullptr};
    int overlayCount = 0;
    EXPECT_EQ(ParseRefreshPartNames("vendor,system", overlayPaths, &overlayCount), 0);
    EXPECT_EQ(overlayCount, 2);
}

HWTEST_F(RemountDmMergeUnitTest, Init_ParseRefreshPartNames_002_withSemicolon, TestSize.Level1)
{
    const char *overlayPaths[MAX_OVERLAY_PATHS] = {nullptr};
    int overlayCount = 0;
    EXPECT_EQ(ParseRefreshPartNames("vendor,system;refresh_data", overlayPaths, &overlayCount), 0);
    EXPECT_EQ(overlayCount, 2);
}

HWTEST_F(RemountDmMergeUnitTest, Init_ParseRefreshPartNames_003_empty, TestSize.Level1)
{
    const char *overlayPaths[MAX_OVERLAY_PATHS] = {nullptr};
    int overlayCount = 0;
    EXPECT_EQ(ParseRefreshPartNames("", overlayPaths, &overlayCount), 0);
    EXPECT_EQ(overlayCount, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_HasCleanupMarker_001, TestSize.Level0)
{
    FILE *tmpFile = fopen("/mnt/overlay_merge/.dm_merge_cleanup", "w");
    if (tmpFile != nullptr) {
        fprintf(tmpFile, "1");
        fclose(tmpFile);
    }
    bool ret = HasCleanupMarker();
    EXPECT_EQ(ret, true);
    unlink("/mnt/overlay_merge/.dm_merge_cleanup");
}

HWTEST_F(RemountDmMergeUnitTest, Init_CleanupPerPartitionOverlay_001, TestSize.Level0)
{
    CleanupPerPartitionOverlay();
    EXPECT_EQ(RemountGetStubResult(STUB_GET_PARAM_FROM_CMDLINE), 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_RefreshPartitionOverlay_001, TestSize.Level0)
{
    int ret = RefreshPartitionOverlay();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_007_updaterModePath, TestSize.Level1)
{
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 1);
    RemountSetStubResult(STUB_ACCESS, -1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_ACCESS, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_008_nonUpdaterCmdlineFail, TestSize.Level1)
{
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_GET_PARAM_FROM_CMDLINE, -1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubResult(STUB_GET_PARAM_FROM_CMDLINE, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_009_bootSlotAdjust, TestSize.Level1)
{
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->deviceName = strdup("/dev/block/dm-0");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_COLLECT_DM_MERGE_TARGETS, -1);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 2);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = TryDmMergeRemount();
    EXPECT_NE(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_010_initDmDevFail, TestSize.Level1)
{
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_COLLECT_DM_MERGE_TARGETS, 0);
    RemountSetStubResult(STUB_FS_DM_CREATE_MULTI_TARGET, 0);
    RemountSetStubResult(STUB_FS_DM_INIT_DM_DEV, -1);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_011_mkdirDirPartitionFail, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    int mkdirSeq[] = {1, -1};
    RemountSetMkdirSequence(mkdirSeq, 2);
    RemountSetStubResult(STUB_MOUNT, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
    RemountSetMkdirSequence(nullptr, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_012_snprintfDirPartitionFail, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetSprintfFailAfter(1);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
    RemountSetSprintfFailAfter(-1);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_013_snprintfDirUpperFail, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetSprintfFailAfter(2);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
    RemountSetSprintfFailAfter(-1);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_014_snprintfDirWorkFail, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetSprintfFailAfter(3);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_SUCC);
    RemountSetStubFstab(nullptr);
    RemountSetSprintfFailAfter(-1);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_015_overlayMountFailRollback, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    int mountSeq[] = {0, -1};
    RemountSetMountSequence(mountSeq, 2);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
    RemountSetMountSequence(nullptr, 0);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_016_strncpyFail_updaterMode, TestSize.Level1)
{
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 1);
    RemountSetStrncpyFail(true);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStrncpyFail(false);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_017_snprintfFail_buildFstab, TestSize.Level1)
{
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_GET_PARAM_FROM_CMDLINE, 0);
    RemountSetSnprintfFailAfter(0);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_FORK, MOCK_FORK_PID);
    RemountSetStubResult(STUB_WAITPID, 0);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetSnprintfFailAfter(-1);
    RemountSetStubResult(STUB_GET_PARAM_FROM_CMDLINE, 0);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_018_snprintfFail_dirPartition, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetSnprintfFailAfter(1);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
    RemountSetSnprintfFailAfter(-1);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_019_snprintfFail_dirUpper, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetSnprintfFailAfter(2);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
    RemountSetSnprintfFailAfter(-1);
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_020_snprintfFail_rollback, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetSnprintfFailAfter(4);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
    RemountSetSnprintfFailAfter(-1);
    DeleteRemountResultFlag();
}

HWTEST_F(RemountDmMergeUnitTest, Init_TryDmMergeRemount_021_snprintfFail_dirWork, TestSize.Level1)
{
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
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_MKDIR, 1);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetSnprintfFailAfter(3);
    int ret = TryDmMergeRemount();
    EXPECT_EQ(ret, REMOUNT_FAIL);
    RemountSetStubFstab(nullptr);
    RemountSetSnprintfFailAfter(-1);
}
} // namespace init_ut
