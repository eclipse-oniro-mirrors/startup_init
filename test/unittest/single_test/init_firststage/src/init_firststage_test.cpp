/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "init_firststage_test.h"
#include "init.h"
#include "init_log.h"
#include "init_utils.h"
#include "init_mount.h"
#include "dm_merge_overlay.h"
#include "erofs_overlay_common.h"
#include "fs_manager/fs_manager.h"
#include "remount_stub.h"
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>

using namespace testing;
using namespace testing::ext;

#ifdef __cplusplus
extern "C" {
#endif

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

namespace OHOS {
class InitFirststageTest : public testing::Test {
public:
    static constexpr int mockForkPid = 1000;
    static constexpr int mockUeventSockFd = 10000;
    static constexpr int mockLargeArgvCount = 1000000;
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void InitFirststageTest::SetUpTestCase()
{
}

void InitFirststageTest::TearDownTestCase()
{
    RemountResetAllStubs();
}

void InitFirststageTest::SetUp()
{
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_MOUNT, 0);
    RemountSetStubResult(STUB_UMOUNT2, 0);
    RemountSetStubResult(STUB_EXECV, 0);
    RemountSetStubResult(STUB_REBOOT, 0);
    RemountSetStubResult(STUB_FORK, mockForkPid);
    RemountSetStubResult(STUB_WAITPID, 0);
    RemountSetStubResult(STUB_MKDIR, 0);
    RemountSetStubResult(STUB_EXIT, 0);
    RemountSetStubResult(STUB_GET_DEV_SIZE, 0);
    RemountSetStubResult(STUB_CHECK_IS_EXT4, 0);
    RemountSetStubResult(STUB_MOUNT_EXT4_DEVICE, 0);
    RemountSetStubResult(STUB_SETMNTENT, -1);
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 1);
    RemountSetStubResult(STUB_GET_PARAM_FROM_CMDLINE, -1);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
    RemountSetStubResult(STUB_FS_DM_CREATE_MULTI_TARGET, 0);
    RemountSetStubResult(STUB_FS_DM_INIT_DM_DEV, 0);
    RemountSetStubResult(STUB_COLLECT_DM_MERGE_TARGETS, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 0);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 0);
    RemountSetStubResult(STUB_TRY_DM_MERGE_OVERLAY, 0);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 0);
    RemountSetStubResult(STUB_MOUNT_DM_MERGE_OVERLAY_ALL, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
    RemountSetStubFstab(nullptr);
}

void InitFirststageTest::TearDown()
{
    RemountSetStubFstab(nullptr);
}

HWTEST_F(InitFirststageTest, StartSecondStageInit_001, TestSize.Level0)
{
    StartSecondStageInit(0);
    EXPECT_EQ(RemountGetStubResult(STUB_EXIT), 0);
}

HWTEST_F(InitFirststageTest, StartSecondStageInit_002, TestSize.Level1)
{
    StartSecondStageInit(mockLargeArgvCount);
    EXPECT_EQ(RemountGetStubResult(STUB_EXIT), 0);
}

HWTEST_F(InitFirststageTest, StartSecondStageInit_Asan_001, TestSize.Level0)
{
    CreateTestFile("/log/asanTest", "test");
    RemountSetStubResult(STUB_ACCESS, 0);
    StartSecondStageInit(0);
    remove("/log/asanTest");
    EXPECT_EQ(RemountGetStubResult(STUB_EXECV), 0);
}

HWTEST_F(InitFirststageTest, StartSecondStageInit_Asan_002, TestSize.Level1)
{
    RemountSetStubResult(STUB_ACCESS, -1);
    StartSecondStageInit(0);
    EXPECT_EQ(RemountGetStubResult(STUB_EXECV), 0);
}

