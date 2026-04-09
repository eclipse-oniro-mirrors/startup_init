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
 * @file begetctl_log_test.cpp
 * @brief begetctl set/get log 传统命令单元测试
 *
 * 测试覆盖：
 * - set log level [value] - 设置日志级别（传统格式）
 * - get log level - 获取日志级别（传统格式）
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlLogTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlLogTest, SetLog_WithValidLevel_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "1";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogTest, SetLog_NoValue_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogTest, SetLog_LargeValue_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "1000";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogTest, SetLog_InvalidValue_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "a";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogTest, GetLog_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "get";
    char arg1[] = "log";
    char arg2[] = "level";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogTest, SetLog_VeryLargeValue_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "999";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlLogTest, SetLog_WithNegativeValue_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "-1";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
