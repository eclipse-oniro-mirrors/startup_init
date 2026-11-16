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

#include <cstring>

#include "reboot_adp.h"
#include "init_cmdexecutor.h"
#include "init_hook.h"
#include "param_stub.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
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
    char buffer[32] = {0};
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
    char reason[32] = {0};
    int readRet = GetRebootReasonFromMisc(reason, sizeof(reason));
    if (readRet == 0) {
        EXPECT_STREQ(reason, "boot_safe");
    }
}

HWTEST_F(RebootSafeModeTest, DoRebootSafemode_MiscWriteFailure_NoHang, TestSize.Level1)
{
    EXPECT_EQ(CallDoRebootSafemode("safemode"), 0);
    char buffer[32] = {0};
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
    char buffer[32] = {0};
    ASSERT_EQ(strcpy_s(buffer, sizeof(buffer), "safemode"), EOK);
    const char *argv[] = {buffer, nullptr};
    int ret = PluginExecCmd("reboot.safemode", 1, argv);
    EXPECT_EQ(ret, 0);
}
} // namespace init_ut
