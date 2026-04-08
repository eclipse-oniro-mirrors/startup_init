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
 * @file begetctl_dac_test.cpp
 * @brief begetctl dac 命令单元测试
 *
 * 测试覆盖：
 * - dac gid [service] - 获取服务GID
 * - dac uid [service] - 获取服务UID
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlDacTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlDacTest, Dac_GetGid_WithServiceName_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dac";
    char arg1[] = "gid";
    char arg2[] = "logd";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDacTest, Dac_Default_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "dac";
    char* args[] = {arg0, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDacTest, Dac_GetUid_WithEmptyService_HandlesGracefully, TestSize.Level1)
{
    char arg0[] = "dac";
    char arg1[] = "uid";
    char arg2[] = "";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
