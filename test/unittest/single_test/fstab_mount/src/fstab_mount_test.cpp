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
#include "fstab_mount_test.h"
#include "fs_manager/fs_manager.h"
#include <gtest/gtest.h>
#include "func_wrapper.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include "init_utils.h"
#include "securec.h"
#include "erofs_mount_overlay.h"
using namespace testing;
using namespace testing::ext;
#define STRSIZE 64
#define MAX_BUFFER_LEN 256

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

size_t SnprintfSReturnZero(char *strDest, size_t destMax, size_t count, const char *format, va_list args)
{
    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


namespace OHOS {
class FstabMountTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void FstabMountTest::SetUpTestCase()
{
    GTEST_LOG_(INFO) << "FstabMountTest SetUpTestCase";
}

void FstabMountTest::TearDownTestCase()
{
    GTEST_LOG_(INFO) << "FstabMountTest TearDownTestCase";
}

void FstabMountTest::SetUp()
{
    GTEST_LOG_(INFO) << "FstabMountTest SetUp";
}

void FstabMountTest::TearDown()
{
    GTEST_LOG_(INFO) << "FstabMountTest TearDown";
}

HWTEST_F(FstabMountTest, DoOneMountItem_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = DoMountOneItem(&item, NULL);
    EXPECT_NE(rc, 0);
}

HWTEST_F(FstabMountTest, DoOneMountItem_002, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "hmfs";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5,checkpoint=disable";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = DoMountOneItem(&item, NULL);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_001, TestSize.Level0)
{
    int rc =  GetDataWithoutCheckpoint(nullptr, 0, nullptr, 0);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_002, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3";
    int rc =  GetDataWithoutCheckpoint(fsData, STRSIZE, nullptr, 0);
    EXPECT_EQ(rc, -1);

    char result[STRSIZE] = {0};
    rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    EXPECT_EQ(rc, 0);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_003, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    StrdupFunc func = [](const char *path) -> char *
    {
        return nullptr;
    };
    UpdateStrdupFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateStrdupFunc(nullptr);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_004, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    MallocFunc func = [](size_t size) -> void* {
        return nullptr;
    };
    UpdateMallocFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateMallocFunc(nullptr);
    EXPECT_EQ(rc, -1);
}


HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_005, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    StrncatSFunc func = [](char * strDest, size_t destMax, const char * strSrc, size_t count) -> int {
        if (strcmp(strSrc, ",") == 0) {
            return -1;
        }
        return __real_strncat_s(strDest, destMax, strSrc, count);
    };
    UpdateStrncatSFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateStrncatSFunc(nullptr);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_006, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    StrncatSFunc func = [](char *, size_t, const char *, size_t) -> int {
        return -1;
    };
    UpdateStrncatSFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateStrncatSFunc(nullptr);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, MountWithCheckpoint_001, TestSize.Level0)
{
    StatFunc func = [](const char* path, struct stat *) -> int {
        return 0;
    };
    UpdateStatFunc(func);
    MountResult result = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    EXPECT_EQ(result.rc, -1);
}

HWTEST_F(FstabMountTest, MountWithCheckpoint_002, TestSize.Level0)
{
    StatFunc func = [](const char* path, struct stat *) -> int {
        return 0;
    };
    UpdateStatFunc(func);

    MkdirFunc mkdirFunc = [](const char*, mode_t) -> int {
        return 0;
    };
    UpdateMkdirFunc(mkdirFunc);
    UpdateSnprintfSFunc(SnprintfSReturnZero);

    MountResult result = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateSnprintfSFunc(nullptr);

    GTEST_LOG_(INFO) << "checkpointMountCounter=" << result.checkpointMountCounter;
    EXPECT_EQ(result.rc, -1);
}

HWTEST_F(FstabMountTest, MountWithCheckpoint_003, TestSize.Level0)
{
    StatFunc func = [](const char* path, struct stat *) -> int {
        return 0;
    };
    UpdateStatFunc(func);

    MkdirFunc mkdirFunc = [](const char*, mode_t) -> int {
        return 0;
    };
    UpdateMkdirFunc(mkdirFunc);

    MountFunc mountFunc = [](const char *, const char *, const char *, unsigned long, const void *) -> int {
        errno = EBUSY;
        return -1;
    };
    UpdateMountFunc(mountFunc);

    MountResult result = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateMountFunc(nullptr);

    GTEST_LOG_(INFO) << "checkpointMountCounter=" << result.checkpointMountCounter;
    EXPECT_EQ(result.rc, 0);
}

// mock state not exist
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 1;
    };
    UpdateAccessFunc(func);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    EXPECT_NE(rc, -1);
}

