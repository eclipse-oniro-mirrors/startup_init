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

#include "erofs_overlay_common.h"
#include "securec.h"
#include "param_stub.h"
using namespace std;
using namespace testing::ext;
namespace init_ut {
class ErofsCommonOverlayUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ErofsCommonOverlayUnitTest, Init_IsOverlayEnable_001, TestSize.Level0)
{
    const char *cmdLine;

    cmdLine = "test=test";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    bool ret = IsOverlayEnable();
    EXPECT_EQ(ret, false);
    remove(BOOT_CMD_LINE);

    cmdLine = "oemmode=test";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = IsOverlayEnable();
    EXPECT_EQ(ret, false);
    remove(BOOT_CMD_LINE);

    cmdLine = "oemmode=test buildvariant=user";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = IsOverlayEnable();
    EXPECT_EQ(ret, false);
    remove(BOOT_CMD_LINE);

    cmdLine = "oemmode=test buildvariant=eng";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = IsOverlayEnable();
    EXPECT_EQ(ret, true);
    ret = remove(BOOT_CMD_LINE);
    PrepareCmdLineData();
}

HWTEST_F(ErofsCommonOverlayUnitTest, Init_IsOverlayCheckExt4_001, TestSize.Level0)
{
    bool ret = CheckIsExt4(STARTUP_INIT_UT_PATH "/data/notexistfile", 0);
    EXPECT_EQ(ret, false);

    const char *fileName = STARTUP_INIT_UT_PATH "/data/ext4_super_block";
    CheckAndCreateDir(fileName);
    ret = CheckIsExt4(fileName, 0);
    EXPECT_EQ(ret, false);

    int fd = open(fileName, O_WRONLY);
    struct ext4_super_block super;
    super.s_magic = EXT4_SUPER_MAGIC;
    write(fd, &super, sizeof(ext4_super_block));
    ret = CheckIsExt4(fileName, 0);
    EXPECT_EQ(ret, false);
    write(fd, &super, sizeof(ext4_super_block));
    ret = CheckIsExt4(fileName, 0);
    remove(fileName);
}

HWTEST_F(ErofsCommonOverlayUnitTest, Init_IsOverlayCheckErofs_001, TestSize.Level0)
{
    bool ret = CheckIsErofs(STARTUP_INIT_UT_PATH"/data/notexistfile");
    EXPECT_EQ(ret, false);

    const char *fileName = STARTUP_INIT_UT_PATH "/data/erofsfile";
    CheckAndCreateDir(fileName);
    ret = CheckIsErofs(fileName);
    EXPECT_EQ(ret, false);

    int fd = open(fileName, O_WRONLY);
    struct ext4_super_block super;
    super.s_magic = EXT4_SUPER_MAGIC;
    write(fd, &super, sizeof(ext4_super_block));
    ret = CheckIsErofs(fileName);
    EXPECT_EQ(ret, false);
    
    write(fd, &super, sizeof(ext4_super_block));
    ret = CheckIsErofs(fileName);
    remove(fileName);
}
}