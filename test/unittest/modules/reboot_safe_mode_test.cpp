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

#include <limits.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "reboot_adp.h"
#include "init_cmdexecutor.h"
#include "init_hook.h"
#include "init_utils.h"
#include "fs_manager/fs_manager.h"
#include "param_stub.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
static constexpr size_t MISC_COMMAND_SIZE = 32;
static constexpr size_t MISC_UPDATE_MESSAGE_SIZE = 2048;

extern "C" int DoRebootSafemode(int id, const char *name, int argc, const char **argv);

class RebootSafeModeTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

static int CallDoRebootSafemode(const char *arg)
{
    char buffer[MISC_COMMAND_SIZE] = {0};
    if (arg != nullptr) {
        if (strcpy_s(buffer, sizeof(buffer), arg) != EOK) {
            return -1;
        }
        const char *argv[] = {buffer, nullptr};
        return DoRebootSafemode(0, "reboot.safemode", 1, argv);
    }
    return DoRebootSafemode(0, "reboot.safemode", 0, nullptr);
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_NormalPath, TestSize.Level1)
{
    EXPECT_EQ(CallDoRebootSafemode("safemode"), 0);
    EXPECT_EQ(CallDoRebootSafemode("safemode:recover"), 0);
    const char *argvAny[] = {"anything", nullptr};
    EXPECT_EQ(DoRebootSafemode(0, "reboot.safemode", 1, argvAny), 0);
    const char *argvMulti[] = {"safemode", "extra", nullptr};
    EXPECT_EQ(DoRebootSafemode(0, "reboot.safemode", 2, argvMulti), 0);
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_MiscContent, TestSize.Level1)
{
    int ret = CallDoRebootSafemode("safemode");
    EXPECT_EQ(ret, 0);
    char reason[MISC_COMMAND_SIZE] = {0};
    int readRet = GetRebootReasonFromMisc(reason, sizeof(reason));
    if (readRet == 0) {
        EXPECT_STREQ(reason, "boot_safe");
    }
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_MiscWriteFailure_NoHang, TestSize.Level1)
{
    EXPECT_EQ(CallDoRebootSafemode("safemode"), 0);
    char buffer[MISC_COMMAND_SIZE] = {0};
    ASSERT_EQ(strcpy_s(buffer, sizeof(buffer), "safemode"), EOK);
    const char *argv[] = {buffer, nullptr};
    EXPECT_EQ(PluginExecCmd("reboot.safemode", 1, argv), 0);
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_InvalidParam, TestSize.Level1)
{
    EXPECT_EQ(CallDoRebootSafemode(nullptr), 0);
    EXPECT_EQ(CallDoRebootSafemode(""), 0);
    EXPECT_EQ(CallDoRebootSafemode(" "), 0);
    const char *argv[] = {"safemode", nullptr};
    EXPECT_EQ(DoRebootSafemode(0, "reboot.safemode", 0, argv), 0);
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_NegativeArgc_ReturnsZero, TestSize.Level1)
{
    const char *argv[] = {"safemode", nullptr};
    int retMinusOne = DoRebootSafemode(0, "reboot.safemode", -1, argv);
    int retIntMin = DoRebootSafemode(0, "reboot.safemode", INT_MIN, argv);
    EXPECT_EQ(retMinusOne, 0);
    EXPECT_EQ(retIntMin, 0);
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_Registered, TestSize.Level1)
{
    size_t size = 0;
    const ParamCmdInfo *infos = GetStartupPowerCtl(&size);
    ASSERT_NE(infos, nullptr);
    bool found = false;
    for (size_t i = 0; i < size; i++) {
        if (infos[i].name != nullptr && strcmp(infos[i].name, "reboot,safemode") == 0) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_DispatchByCmd, TestSize.Level1)
{
    char buffer[MISC_COMMAND_SIZE] = {0};
    ASSERT_EQ(strcpy_s(buffer, sizeof(buffer), "safemode"), EOK);
    const char *argv[] = {buffer, nullptr};
    int ret = PluginExecCmd("reboot.safemode", 1, argv);
    EXPECT_EQ(ret, 0);
}

static void WriteSafeModeCmdLine(const char *miscDevice)
{
    std::string cmdLine = "bootgroup=device.boot.group earlycon=uart8250,mmio32,0xfe660000 "
        "root=PARTUUID=614e0000-0000 rw rootwait rootfstype=ext4 console=ttyFIQ0 hardware=rk3568 "
        "ohos.required_mount.system="
        "/dev/block/platform/fe310000.sdhci/by-name/system@/usr@ext4@ro,barrier=1@wait,required ";
    if (miscDevice != nullptr) {
        cmdLine += "ohos.required_mount.misc=";
        cmdLine += miscDevice;
        cmdLine += "@/misc@none@none@wait,required ";
    }
    CreateTestFile(BOOT_CMD_LINE, cmdLine.c_str());
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_NoMiscMount_ReturnsZero, TestSize.Level1)
{
    WriteSafeModeCmdLine(nullptr);
    char miscPath[PATH_MAX] = {0};
    ASSERT_NE(GetBlockDevicePath("/misc", miscPath, sizeof(miscPath)), 0);
    char buffer[MISC_COMMAND_SIZE] = {0};
    ASSERT_EQ(strcpy_s(buffer, sizeof(buffer), "safemode"), EOK);
    const char *argv[] = {buffer, nullptr};
    int ret = DoRebootSafemode(0, "reboot.safemode", 1, argv);
    EXPECT_EQ(ret, 0);
    PrepareCmdLineData();
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_MiscReadFailure_ReturnsZero, TestSize.Level1)
{
    const char *missingMisc = STARTUP_INIT_UT_PATH "/misc_not_exist";
    WriteSafeModeCmdLine(missingMisc);
    char miscPath[PATH_MAX] = {0};
    ASSERT_EQ(GetBlockDevicePath("/misc", miscPath, sizeof(miscPath)), 0);
    char reason[MISC_COMMAND_SIZE] = {0};
    ASSERT_NE(GetRebootReasonFromMisc(reason, sizeof(reason)), 0);
    char buffer[MISC_COMMAND_SIZE] = {0};
    ASSERT_EQ(strcpy_s(buffer, sizeof(buffer), "safemode"), EOK);
    const char *argv[] = {buffer, nullptr};
    int ret = DoRebootSafemode(0, "reboot.safemode", 1, argv);
    EXPECT_EQ(ret, 0);
    PrepareCmdLineData();
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_MiscWriteFailure_ReturnsZero, TestSize.Level1)
{
    WriteSafeModeCmdLine("/proc/version");
    char miscPath[PATH_MAX] = {0};
    ASSERT_EQ(GetBlockDevicePath("/misc", miscPath, sizeof(miscPath)), 0);
    FILE *readFp = fopen(miscPath, "rb");
    ASSERT_NE(readFp, nullptr);
    (void)fclose(readFp);
    FILE *writeFp = fopen(miscPath, "rb+");
    bool writable = false;
    if (writeFp != nullptr) {
        char probe[MISC_UPDATE_MESSAGE_SIZE] = {0};
        size_t writeLen = fwrite(probe, sizeof(probe), 1, writeFp);
        int flushRet = fflush(writeFp);
        writable = (writeLen == 1) && (flushRet == 0) && (ferror(writeFp) == 0);
        (void)fclose(writeFp);
    }
    EXPECT_FALSE(writable) << "misc path must be readable but not writable";
    char buffer[MISC_COMMAND_SIZE] = {0};
    ASSERT_EQ(strcpy_s(buffer, sizeof(buffer), "safemode"), EOK);
    const char *argv[] = {buffer, nullptr};
    int ret = DoRebootSafemode(0, "reboot.safemode", 1, argv);
    EXPECT_EQ(ret, 0);
    PrepareCmdLineData();
}
} // namespace init_ut
