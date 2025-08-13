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
#include "libfs_dm_snapshot_test.h"
#include <gtest/gtest.h>
#include "func_wrapper.h"
#include "fs_dm_mock.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include "init_utils.h"
#include "securec.h"
using namespace testing;
using namespace testing::ext;
#define STRSIZE 64
#define TESTIODATA "10 10 10"
#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
const static int FD_ID = 100000;
namespace OHOS {
class LibFsDmSnapshotTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void LibFsDmSnapshotTest::SetUpTestCase()
{
    GTEST_LOG_(INFO) << "LibFsDmSnapshotTest SetUpTestCase";
}

void LibFsDmSnapshotTest::TearDownTestCase()
{
    GTEST_LOG_(INFO) << "LibFsDmSnapshotTest TearDownTestCase";
}

void LibFsDmSnapshotTest::SetUp()
{
    GTEST_LOG_(INFO) << "LibFsDmSnapshotTest SetUp";
}

void LibFsDmSnapshotTest::TearDown()
{
    GTEST_LOG_(INFO) << "LibFsDmSnapshotTest TearDown";
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_001, TestSize.Level0)
{
    int ret = FsDmCreateSnapshotDevice(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_002, TestSize.Level0)
{
    OpenFunc func = [](const char *path, int flags) -> int {
        return -1;
    };
    UpdateOpenFunc(func);
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(DmSnapshotTarget));
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmCreateSnapshotDevice("devName", devpath, strlen(devpath), target);
    UpdateOpenFunc(nullptr);

    free(target);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_003, TestSize.Level0)
{
    OpenFunc func = [](const char *path, int flags) -> int {
        return 1;
    };
    CreatedmdevFunc dmFunc = [](int, const char *)  {
        return -1;
    };
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateCloseFunc(closeFunc);
    UpdateOpenFunc(func);
    UpdateCreatedmdevFunc(dmFunc);
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(DmSnapshotTarget));
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmCreateSnapshotDevice("devName", devpath, strlen(devpath), target);
    UpdateOpenFunc(nullptr);
    UpdateCreatedmdevFunc(nullptr);
    UpdateCloseFunc(nullptr);
    free(target);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_004, TestSize.Level0)
{
    OpenFunc func = [](const char *path, int flags) -> int {
        return 1;
    };
    CreatedmdevFunc dmFunc = [](int, const char *) {
        return 0;
    };
    LoaddmdevicetableFunc loadFunc = [](int, const char *, DmVerityTarget *, int) -> int {
        return -1;
    };
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateCloseFunc(closeFunc);
    UpdateOpenFunc(func);
    UpdateCreatedmdevFunc(dmFunc);
    UpdateLoaddmdevicetableFunc(loadFunc);
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(DmSnapshotTarget));
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmCreateSnapshotDevice("devName", devpath, strlen(devpath), target);
    UpdateOpenFunc(nullptr);
    UpdateCreatedmdevFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);

    free(target);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_005, TestSize.Level0)
{
    OpenFunc func = [](const char *path, int flags) -> int {
        return 1;
    };
    CreatedmdevFunc dmFunc = [](int, const char *) -> int {
        return 0;
    };
    LoaddmdevicetableFunc loadFunc = [](int, const char *, DmVerityTarget *, int) -> int {
        return 0;
    };
    ActivedmdeviceFunc activeDmDeviceFunc = [](int, const char *) -> int {
        return -1;
    };
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateOpenFunc(func);
    UpdateCreatedmdevFunc(dmFunc);
    UpdateActivedmdeviceFunc(activeDmDeviceFunc);
    UpdateLoaddmdevicetableFunc(loadFunc);
    UpdateCloseFunc(closeFunc);

    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(DmSnapshotTarget));
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmCreateSnapshotDevice("devName", devpath, strlen(devpath), target);
    UpdateOpenFunc(nullptr);
    UpdateCreatedmdevFunc(nullptr);
    UpdateActivedmdeviceFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);

    free(target);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_006, TestSize.Level0)
{
    OpenFunc func = [](const char *path, int flags) -> int {
        return 1;
    };
    CreatedmdevFunc dmFunc = [](int, const char *) -> int {
        return 0;
    };
    LoaddmdevicetableFunc loadFunc = [](int, const char *, DmVerityTarget *, int) -> int {
        return 0;
    };
    ActivedmdeviceFunc activeDmDeviceFunc = [](int, const char *) -> int {
        return 0;
    };
    DmgetdevicenameFunc deviceNameFunc = [](int, const char *, char *, const uint64_t) -> int {
        return -1;
    };
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateOpenFunc(func);
    UpdateCreatedmdevFunc(dmFunc);
    UpdateActivedmdeviceFunc(activeDmDeviceFunc);
    UpdateDmgetdevicenameFunc(deviceNameFunc);
    UpdateLoaddmdevicetableFunc(loadFunc);
    UpdateCloseFunc(closeFunc);

    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(DmSnapshotTarget));
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmCreateSnapshotDevice("devName", devpath, strlen(devpath), target);
    UpdateOpenFunc(nullptr);
    UpdateCreatedmdevFunc(nullptr);
    UpdateActivedmdeviceFunc(nullptr);
    UpdateDmgetdevicenameFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);
    free(target);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmCreateSnapshotDevice_007, TestSize.Level0)
{
    OpenFunc func = [](const char *path, int flags) -> int {
        return 1;
    };
    CreatedmdevFunc dmFunc = [](int, const char *) -> int {
        return 0;
    };
    
    LoaddmdevicetableFunc loadFunc = [](int, const char *, DmVerityTarget *, int) -> int {
        return 0;
    };
    ActivedmdeviceFunc activeDmDeviceFunc = [](int, const char *) -> int {
        return 0;
    };
    DmgetdevicenameFunc deviceNameFunc = [](int, const char *, char *, const uint64_t) -> int {
        return 0;
    };
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateOpenFunc(func);
    UpdateCreatedmdevFunc(dmFunc);
    UpdateActivedmdeviceFunc(activeDmDeviceFunc);
    UpdateDmgetdevicenameFunc(deviceNameFunc);
    UpdateLoaddmdevicetableFunc(loadFunc);
    UpdateCloseFunc(closeFunc);

    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(DmSnapshotTarget));
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmCreateSnapshotDevice("devName", devpath, strlen(devpath), target);
    UpdateOpenFunc(nullptr);
    UpdateCreatedmdevFunc(nullptr);
    UpdateActivedmdeviceFunc(nullptr);
    UpdateDmgetdevicenameFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);

    free(target);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmSwitchToSnapshotMerge_001, TestSize.Level0)
{
    int ret = FsDmSwitchToSnapshotMerge(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmSwitchToSnapshotMerge_002, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return -1;
    };
    UpdateOpenFunc(openFunc);
    char devName[PATH_MAX] = "devName";
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(target));
    int ret = FsDmSwitchToSnapshotMerge(devName, target);
    free(target);
    UpdateOpenFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmSwitchToSnapshotMerge_003, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    LoaddmdevicetableFunc loadFunc = [](int, const char*, DmSnapshotTarget *, int) -> int {
        return -1;
    };
    UpdateLoaddmdevicetableFunc(loadFunc);
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateCloseFunc(closeFunc);

    char devName[PATH_MAX] = "devName";
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(target));
    int ret = FsDmSwitchToSnapshotMerge(devName, target);
    free(target);

    UpdateOpenFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);

    EXPECT_EQ(ret, -1);
}


