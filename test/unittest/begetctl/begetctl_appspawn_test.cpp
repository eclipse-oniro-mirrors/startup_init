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
 * @file begetctl_appspawn_test.cpp
 * @brief begetctl appspawn_time 命令单元测试
 *
 * 测试覆盖：
 * - appspawn_time - 获取应用启动时间信息
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlAppspawnTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlAppspawnTest, AppspawnTime_GetSpawnTime_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "appspawn_time";
    char* args[] = {arg0, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
