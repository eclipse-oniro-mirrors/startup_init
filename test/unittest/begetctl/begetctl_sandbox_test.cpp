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
 * @file begetctl_sandbox_test.cpp
 * @brief begetctl sandbox 命令单元测试
 *
 * 测试覆盖：
 * - sandbox -s -n -p -h - 沙箱多选项命令
 * - sandbox -b [bundleId] - 指定bundleId
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlSandboxTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlSandboxTest, Sandbox_WithMultipleOptions_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "sandbox";
    char arg1[] = "-s";
    char arg2[] = "test";
    char arg3[] = "-n";
    char arg4[] = "test2";
    char arg5[] = "-p";
    char arg6[] = "test3";
    char arg7[] = "-h";
    char arg8[] = "?";
    char* args[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 9, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlSandboxTest, Sandbox_WithBundleId_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "sandbox";
    char arg1[] = "-b";
    char arg2[] = "1008";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlSandboxTest, Sandbox_InvalidOption_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "sandbox";
    char arg1[] = "--invalid";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
