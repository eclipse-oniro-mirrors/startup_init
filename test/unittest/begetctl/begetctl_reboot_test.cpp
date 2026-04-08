/**
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

/**
 * @file begetctl_reboot_test.cpp
 * @brief begetctl reboot 命令单元测试
 *
 * 测试覆盖：
 * - reboot - 默认重启
 * - reboot shutdown - 关机
 * - reboot charge - 充电模式
 * - reboot updater[:params] - 更新模式
 * - reboot flashd[:params] - flashd模式
 * - reboot suspend - 休眠模式
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlRebootTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlRebootTest, Reboot_Default_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char* args[] = {arg0, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_Shutdown_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "shutdown";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_Charge_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "charge";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_Updater_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "updater";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_UpdaterWithCustom_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "updater:aaaaaaa";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_Flashd_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "flashd";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_FlashdWithCustom_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "flashd:aaaaaaa";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_Suspend_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "suspend";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlRebootTest, Reboot_InvalidMode_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "222222222";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