HWTEST_F(LibFsDmSnapshotTest, FsDmSwitchToSnapshotMerge_004, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    LoaddmdevicetableFunc loadFunc = [](int, const char*, DmSnapshotTarget *, int) -> int {
        return 0;
    };
    UpdateLoaddmdevicetableFunc(loadFunc);
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateCloseFunc(closeFunc);
    ActivedmdeviceFunc activeDmDeviceFunc = [](int, const char *) -> int {
        return -1;
    };
    UpdateActivedmdeviceFunc(activeDmDeviceFunc);
    char devName[PATH_MAX] = "devName";
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(target));
    int ret = FsDmSwitchToSnapshotMerge(devName, target);
    free(target);

    UpdateOpenFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);
    UpdateActivedmdeviceFunc(nullptr);

    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmSnapshotTest, FsDmSwitchToSnapshotMerge_005, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    LoaddmdevicetableFunc loadFunc = [](int, const char*, DmSnapshotTarget *, int) -> int {
        return 0;
    };
    UpdateLoaddmdevicetableFunc(loadFunc);
    CloseFunc closeFunc = [](int) -> int {
        return 0;
    };
    UpdateCloseFunc(closeFunc);
    ActivedmdeviceFunc activeDmDeviceFunc = [](int, const char *) -> int {
        return 0;
    };
    UpdateActivedmdeviceFunc(activeDmDeviceFunc);
    char devName[PATH_MAX] = "devName";
    DmSnapshotTarget *target = (DmSnapshotTarget *)malloc(sizeof(target));
    int ret = FsDmSwitchToSnapshotMerge(devName, target);
    free(target);

    UpdateOpenFunc(nullptr);
    UpdateLoaddmdevicetableFunc(nullptr);
    UpdateCloseFunc(nullptr);
    UpdateActivedmdeviceFunc(nullptr);

    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmSnapshotTest, ParseStatusText_001, TestSize.Level0)
{
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    char data[PATH_MAX] = "10 10 10";
    bool ret = ParseStatusText(data, info);
    EXPECT_EQ(ret, true);
}

