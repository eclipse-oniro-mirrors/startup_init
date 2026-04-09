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
 * @file begetctl_loglevel_test.cpp
 * @brief begetctl setloglevel/getloglevel 命令单元测试
 *
 * 测试覆盖：
 * - setloglevel [level] - 设置日志级别
 * - getloglevel - 获取当前日志级别
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlLogLevelTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlLogLevelTest, SetLogLevel_WithValidLevel_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char arg1[] = "1";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogLevelTest, SetLogLevel_NoArguments_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char* args[] = {arg0, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogLevelTest, SetLogLevel_InvalidLevel_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char arg1[] = "a";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogLevelTest, SetLogLevel_OutOfRangeLevel_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char arg1[] = "999";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogLevelTest, GetLogLevel_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "getloglevel";
    char* args[] = {arg0, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
