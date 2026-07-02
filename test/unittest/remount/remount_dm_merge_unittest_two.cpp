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

#include "remount_dm_merge_unittest_common.h"

using namespace testing::ext;

namespace init_ut {
HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_InactiveFails, TestSize.Level0)
{
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);
    EXPECT_EQ(g_state.createLinearCalls, 0);

    ReleaseFstabItem(item);
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
    EXPECT_EQ(g_state.mapperCalls, 0);
    EXPECT_EQ(g_state.createLinearCalls, 0);

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
    EXPECT_EQ(g_state.mapperCalls, 1);
    EXPECT_EQ(g_state.createLinearCalls, 0);

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
    EXPECT_EQ(g_state.mapperCalls, 1);
    EXPECT_EQ(g_state.createLinearCalls, 0);

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
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 0);

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
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.waitForFileCalls, 0);
    EXPECT_EQ(g_state.mkdirCalls, 0);

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
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.mkdirCalls, 1);
    EXPECT_EQ(g_state.waitForFileCalls, 0);

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
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.waitForFileCalls, 1);
    EXPECT_EQ(g_state.mkdirCalls, 2);

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
    EXPECT_EQ(g_state.waitForFileCalls, 1);
    EXPECT_EQ(g_state.mkdirCalls, 3);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_SuccessMountsRofsAndLower, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), 0);
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.waitForFileCalls, 1);
    EXPECT_GE(g_state.mkdirCalls, 3);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_UsrSuccessSwitchesRoot, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockSwitchRoot = true;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/usr", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), 0);
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.switchRootCalls, 1);
    EXPECT_GE(g_state.mkdirCalls, 3);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, MountItemByFsType_OverlayDisabledFallsBackToPlainMount, TestSize.Level0)
{
    g_state.mockCheckErofs = true;
    g_state.checkErofsRet = true;
    g_state.mockOverlayEnable = true;
    g_state.overlayEnableRet = false;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockDoMountOverlayDevice = true;
    g_state.mockStat = true;
    g_state.statRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);
    MountResult result = {};

    EXPECT_EQ(MountItemByFsType(item, &result), 0);
    EXPECT_EQ(g_state.checkErofsCalls, 1);
    EXPECT_EQ(g_state.overlayEnableCalls, 1);
    EXPECT_EQ(g_mountStubCalls, 1);
    EXPECT_EQ(g_state.doMountOverlayDeviceCalls, 0);
    EXPECT_EQ(g_state.createLinearCalls, 0);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, MountErofsOverlayItem_DmMergeActiveUsesErofsOnlyPath, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.mockOverlayEnable = true;
    g_state.overlayEnableRet = true;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockDoMountOverlayDevice = true;
    g_state.doMountOverlayDeviceRet = -1;
    g_state.mockStat = true;
    g_state.statRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(MountErofsOverlayItem(item), 0);
    EXPECT_EQ(g_state.checkErofsCalls, 0);
    EXPECT_EQ(g_state.overlayEnableCalls, 1);
    EXPECT_EQ(g_state.mapperCalls, 1);
    EXPECT_EQ(g_state.createLinearCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.waitForFileCalls, 1);
    EXPECT_GE(g_state.mkdirCalls, 3);
    EXPECT_EQ(g_mountStubCalls, 2);
    EXPECT_EQ(g_state.statCalls, 0);
    EXPECT_EQ(g_state.doMountOverlayDeviceCalls, 0);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, MountErofsOverlayItem_DmMergeInactiveUsesLegacyOverlayPath, TestSize.Level0)
{
    g_state.mockOverlayEnable = true;
    g_state.overlayEnableRet = true;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    g_state.mockDoMountOverlayDevice = true;
    g_state.doMountOverlayDeviceRet = 0;
    g_state.mockStat = true;
    g_state.statRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(MountErofsOverlayItem(item), 0);
    EXPECT_EQ(g_state.checkErofsCalls, 0);
    EXPECT_EQ(g_state.overlayEnableCalls, 1);
    EXPECT_EQ(g_state.doMountOverlayDeviceCalls, 1);
    EXPECT_EQ(g_state.createLinearCalls, 0);
    EXPECT_EQ(g_state.initDmCalls, 0);
    EXPECT_EQ(g_state.waitForFileCalls, 0);

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

    EXPECT_EQ(g_state.accessCalls, 1);
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

    EXPECT_EQ(g_state.accessCalls, 1);
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

    EXPECT_EQ(g_state.accessCalls, 1);
    EXPECT_EQ(g_state.strdupCalls, 1);
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
    EXPECT_EQ(g_state.mapperCalls, 2);

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
    EXPECT_EQ(g_state.callocCalls, 1);
    EXPECT_EQ(ctx.num, 0U);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_LoadFstabFails, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.loadFstabNull = true;
    g_state.mockInUpdaterMode = true;
    g_state.updaterMode = 0;
    g_state.mockGetParameterFromCmdLine = true;
    g_state.getParameterRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.createDmCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_ReadFstabFallbackSuccess, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.loadFstabNull = true;
    g_state.mockReadFstab = true;
    g_state.mockInUpdaterMode = true;
    g_state.updaterMode = 0;
    g_state.mockGetParameterFromCmdLine = true;
    g_state.getParameterRet = 0;
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_SUCC);
    EXPECT_EQ(g_state.readFstabCalls, 1);
    EXPECT_EQ(g_state.createDmCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_UpdaterFstabFileMissingFails, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.loadFstabNull = true;
    g_state.mockInUpdaterMode = true;
    g_state.updaterMode = 1;
    g_state.accessRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.createDmCalls, 0);
    EXPECT_EQ(g_state.readFstabCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_BootSlotsAdjustsPartitionName, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.bootSlots = 2;
    g_state.mockGetCurrentSlot = true;
    g_state.currentSlot = 1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_SUCC);
    EXPECT_EQ(g_state.createDmCalls, 1);
    EXPECT_EQ(g_state.lastTargetNum, 1U);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_CollectEmptyFails, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-data", "/data", "ext4"}};

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.createDmCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_CreateDeviceFailure, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.createDmRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.createDmCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_InitDeviceFailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.initDmRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.createDmCalls, 1);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.removeDmCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_DeviceNotReadyRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.accessRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.waitForFileCalls, 1);
    EXPECT_EQ(g_state.removeDmCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_FormatFailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.formatExt4Ret = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.formatExt4Calls, 1);
    EXPECT_EQ(g_state.removeDmCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MkdirMergeRootFailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.mkdirFailAt = 1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.mkdirCalls, 1);
    EXPECT_EQ(g_state.removeDmCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MountExt4FailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    SetStubResult(STUB_MOUNT, -1);

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.removeDmCalls, 1);
    EXPECT_EQ(g_state.mountOverlayCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MkdirOverlayDirsFailureRollsBack, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.mkdirFailAt = 3;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.removeDmCalls, 1);
    EXPECT_EQ(g_state.mountOverlayCalls, 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MountOverlayFailureRollsBack, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {
        {"/dev/block/dm-0", "/", "erofs"},
        {"/dev/block/dm-1", "/vendor", "erofs"},
    };
    g_state.mountOverlayRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
    EXPECT_EQ(g_state.mountOverlayCalls, 1);
    EXPECT_EQ(g_state.removeDmCalls, 1);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_SuccessMountsDmMergeOverlay, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {
        {"/dev/block/dm-0", "/", "erofs"},
        {"/dev/block/dm-1", "/vendor", "erofs"},
    };

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_SUCC);
    EXPECT_EQ(g_state.createDmCalls, 1);
    EXPECT_EQ(g_state.lastTargetNum, 2U);
    EXPECT_EQ(g_state.initDmCalls, 1);
    EXPECT_EQ(g_state.formatExt4Calls, 1);
    EXPECT_EQ(g_state.mountOverlayCalls, 2);
    ASSERT_EQ(g_state.mountedOverlayPaths.size(), 2U);
    EXPECT_EQ(g_state.mountedOverlayPaths[0], "/usr");
    EXPECT_EQ(g_state.mountedOverlayPaths[1], "/vendor");
    EXPECT_EQ(g_state.vendorPreCalls, 1);
    EXPECT_EQ(g_state.vendorPostCalls, 1);
    EXPECT_EQ(g_state.setRemountResultFlagCalls, 1);
    EXPECT_EQ(g_state.engFilesOverlayCalls, 2);
    EXPECT_EQ(g_state.removeDmCalls, 0);
}
} // namespace init_ut
