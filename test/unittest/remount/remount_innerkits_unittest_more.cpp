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

#include "remount_innerkits_unittest_common.h"

using namespace testing::ext;

namespace init_ut {

HWTEST_F(RemountDmMergeUnitTest, RemountOverlay_LegacyOverlayPathMissingContinues, TestSize.Level1)
{
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    g_state.mockLstat = true;
    g_state.lstatRet = -1;
    g_state.mockMountOverlay = true;

    EXPECT_EQ(RemountOverlay(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, RemountOverlay_LegacyOverlayMountsAllPaths, TestSize.Level1)
{
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    g_state.mockLstat = true;
    g_state.lstatRet = 0;
    g_state.mockMountOverlay = true;

    EXPECT_EQ(RemountOverlay(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, RemountOverlay_LegacyOverlayMountFailureReturnsFail, TestSize.Level1)
{
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    g_state.mockLstat = true;
    g_state.lstatRet = 0;
    g_state.mockMountOverlay = true;
    g_state.mountOverlayRet = -1;

    EXPECT_EQ(RemountOverlay(), -1);
}

HWTEST_F(RemountDmMergeUnitTest, DmMergeHelpers_ReturnExpectedValues, TestSize.Level1)
{
    const char *seenDevices[] = {"/dev/block/dm-0", "/dev/block/dm-1"};

    EXPECT_TRUE(MntNeedRemount("/vendor"));
    EXPECT_FALSE(MntNeedRemount("/data"));
    EXPECT_STREQ(OverlayPathFromMnt("/"), "/usr");
    EXPECT_STREQ(OverlayPathFromMnt("/usr"), "/usr");
    EXPECT_STREQ(OverlayPathFromMnt("/vendor"), "/vendor");
    EXPECT_TRUE(IsDeviceNameSeen(seenDevices, 2, "/dev/block/dm-1"));
    EXPECT_FALSE(IsDeviceNameSeen(seenDevices, 2, "/dev/block/dm-2"));
    EXPECT_EQ(AlignDown(4097, 512), 4096U);
    EXPECT_EQ(AlignDown(4097, 0), 4097U);
    EXPECT_EQ(AlignTo(4097, 4096), 8192U);
    EXPECT_EQ(AlignTo(4097, 0), 4097U);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeOverlay_InvalidInputFails, TestSize.Level1)
{
    EXPECT_EQ(TryDmMergeOverlay(nullptr), -1);
    UseFirstStageDmMergeMocks();
    Fstab *fstab = BuildFstab({{"/dev/block/dm-data", "/data", "ext4"}});
    ASSERT_NE(fstab, nullptr);

    EXPECT_EQ(TryDmMergeOverlay(fstab), -1);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeOverlay_CreateDeviceFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.createDmRet = -1;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    EXPECT_EQ(TryDmMergeOverlay(fstab), -1);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeOverlay_DeviceNotReadyFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.accessRet = -1;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    EXPECT_EQ(TryDmMergeOverlay(fstab), -1);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeOverlayActive_OpenFailureReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_FALSE(IsDmMergeOverlayActive());
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeOverlayActive_DeviceNameFailureReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockDmGetDeviceName = true;
    g_state.dmGetDeviceNameRet = -1;

    EXPECT_FALSE(IsDmMergeOverlayActive());
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeOverlayActive_DeviceNameSuccessReturnsTrue, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();

    EXPECT_TRUE(IsDmMergeOverlayActive());
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeOverlayAll_InitDmFailureReturnsFail, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.initDmRet = -1;

    EXPECT_EQ(MountDmMergeOverlayAll(), -1);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeOverlayAll_MountOverlayFailureStillCompletes, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockMountOverlay = true;
    g_state.mountOverlayRet = -1;

    EXPECT_EQ(MountDmMergeOverlayAll(), 0);
    ExpectUniqueMountedOverlayPaths();
}

HWTEST_F(RemountDmMergeUnitTest, GetDmMergeDevPath_OpenFailureReturnsFail, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;
    g_state.mockDmGetDeviceName = true;
    char dmDevPath[MAX_BUFFER_LEN] = {};

    EXPECT_EQ(GetDmMergeDevPath(dmDevPath, MAX_BUFFER_LEN), -1);
}

HWTEST_F(RemountDmMergeUnitTest, GetDmMergeDevPath_DeviceNameFailureClosesFd, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockDmGetDeviceName = true;
    g_state.dmGetDeviceNameRet = -1;
    char dmDevPath[MAX_BUFFER_LEN] = {};

    EXPECT_EQ(GetDmMergeDevPath(dmDevPath, MAX_BUFFER_LEN), -1);
}

HWTEST_F(RemountDmMergeUnitTest, GetDmMergeDevPath_SuccessCopiesPathAndClosesFd, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockDmGetDeviceName = true;
    g_state.dmGetDevicePath = "/dev/block/dm-merge-test";
    char dmDevPath[MAX_BUFFER_LEN] = {};

    EXPECT_EQ(GetDmMergeDevPath(dmDevPath, MAX_BUFFER_LEN), 0);
    EXPECT_STREQ(dmDevPath, "/dev/block/dm-merge-test");
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeExt4_InitDmFailureReturnsFail, TestSize.Level1)
{
    g_state.mockInitDm = true;
    g_state.initDmRet = -1;
    char dmDevPath[] = "/dev/block/dm-merge-test";

    EXPECT_EQ(MountDmMergeExt4(dmDevPath), -1);
    EXPECT_EQ(g_mountStubCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeExt4_MkdirFailureReturnsFail, TestSize.Level1)
{
    g_state.mockInitDm = true;
    g_state.mkdirFailAt = 1;
    char dmDevPath[] = "/dev/block/dm-merge-test";

    EXPECT_EQ(MountDmMergeExt4(dmDevPath), -1);
    EXPECT_EQ(g_mountStubCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeExt4_MountFailureReturnsFail, TestSize.Level1)
{
    g_state.mockInitDm = true;
    SetStubResult(STUB_MOUNT, -1);
    char dmDevPath[] = "/dev/block/dm-merge-test";

    EXPECT_EQ(MountDmMergeExt4(dmDevPath), -1);
    EXPECT_EQ(g_mountStubCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeExt4_SuccessMountsMergeRoot, TestSize.Level1)
{
    g_state.mockInitDm = true;
    char dmDevPath[] = "/dev/block/dm-merge-test";

    EXPECT_EQ(MountDmMergeExt4(dmDevPath), 0);
    EXPECT_EQ(g_mountStubCalls, 1);
    EXPECT_STREQ(g_mountStubTarget, PREFIX_OVERLAY_MERGE);
}

HWTEST_F(RemountDmMergeUnitTest, NormalizeDmName_ReplacesSlashOnly, TestSize.Level1)
{
    char dmName[MAX_BUFFER_LEN] = "/usr_erofs";
    NormalizeDmName(dmName);
    EXPECT_STREQ(dmName, "_usr_erofs");

    char plainName[MAX_BUFFER_LEN] = "vendor_erofs";
    NormalizeDmName(plainName);
    EXPECT_STREQ(plainName, "vendor_erofs");
}

HWTEST_F(RemountDmMergeUnitTest, RemoveOneErofsDmDevice_NonRemountSkips, TestSize.Level1)
{
    g_state.mockRemoveDm = true;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-data", "/data", "ext4"});
    ASSERT_NE(item, nullptr);

    RemoveOneErofsDmDevice(item);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, RemoveOneErofsDmDevice_RemovesNormalizedName, TestSize.Level1)
{
    g_state.mockRemoveDm = true;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/usr", "erofs"});
    ASSERT_NE(item, nullptr);

    RemoveOneErofsDmDevice(item);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, RemoveAllErofsDmDevices_LoadFstabNullSkips, TestSize.Level1)
{
    g_state.mockLoadFstab = true;
    g_state.loadFstabNull = true;
    g_state.mockRemoveDm = true;

    RemoveAllErofsDmDevices();
    EXPECT_EQ(g_state.removeDmCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, RemoveAllErofsDmDevices_RemovesOnlyRemountDevices, TestSize.Level1)
{
    g_state.mockLoadFstab = true;
    g_state.mockRemoveDm = true;
    g_state.fstabEntries = {
        {"/dev/block/dm-0", "/usr", "erofs"},
        {"/dev/block/dm-data", "/data", "ext4"},
        {"/dev/block/dm-1", "/vendor", "erofs"},
    };

    RemoveAllErofsDmDevices();
    EXPECT_EQ(g_state.removeDmCalls, 2);
    ASSERT_EQ(g_state.removedDmNames.size(), 2U);
    EXPECT_EQ(g_state.removedDmNames[0], "_usr_erofs");
    EXPECT_EQ(g_state.removedDmNames[1], "_vendor_erofs");
}

HWTEST_F(RemountDmMergeUnitTest, RemoveDirContent_OpenFailureReturnsFail, TestSize.Level1)
{
    g_state.mockDirOps = true;

    EXPECT_EQ(RemoveDirContent("/mnt/overlay_merge/usr/upper"), -1);
    EXPECT_EQ(g_state.opendirCalls, 1);
    EXPECT_EQ(g_state.readdirCalls, 0);
    EXPECT_EQ(g_state.closedirCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, RemoveDirContent_RemovesFilesAndDirectories, TestSize.Level1)
{
    g_state.mockDirOps = true;
    g_state.dirOpsOpenSucceeds = true;
    g_state.mockUnlink = true;
    g_state.mockRmdir = true;
    g_state.dirEntries = {
        {".", DT_DIR},
        {"..", DT_DIR},
        {"child", DT_DIR},
        {"file", DT_REG},
    };

    EXPECT_EQ(RemoveDirContent("/mnt/overlay_merge/usr/upper"), 0);
}

HWTEST_F(RemountDmMergeUnitTest, CheckDmMergeCleanup_InactiveSkips, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_EQ(CheckDmMergeCleanup(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, CheckDmMergeCleanup_MountFailureFails, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.initDmRet = -1;

    EXPECT_EQ(CheckDmMergeCleanup(), -1);
}

HWTEST_F(RemountDmMergeUnitTest, CheckDmMergeCleanup_NoMarkerUnmountsAndSkips, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockStat = true;
    g_state.statRet = -1;

    EXPECT_EQ(CheckDmMergeCleanup(), 0);
    EXPECT_EQ(g_umount2StubCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, CheckDmMergeCleanup_MarkerPerformsCleanup, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockStat = true;
    g_state.statRet = 0;
    g_state.mockLoadFstab = true;
    g_state.loadFstabNull = true;
    g_state.mockPwrite = true;

    EXPECT_EQ(CheckDmMergeCleanup(), 1);
    EXPECT_GE(g_umount2StubCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, CheckHyperholdDisableMarker_ReadFailureSkips, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_EQ(CheckHyperholdDisableMarker(nullptr), 0);
}

HWTEST_F(RemountDmMergeUnitTest, CheckHyperholdDisableMarker_NotDisableSkips, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_ENABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_EQ(CheckHyperholdDisableMarker(nullptr), 0);
}

HWTEST_F(RemountDmMergeUnitTest, CheckHyperholdDisableMarker_InactiveCleansPerPartitionOverlay, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openSeq = {100, -1, 100};
    g_state.mockPread = true;
    g_state.mockPwrite = true;
    g_state.mockUnlink = true;
    g_state.mockDirOps = true;
    g_state.preadContent = HYPERHOLD_SWITCH_DISABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_EQ(CheckHyperholdDisableMarker(nullptr), 1);
}

HWTEST_F(RemountDmMergeUnitTest, CheckHyperholdDisableMarker_WriteEnableOpenFailureStillReturnsDone, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openSeq = {100, -1, -1};
    g_state.mockPread = true;
    g_state.mockPwrite = true;
    g_state.mockUnlink = true;
    g_state.mockDirOps = true;
    g_state.preadContent = HYPERHOLD_SWITCH_DISABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_EQ(CheckHyperholdDisableMarker(nullptr), 1);
}

HWTEST_F(RemountDmMergeUnitTest, CheckHyperholdDisableMarker_ActiveMountSuccessPerformsCleanup, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockPread = true;
    g_state.mockPwrite = true;
    g_state.preadContent = HYPERHOLD_SWITCH_DISABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());
    g_state.mockLoadFstab = true;
    g_state.loadFstabNull = true;

    EXPECT_EQ(CheckHyperholdDisableMarker(nullptr), 1);
    EXPECT_GE(g_umount2StubCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, CheckHyperholdDisableMarker_ActiveMountFailureDiscardsAndRemoves, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockPread = true;
    g_state.mockPwrite = true;
    g_state.preadContent = HYPERHOLD_SWITCH_DISABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());
    g_state.initDmRet = -1;
    g_state.mockLoadFstab = true;
    g_state.loadFstabNull = true;

    EXPECT_EQ(CheckHyperholdDisableMarker(nullptr), 1);
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_HyperholdEmptyReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadRet = 0;

    EXPECT_FALSE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_HyperholdEnableReturnsTrue, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_ENABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_TRUE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_HyperholdOtherReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_DISABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_FALSE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, RefreshPartitionOverlay_ReadFailureSkips, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_EQ(RefreshPartitionOverlay(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, RefreshPartitionOverlay_InactiveSkips, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openSeq = {100, -1};
    g_state.mockPread = true;
    g_state.preadContent = "system,vendor";
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_EQ(RefreshPartitionOverlay(), 0);
    EXPECT_EQ(g_umount2StubCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, RefreshPartitionOverlay_NoValidPartitionSkips, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockPread = true;
    g_state.preadContent = "invalid";
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_EQ(RefreshPartitionOverlay(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, RefreshPartitionOverlay_MountFailureFails, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockPread = true;
    g_state.preadContent = "system";
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());
    g_state.initDmRet = -1;

    EXPECT_EQ(RefreshPartitionOverlay(), -1);
}

HWTEST_F(RemountDmMergeUnitTest, RefreshPartitionOverlay_SuccessRemovesSelectedOverlayContent, TestSize.Level1)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockPread = true;
    g_state.mockPwrite = true;
    g_state.mockDirOps = true;
    g_state.preadContent = "system,vendor;ignored";
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_EQ(RefreshPartitionOverlay(), 0);
    EXPECT_EQ(g_umount2StubCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_ParamReadFailureDoesNotBlockHyperholdEnable, TestSize.Level1)
{
    g_state.mockSystemReadParam = true;
    g_state.systemReadParamRet = -1;
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_ENABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_TRUE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, IsHyperholdEnableMarkerSet_OpenFailureReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_FALSE(IsHyperholdEnableMarkerSet());
}

HWTEST_F(RemountDmMergeUnitTest, IsHyperholdEnableMarkerSet_EmptyContentReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockPread = true;
    g_state.preadRet = 0;

    EXPECT_FALSE(IsHyperholdEnableMarkerSet());
}

HWTEST_F(RemountDmMergeUnitTest, IsHyperholdEnableMarkerSet_EnableMarkerReturnsTrue, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_ENABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_TRUE(IsHyperholdEnableMarkerSet());
}

HWTEST_F(RemountDmMergeUnitTest, IsHyperholdEnableMarkerSet_OtherMarkerReturnsFalse, TestSize.Level1)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_DISABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_FALSE(IsHyperholdEnableMarkerSet());
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_AccessFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.accessRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_MapperFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mapperRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_ZeroErofsSizeFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mapperStart = 0;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_CreateLinearFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.createLinearRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_InitDmFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.initDmRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_PrepareMountPointFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mkdirFailAt = 1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_MountLowerDirFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mkdirFailAt = 2;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_MountLowerSubdirFailureFails, TestSize.Level1)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mkdirFailAt = 3;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, FsAdjustPartitionNameBySlot_NoSlotDeviceKeepsOriginalName, TestSize.Level1)
{
    g_state.mockGetCurrentSlot = true;
    g_state.currentSlot = 2;
    g_state.accessRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/by-name/system", "/", "erofs"});
    ASSERT_NE(item, nullptr);

    FsAdjustPartitionNameBySlot(item);

    EXPECT_STREQ(item->deviceName, "/dev/block/by-name/system");

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, FsAdjustPartitionNameBySlot_InvalidSlotUsesDefaultSlot, TestSize.Level1)
{
    g_state.mockGetCurrentSlot = true;
    g_state.currentSlot = 0;
    FstabItem *item = MakeFstabItem({"/dev/block/by-name/system", "/", "erofs"});
    ASSERT_NE(item, nullptr);

    FsAdjustPartitionNameBySlot(item);

    EXPECT_STREQ(item->deviceName, "/dev/block/by-name/system_a");

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, FsAdjustPartitionNameBySlot_StrdupFailureLeavesNullDeviceName, TestSize.Level1)
{
    g_state.mockGetCurrentSlot = true;
    g_state.currentSlot = 2;
    FstabItem *item = MakeFstabItem({"/dev/block/by-name/system", "/", "erofs"});
    ASSERT_NE(item, nullptr);
    UpdateStrdupFunc(StrdupMock);
    g_state.strdupFailAt = 1;

    FsAdjustPartitionNameBySlot(item);

    EXPECT_EQ(item->deviceName, nullptr);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DeleteRemountResultFlag_UnlinkSuccess, TestSize.Level1)
{
    g_state.mockUnlink = true;
    g_state.unlinkRet = 0;

    DeleteRemountResultFlag();

    EXPECT_EQ(g_state.unlinkCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, DeleteRemountResultFlag_UnlinkFailure, TestSize.Level1)
{
    g_state.mockUnlink = true;
    g_state.unlinkRet = -1;

    DeleteRemountResultFlag();

    EXPECT_EQ(g_state.unlinkCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, MountOverlayOne_ModemPathSkipsMount, TestSize.Level1)
{
    UpdateMountFunc(MountRecordMock);

    EXPECT_EQ(MountOverlayOne("/data/init_ut/vendor/modem/modem_driver", PREFIX_OVERLAY), 0);
    EXPECT_EQ(g_mountStubCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, MountOverlayOne_UsrUsesSystemLowerAndTarget, TestSize.Level1)
{
    UpdateMountFunc(MountRecordMock);

    EXPECT_EQ(MountOverlayOne("/usr", PREFIX_OVERLAY), 0);
    EXPECT_EQ(g_mountStubCalls, 1);
    EXPECT_STREQ(g_mountStubTarget, "/system");
    EXPECT_NE(std::string(g_mountStubData).find("lowerdir=/system"), std::string::npos);
}

HWTEST_F(RemountDmMergeUnitTest, MountOverlayOne_VendorUsesLowerMountPath, TestSize.Level1)
{
    UpdateMountFunc(MountRecordMock);

    EXPECT_EQ(MountOverlayOne("/vendor", PREFIX_OVERLAY), 0);
    EXPECT_EQ(g_mountStubCalls, 1);
    EXPECT_STREQ(g_mountStubTarget, "/vendor");
    EXPECT_NE(std::string(g_mountStubData).find("lowerdir=/mnt/lower/vendor"), std::string::npos);
}

HWTEST_F(RemountDmMergeUnitTest, CollectDmMergeTargets_SkipsDuplicateAndNonRemount, TestSize.Level1)
{
    g_state.mockMapper = true;
    g_state.mapperStart = TEST_MAPPER_START;
    g_state.mapperLength = 8192;
    Fstab *fstab = BuildFstab({
        {"/dev/block/dm-0", "/", "erofs"},
        {"/dev/block/dm-0", "/usr", "erofs"},
        {"/dev/block/dm-data", "/data", "ext4"},
        {"/dev/block/dm-1", "/vendor", "erofs"},
    });
    ASSERT_NE(fstab, nullptr);

    DmLinearTarget targets[MAX_TARGET_NUM] = {};
    char mntPaths[MAX_TARGET_NUM][MAX_BUFFER_LEN] = {};
    DmMergeCollectCtx ctx = MakeCollectCtx(targets, mntPaths);

    EXPECT_EQ(CollectDmMergeTargets(fstab, &ctx), 0);
    EXPECT_EQ(ctx.num, 2U);
    EXPECT_STREQ(mntPaths[0], "/usr");
    EXPECT_STREQ(mntPaths[1], "/vendor");

    DestroyDmMergeTargets(targets, ctx.num);
    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, CollectDmMergeTargets_MapperMissesAreSkipped, TestSize.Level1)
{
    g_state.mockMapper = true;
    g_state.mapperRet = -1;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    DmLinearTarget targets[MAX_TARGET_NUM] = {};
    char mntPaths[MAX_TARGET_NUM][MAX_BUFFER_LEN] = {};
    DmMergeCollectCtx ctx = MakeCollectCtx(targets, mntPaths);

    EXPECT_EQ(CollectDmMergeTargets(fstab, &ctx), 0);
    EXPECT_EQ(ctx.num, 0U);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, CollectDmMergeTargets_ZeroLengthIsSkipped, TestSize.Level1)
{
    g_state.mockMapper = true;
    g_state.mapperRet = 0;
    g_state.mapperLength = 0;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    DmLinearTarget targets[MAX_TARGET_NUM] = {};
    char mntPaths[MAX_TARGET_NUM][MAX_BUFFER_LEN] = {};
    DmMergeCollectCtx ctx = MakeCollectCtx(targets, mntPaths);

    EXPECT_EQ(CollectDmMergeTargets(fstab, &ctx), 0);
    EXPECT_EQ(ctx.num, 0U);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, CollectDmMergeTargets_AlignedZeroLengthIsSkipped, TestSize.Level1)
{
    g_state.mockMapper = true;
    g_state.mapperRet = 0;
    g_state.mapperLength = SECTOR_SIZE - 1;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    DmLinearTarget targets[MAX_TARGET_NUM] = {};
    char mntPaths[MAX_TARGET_NUM][MAX_BUFFER_LEN] = {};
    DmMergeCollectCtx ctx = MakeCollectCtx(targets, mntPaths);

    EXPECT_EQ(CollectDmMergeTargets(fstab, &ctx), 0);
    EXPECT_EQ(ctx.num, 0U);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, CollectDmMergeTargets_TargetLimitFails, TestSize.Level1)
{
    g_state.mockMapper = true;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    DmLinearTarget target = {};
    char mntPaths[1][MAX_BUFFER_LEN] = {};
    DmMergeCollectCtx ctx = MakeCollectCtx(&target, mntPaths);
    ctx.num = MAX_TARGET_NUM;

    EXPECT_EQ(CollectDmMergeTargets(fstab, &ctx), -1);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, CollectDmMergeTargets_CallocFailureFails, TestSize.Level1)
{
    g_state.mockMapper = true;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);
    UpdateCallocFunc(CallocMock);
    g_state.callocFailAt = 1;

    DmLinearTarget targets[MAX_TARGET_NUM] = {};
    char mntPaths[MAX_TARGET_NUM][MAX_BUFFER_LEN] = {};
    DmMergeCollectCtx ctx = MakeCollectCtx(targets, mntPaths);

    EXPECT_EQ(CollectDmMergeTargets(fstab, &ctx), -1);
    EXPECT_EQ(ctx.num, 0U);

    ReleaseFstab(fstab);
}
} // namespace init_ut
