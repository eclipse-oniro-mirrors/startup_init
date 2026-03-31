/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "init_log.h"
#include "init_group_manager.h"
#include "init_param.h"
#include "init_utils.h"
#include "init_cmdexecutor.h"
#include "param_stub.h"
#include "init_modulemgr.h"
#include "init_modulemgr.h"

using namespace testing::ext;
using namespace std;

static int TestCmdExecutor(int id, const char *name, int argc, const char **argv)
{
    (void)id;
    (void)name;
    (void)argc;
    (void)argv;
    return 0;
}

namespace init_ut {
class CmdExecutorUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {
        InitServiceSpace();
    }
    static void TearDownTestCase(void) {
        CloseParamWorkSpace();
    }
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(CmdExecutorUnitTest, TestAddCmdExecutor, TestSize.Level0)
{
    const char *cmdName = "test_cmd";
    int ret = AddCmdExecutor(cmdName, TestCmdExecutor);
    EXPECT_GE(ret, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestAddCareContextCmdExecutor, TestSize.Level0)
{
    const char *cmdName = "test_care_context_cmd";
    int ret = AddCareContextCmdExecutor(cmdName, TestCmdExecutor);
    EXPECT_GE(ret, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestAddCmdExecutorWithNullName, TestSize.Level0)
{
    int ret = AddCmdExecutor(nullptr, TestCmdExecutor);
    EXPECT_LT(ret, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestAddCmdExecutorWithNullExecutor, TestSize.Level0)
{
    const char *cmdName = "test_null_executor";
    int ret = AddCmdExecutor(cmdName, nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestRemoveCmdExecutor, TestSize.Level0)
{
    const char *cmdName = "test_remove_cmd";
    int id = AddCmdExecutor(cmdName, TestCmdExecutor);
    EXPECT_GE(id, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestRemoveCmdExecutorWithNullName, TestSize.Level0)
{
}

HWTEST_F(CmdExecutorUnitTest, TestPluginExecCmdByName, TestSize.Level0)
{
    const char *cmdName = "test_exec_by_name";
    AddCmdExecutor(cmdName, TestCmdExecutor);
    
    PluginExecCmdByName(cmdName, "arg1 arg2");
}

HWTEST_F(CmdExecutorUnitTest, TestPluginExecCmdByNameWithNullName, TestSize.Level0)
{
    PluginExecCmdByName(nullptr, "arg1");
}

HWTEST_F(CmdExecutorUnitTest, TestPluginExecCmdByNameWithNonExistentName, TestSize.Level0)
{
    PluginExecCmdByName("non_existent_cmd", "arg1");
}

HWTEST_F(CmdExecutorUnitTest, TestPluginExecCmd, TestSize.Level0)
{
    const char *cmdName = "test_exec_cmd";
    AddCmdExecutor(cmdName, TestCmdExecutor);
    
    const char *argv[] = {"arg1", "arg2"};
    int ret = PluginExecCmd(cmdName, 2, argv);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestPluginExecCmdWithNullName, TestSize.Level0)
{
    int ret = PluginExecCmd(nullptr, 0, nullptr);
    EXPECT_LT(ret, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestGetPluginCmdNameByIndex, TestSize.Level0)
{
    const char *cmdName = "test_get_name";
    int id = AddCmdExecutor(cmdName, TestCmdExecutor);
    
    const char *name = GetPluginCmdNameByIndex(id);
    EXPECT_NE(name, nullptr);
}

HWTEST_F(CmdExecutorUnitTest, TestGetPluginCmdNameByIndexWithInvalidIndex, TestSize.Level0)
{
    const char *name = GetPluginCmdNameByIndex(9999);
    EXPECT_EQ(name, nullptr);
}

HWTEST_F(CmdExecutorUnitTest, TestPluginGetCmdIndex, TestSize.Level0)
{
    const char *cmdName = "test_get_index";
    int index = 0;
    const char *result = PluginGetCmdIndex(cmdName, &index);
    EXPECT_NE(result, nullptr);
    EXPECT_GE(index, 0);
}

HWTEST_F(CmdExecutorUnitTest, TestPluginGetCmdIndexWithNullCmd, TestSize.Level0)
{
    int index = 0;
    const char *result = PluginGetCmdIndex(nullptr, &index);
    EXPECT_EQ(result, nullptr);
}

} // namespace init_ut