HWTEST_F(LibFsDmSnapshotTest, ParseStatusText_002, TestSize.Level0)
{
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    char data[PATH_MAX] = "10 1011asasdfdasfasdfasdfasdfas";
    bool ret = ParseStatusText(data, info);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, ParseStatusText_003, TestSize.Level0)
{
    StrcpySFunc strcpyFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };
    UpdateStrcpySFunc(strcpyFunc);
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    char data[PATH_MAX] = "10 10";
    bool ret = ParseStatusText(data, info);
    UpdateStrcpySFunc(nullptr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, ParseStatusText_004, TestSize.Level0)
{
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    
    char data[PATH_MAX] = "Invalid";
    bool ret = ParseStatusText(data, info);
    EXPECT_EQ(ret, true);
}

HWTEST_F(LibFsDmSnapshotTest, ParseStatusText_005, TestSize.Level0)
{
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    
    char data[PATH_MAX] = "otherError";
    bool ret = ParseStatusText(data, info);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_001, TestSize.Level0)
{
    int ret = GetDmSnapshotStatus(nullptr, nullptr, nullptr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_002, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return -1;
    };
    UpdateInitdmioFunc(initdmioFunc);
    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);

    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_003, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return 0;
    };
    UpdateInitdmioFunc(initdmioFunc);

    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);

    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);
    UpdateIoctlFunc(nullptr);

    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_004, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return 0;
    };
    UpdateInitdmioFunc(initdmioFunc);

    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        struct dm_ioctl *io = va_arg(args, struct dm_ioctl *);
        io->flags = DM_BUFFER_FULL_FLAG;
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);
    UpdateIoctlFunc(nullptr);

    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_005, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return 0;
    };
    UpdateInitdmioFunc(initdmioFunc);

    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        struct dm_ioctl *io = va_arg(args, struct dm_ioctl *);
        io->data_start = sizeof(struct dm_ioctl *);
        io->data_size =sizeof (struct dm_ioctl *) + sizeof(struct dm_target_spec) - 1;
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);
    UpdateIoctlFunc(nullptr);

    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_006, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return 0;
    };
    UpdateInitdmioFunc(initdmioFunc);

    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        struct dm_ioctl *io = va_arg(args, struct dm_ioctl *);
        io->data_start = sizeof(struct dm_ioctl *);
        io->data_size = sizeof(struct dm_ioctl *) + sizeof(struct dm_target_spec);
        struct dm_target_spec* spec = (struct dm_target_spec*)(&((char*)io)[sizeof(struct dm_ioctl *)]);
        spec->next = sizeof(struct dm_target_spec);
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);
    UpdateIoctlFunc(nullptr);

    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_007, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return 0;
    };
    UpdateInitdmioFunc(initdmioFunc);

    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        struct dm_ioctl *io = va_arg(args, struct dm_ioctl *);
        io->data_start = sizeof(struct dm_ioctl *);
        io->data_size = sizeof(struct dm_ioctl *) + sizeof(struct dm_target_spec);
        struct dm_target_spec* spec = (struct dm_target_spec*)(&((char*)io)[sizeof(struct dm_ioctl *)]);
        spec->next = sizeof(struct dm_target_spec) + 1;
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);
    UpdateIoctlFunc(nullptr);

    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmSnapshotStatus_008, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    InitdmioFunc initdmioFunc = [](struct dm_ioctl *, const char*) -> int {
        return 0;
    };
    UpdateInitdmioFunc(initdmioFunc);

    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        struct dm_ioctl *io = va_arg(args, struct dm_ioctl *);
        io->data_start = sizeof(struct dm_ioctl);
        io->data_size = sizeof(struct dm_ioctl) + sizeof(struct dm_target_spec);
        struct dm_target_spec* spec = (struct dm_target_spec*)(&((char*)io)[sizeof(struct dm_ioctl)]);
        spec->next = sizeof(struct dm_target_spec) + 1 + strlen(TESTIODATA);
        char *data = (char *)io + sizeof(struct dm_target_spec) + io->data_start;
        int ret = strcpy_s(spec->target_type, DM_MAX_TYPE_NAME, "targetType");
        if (ret < 0) {
            return -1;
        }
        ret = strcpy_s(data, spec->next - sizeof(struct dm_target_spec), TESTIODATA);
        if (ret < 0) {
            return -1;
        }
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char name[PATH_MAX] = "name";
    char targetType[PATH_MAX] = "targetType";
    StatusInfo *info = (StatusInfo *)malloc(sizeof(StatusInfo));
    int ret = GetDmSnapshotStatus(name, targetType, info);
    UpdateOpenFunc(nullptr);
    UpdateInitdmioFunc(nullptr);
    UpdateIoctlFunc(nullptr);

    EXPECT_EQ(ret, true);
}

HWTEST_F(LibFsDmSnapshotTest, GetDmMergeProcess_001, TestSize.Level0)
{
    bool ret = GetDmMergeProcess(nullptr, nullptr);
    EXPECT_EQ(ret, false);
}
}