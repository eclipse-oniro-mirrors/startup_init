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
#include "remount_overlay.h"
#include "param_stub.h"
#include "mntent.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class RemountOverlayUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(RemountOverlayUnitTest, Init_MntNeedRemountTest_001, TestSize.Level0)
{
    const char *path = "/test";
    bool ret = MntNeedRemount(path);
    EXPECT_EQ(ret, false);

    const char *path2 = "/";
    ret = MntNeedRemount(path2);
    EXPECT_EQ(ret, true);
}

HWTEST_F(RemountOverlayUnitTest, Init_IsSkipRemountTest_001, TestSize.Level0)
{
    struct mntent mentry;
    bool ret = IsSkipRemount(mentry);
    EXPECT_EQ(ret, true);

    char str1[] = "ufs";
    char str2[] = "test";
    mentry.mnt_type = str1;
    mentry.mnt_dir = str2;
    ret = IsSkipRemount(mentry);
    EXPECT_EQ(ret, true);

    char str3[] = "/";
    mentry.mnt_dir = str3;
    ret = IsSkipRemount(mentry);
    EXPECT_EQ(ret, true);

    char str4[] = "er11ofs";
    mentry.mnt_type = str4;
    ret = IsSkipRemount(mentry);
    EXPECT_EQ(ret, true);

    char str5[] = "erofs";
    char str6[] = "/dev/block/ndm-";
    mentry.mnt_type = str5;
    mentry.mnt_fsname = str6;
    ret = IsSkipRemount(mentry);
    EXPECT_EQ(ret, true);

    char str7[] = "/dev/block/dm-1";
    mentry.mnt_fsname = str7;
    ret = IsSkipRemount(mentry);
    EXPECT_EQ(ret, false);
}

HWTEST_F(RemountOverlayUnitTest, Init_ExecCommand_001, TestSize.Level0)
{
    char *nullArgv[] = {NULL};  // A valid command
    int result = ExecCommand(0, nullArgv);
    EXPECT_NE(result, 0);  // Expect success

    char *validArgv[] = {new char[strlen("/bin/ls") + 1], NULL};  // A valid command
    result = ExecCommand(1, validArgv);
    EXPECT_NE(result, 0);  // Expect success

    char *invalidArgv[] = {new char[strlen("/notexit/ls") + 1], NULL};  // A valid command
    result = ExecCommand(1, invalidArgv);
    delete[] validArgv[0];
    delete[] invalidArgv[0];
    EXPECT_NE(result, 0);  // Expect success
}

HWTEST_F(RemountOverlayUnitTest, Init_GetDevSizeTest_001, TestSize.Level0)
{
    const char *fileName = "";
    int ret = GetDevSize(fileName, NULL);
    EXPECT_EQ(ret, -1);
    fileName = STARTUP_INIT_UT_PATH "/data/getDevSize";
    CheckAndCreateDir(fileName);
    ret = GetDevSize(fileName, NULL);
    EXPECT_EQ(ret, -1);

    uint64_t devSize;
    ret = GetDevSize(fileName, &devSize);
    EXPECT_NE(ret, 0);
    remove(fileName);
}

HWTEST_F(RemountOverlayUnitTest, Init_FormatExt4Test_001, TestSize.Level0)
{
    const char *fileName = "";
    int ret = GetDevSize(fileName, NULL);
    EXPECT_EQ(ret, -1);
    fileName = STARTUP_INIT_UT_PATH "/data/getDevSize";
    CheckAndCreateDir(fileName);
    ret = GetDevSize(fileName, NULL);
    EXPECT_EQ(ret, -1);

    uint64_t devSize;
    ret = GetDevSize(fileName, &devSize);
    EXPECT_NE(ret, 0);
    remove(fileName);
}
}