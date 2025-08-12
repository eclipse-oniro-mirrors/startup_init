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
#include <gtest/gtest.h>
#include "func_wrapper.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include "init_utils.h"
#include "securec.h"
using namespace testing;
using namespace testing::ext;
#define STRSIZE 64

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

    int rc = DoMountOneItem(&item);
    EXPECT_EQ(rc, -1);
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

    int rc = DoMountOneItem(&item);
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
    int rc = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    EXPECT_EQ(rc, -1);
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

    int rc = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateSnprintfSFunc(nullptr);

    EXPECT_EQ(rc, -1);
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

    int rc = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateMountFunc(nullptr);

    EXPECT_EQ(rc, 0);
}

}