// mock state exist open state fail
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_002, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return -1;
    };
    UpdateOpenFunc(func2);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    EXPECT_EQ(rc, -1);
}

//mock read state suc but state < 0 , record stage
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_003, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return 1;
    };
    UpdateOpenFunc(func2);

    ReadFunc mock_read_specific_state = [](int fd, void *buf, size_t count) -> ssize_t {
        static int call_count = 0;
        call_count++;
        if (call_count == 1) {
            const char* data = "-100";
            memcpy_s(buf, count, data, strlen(data));
            return strlen(data);
        } else if (call_count == 2) {
            const char* data2 = "100";
            memcpy_s(buf, count, data2, strlen(data2));
            return strlen(data2);
        }
        return -1;
    };
    UpdateReadFunc(mock_read_specific_state);


    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    UpdateReadFunc(NULL);
    EXPECT_EQ(rc, -1);
}

//mock read state suc read dm fail
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_004, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return 1;
    };
    UpdateOpenFunc(func2);

    ReadFunc mock_read_specific_state = [](int fd, void *buf, size_t count) -> ssize_t {
        static int call_count = 0;
        call_count++;
        if (call_count == 1) {
            memcpy_s(buf, count, "22873", 5);
            return 5;
        } else if (call_count == 2) {
            errno = EIO;
            return -1;
        }
        return -1;
    };
    UpdateReadFunc(mock_read_specific_state);


    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    UpdateReadFunc(NULL);
    EXPECT_EQ(rc, -1);
}

//mock read state suc read dm suc GetDmDevPath fail
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_005, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return 1;
    };
    UpdateOpenFunc(func2);

    ReadFunc mock_read_specific_state = [](int fd, void *buf, size_t count) -> ssize_t {
        static int call_count = 0;
        call_count++;
        if (call_count == 1) {
            memcpy_s(buf, count, "22873", 5);
            return 5;
        } else if (call_count == 2) {
            memcpy_s(buf, count, "22873", 5);
            return 5;
        }
        return -1;
    };
    UpdateReadFunc(mock_read_specific_state);


    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    UpdateReadFunc(NULL);
#ifdef SUPPORT_HVB
    EXPECT_EQ(rc, -1);
#else
    EXPECT_NE(rc, -1);
#endif
}

HWTEST_F(FstabMountTest, DoMountOverlayDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = DoMountOverlayDevice(&item);
    EXPECT_NE(rc, 0);
}

HWTEST_F(FstabMountTest, GetOverlayDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    char devRofs[MAX_BUFFER_LEN] = {0};
    char devExt4[MAX_BUFFER_LEN] = {0};
    int rc = 0;
    rc = GetOverlayDevice(&item, devRofs, MAX_BUFFER_LEN, devExt4, MAX_BUFFER_LEN);
    EXPECT_NE(rc, 0);
}

HWTEST_F(FstabMountTest, MountPartitionDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    char devRofs[MAX_BUFFER_LEN] = {0};
    char devExt4[MAX_BUFFER_LEN] = {0};
    int rc = 0;
    rc = MountPartitionDevice(&item, devRofs, devExt4);
    EXPECT_EQ(rc, 0);
}
}