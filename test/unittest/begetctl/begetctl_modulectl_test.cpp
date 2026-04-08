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
 * @file begetctl_modulectl_test.cpp
 * @brief begetctl modulectl 命令单元测试
 *
 * 测试覆盖：
 * - modulectl install [module] - 安装模块
 * - modulectl uninstall [module] - 卸载模块
 * - modulectl list [module] - 列出模块信息
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlModulectlTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlModulectlTest, Modulectl_Install_WithModuleName_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "install";
    char arg2[] = "testModule";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlModulectlTest, Modulectl_Install_NoArguments_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "install";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlModulectlTest, Modulectl_Uninstall_WithModuleName_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "uninstall";
    char arg2[] = "testModule";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlModulectlTest, Modulectl_List_WithModuleName_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "list";
    char arg2[] = "testModule";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlModulectlTest, Modulectl_List_AllModules_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "list";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlModulectlTest, Modulectl_InvalidCommand_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "invalid";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_GE(ret, 0);
}

} // namespace init_ut
