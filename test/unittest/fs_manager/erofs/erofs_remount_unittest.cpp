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

#include "erofs_remount_overlay.h"
#include "securec.h"
#include "param_stub.h"
#include "fs_manager.h"
using namespace std;
using namespace testing::ext;
namespace init_ut {
class ErofsRemountUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ErofsRemountUnitTest, Init_GetRemountResult_001, TestSize.Level0)
{
    const char *cmdLine;
    rmdir(REMOUNT_RESULT_PATH);
    SetRemountResultFlag();

    CheckAndCreateDir(REMOUNT_RESULT_FLAG);
    SetRemountResultFlag();
    rmdir(REMOUNT_RESULT_PATH);
}

HWTEST_F(ErofsRemountUnitTest, Init_Modem2Exchange_001, TestSize.Level0)
{
    const char *srcPath = STARTUP_INIT_UT_PATH"/data/modem2Exchange";
    const char *targetPath = STARTUP_INIT_UT_PATH"/data/modem2Exchangetarget";

    CheckAndCreateDir(srcPath);
    CheckAndCreateDir(targetPath);

    SetStubResult(STUB_MOUNT, -1);
    int ret = Modem2Exchange(srcPath, targetPath);
    EXPECT_EQ(ret, -1);

    SetStubResult(STUB_MOUNT, 0);
    ret = Modem2Exchange(srcPath, targetPath);
    EXPECT_EQ(ret, 0);

    rmdir(srcPath);
    rmdir(targetPath);
}

HWTEST_F(ErofsRemountUnitTest, Init_ExchangeToMode_001, TestSize.Level0)
{
    const char *srcPath = STARTUP_INIT_UT_PATH"/data/Exchange2Mode";
    const char *targetPath = STARTUP_INIT_UT_PATH"/data/Exchange2Modetarget";

    int ret = Exchange2Modem(srcPath, targetPath);
    EXPECT_EQ(ret, 0);

    CheckAndCreateDir(srcPath);
    CheckAndCreateDir(targetPath);

    SetStubResult(STUB_MOUNT, 0);
    SetStubResult(STUB_UMOUNT, 0);

    ret = Exchange2Modem(srcPath, targetPath);
    EXPECT_EQ(ret, 0);

    CheckAndCreateDir(targetPath);
    SetStubResult(STUB_MOUNT, -1);
    SetStubResult(STUB_UMOUNT, -1);

    Exchange2Modem(srcPath, targetPath);
    EXPECT_EQ(ret, 0);

    rmdir(srcPath);
    rmdir(targetPath);
}

HWTEST_F(ErofsRemountUnitTest, Init_OverlayRemountVendorPre_001, TestSize.Level0)
{
    CheckAndCreateDir(MODEM_DRIVER_MNT_PATH);
    CheckAndCreateDir(MODEM_VENDOR_MNT_PATH);
    CheckAndCreateDir(MODEM_FW_MNT_PATH);
    OverlayRemountVendorPre();
    rmdir(MODEM_DRIVER_MNT_PATH);
    rmdir(MODEM_VENDOR_MNT_PATH);
    rmdir(MODEM_FW_MNT_PATH);

    OverlayRemountVendorPost();
}

HWTEST_F(ErofsRemountUnitTest, Init_RemountOverlayTest_001, TestSize.Level0)
{
    int ret = RemountOverlay();
    EXPECT_EQ(ret, 0);
}
}