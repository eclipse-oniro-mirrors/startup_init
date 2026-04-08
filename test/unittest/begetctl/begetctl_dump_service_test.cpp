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
 * @file begetctl_dump_service_test.cpp
 * @brief begetctl dump_service 命令单元测试
 *
 * 测试覆盖：
 * - dump_service all - 转储所有服务信息
 * - dump_service param_watcher - 转储参数监视器
 * - dump_service parameter-service trigger - 转储触发器信息
 * - dump_service loop - 转储事件循环信息
 * - dump_service [name] - 按服务名转储
 * - dump_nwebspawn/dump_appspawn - 应用spawn信息
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

class BegetctlDumpServiceTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpAll_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "all";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpParamWatcher_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "param_watcher";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpParameterServiceTrigger_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "parameter-service";
    char arg2[] = "trigger";
    char* args[] = {arg0, arg1, arg2, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpLoop_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "loop";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpByName_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "foundation";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpNwebspawn_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_nwebspawn";
    char arg1[] = "";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlDumpServiceTest, DumpService_DumpAppspawn_ExecutesSuccessfully, TestSize.Level1)
{
    char arg0[] = "dump_appspawn";
    char arg1[] = "";
    char* args[] = {arg0, arg1, nullptr};
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