HWTEST_F(InitFirststageTest, StartSecondStageInit_execvFail_001, TestSize.Level1)
{
    RemountSetStubResult(STUB_EXIT, -1);
    StartSecondStageInit(0);
    EXPECT_EQ(RemountGetStubResult(STUB_EXECV), 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_001_updaterMode, TestSize.Level0)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 1);
    SystemPrepare(0);
    EXPECT_EQ(RemountGetStubResult(STUB_IS_OVERLAY_ENABLE), 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_002_noUpdater_fstabNull, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubFstab(nullptr);
    SystemPrepare(0);
    EXPECT_EQ(RemountGetStubResult(STUB_IS_OVERLAY_ENABLE), 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_001_perPartitionMode, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    item->deviceName = strdup("/dev/test");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    EXPECT_EQ(RemountGetStubResult(STUB_IS_OVERLAY_ENABLE), 1);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_002_hyperholdDisable, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 2);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    item->deviceName = strdup("/dev/test");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    EXPECT_EQ(RemountGetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER), 2);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_003_dmMergeSetup, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 1);
    RemountSetStubResult(STUB_TRY_DM_MERGE_OVERLAY, 0);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    item->deviceName = strdup("/dev/test");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    EXPECT_EQ(RemountGetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET), 1);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_004_dmMergeCleanup, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 1);
    RemountSetStubResult(STUB_TRY_DM_MERGE_OVERLAY, 0);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 1);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    item->deviceName = strdup("/dev/test");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    EXPECT_EQ(RemountGetStubResult(STUB_CHECK_DM_MERGE_CLEANUP), 1);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_005_mountAll, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 1);
    RemountSetStubResult(STUB_TRY_DM_MERGE_OVERLAY, 0);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 0);
    RemountSetStubResult(STUB_MOUNT_DM_MERGE_OVERLAY_ALL, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    item->deviceName = strdup("/dev/test");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    EXPECT_EQ(RemountGetStubResult(STUB_MOUNT_DM_MERGE_OVERLAY_ALL), 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_MountFail_001, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, -1);
    RemountSetStubFstab(nullptr);
    SystemPrepare(0);
    EXPECT_EQ(RemountGetStubResult(STUB_IN_UPDATER_MODE), 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_GetRequiredDevices_001, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->deviceName = strdup("/dev/block/dm-0");
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_GetRequiredDevices_002_twoItems, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item1 = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    FstabItem *item2 = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item1, nullptr);
    ASSERT_NE(item2, nullptr);
    item1->deviceName = strdup("/dev/block/dm-0");
    item1->fsManagerFlags = FS_MANAGER_REQUIRED;
    item2->deviceName = strdup("/dev/block/dm-0");
    item2->mountPoint = strdup("/vendor");
    item2->fsManagerFlags = FS_MANAGER_REQUIRED;
    item1->next = item2;
    fstab->head = item1;
    fstab->tail = item2;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_006_dmMergeSetupViaDoMount, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 1);
    RemountSetStubResult(STUB_TRY_DM_MERGE_OVERLAY, 0);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);
    RemountSetStubResult(STUB_MOUNT_DM_MERGE_OVERLAY_ALL, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->deviceName = strdup("/dev/block/dm-0");
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_007_perPartitionViaDoMount, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->deviceName = strdup("/dev/block/dm-0");
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_Overlay_008_hyperholdDisableViaDoMount, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 2);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->deviceName = strdup("/dev/block/dm-0");
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, StartSecondStageInit_003_execvFailLog, TestSize.Level1)
{
    jmp_buf exitJmp;
    int jmpRet = setjmp(exitJmp);
    if (jmpRet == 0) {
        RemountSetupExitLongjmp(&exitJmp);
        RemountSetStubResult(STUB_EXECV, -1);
        StartSecondStageInit(0);
    }
    RemountClearExitLongjmp();
    RemountSetStubResult(STUB_EXECV, 0);
    EXPECT_EQ(jmpRet, 1);
}

HWTEST_F(InitFirststageTest, SystemPrepare_UeventSockFail, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    item->deviceName = strdup("/dev/block/dm-0");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, -1);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_EXECV, 0);
    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_DmMergeCleanup, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    item->deviceName = strdup("/dev/block/dm-0");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 1);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 1);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 1);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_EXECV, 0);
    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_IS_OVERLAY_ENABLE, 0);
    RemountSetStubResult(STUB_CHECK_HYPERHOLD_DISABLE_MARKER, 0);
    RemountSetStubResult(STUB_IS_HYPERHOLD_ENABLE_MARKER_SET, 0);
    RemountSetStubResult(STUB_CHECK_DM_MERGE_CLEANUP, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_MountFailLogOnly, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    item->deviceName = strdup("/dev/block/dm-0");
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, -1);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_EXECV, 0);
    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);
}

HWTEST_F(InitFirststageTest, SystemPrepare_SnapshotActive, TestSize.Level1)
{
    RemountSetStubResult(STUB_IN_UPDATER_MODE, 0);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, mockUeventSockFd);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 2);
    RemountSetStubResult(STUB_MOUNT_REQURIED_PARTITIONS, 0);
    RemountSetStubResult(STUB_ACCESS, 0);
    RemountSetStubResult(STUB_EXECV, 0);

    Fstab *fstab = static_cast<Fstab*>(calloc(1, sizeof(Fstab)));
    FstabItem *item = static_cast<FstabItem*>(calloc(1, sizeof(FstabItem)));
    ASSERT_NE(fstab, nullptr);
    ASSERT_NE(item, nullptr);
    item->deviceName = strdup("/dev/block/dm-0");
    item->fsManagerFlags = FS_MANAGER_REQUIRED;
    fstab->head = item;
    fstab->tail = item;
    RemountSetStubFstab(fstab);

    SystemPrepare(0);
    RemountSetStubFstab(nullptr);
    RemountSetStubResult(STUB_UEVENT_SOCKET_INIT, 0);
    RemountSetStubResult(STUB_GET_BOOT_SLOTS, 0);
}

}
