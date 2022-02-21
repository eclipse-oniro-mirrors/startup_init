/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "init_cmds.h"
#include "init_group_manager.h"
#include "init_hashmap.h"
#include "init_param.h"
#include "init_plugin.h"
#include "init_plugin_manager.h"
#include "init_unittest.h"
#include "init_utils.h"
#include "securec.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class PluginUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

int g_cmdExecId = 0;
int TestCmdExecutor(int id, const char *name, int argc, const char **argv)
{
    printf("TestCmdExecutor id %d, name %s \n", id, name);
    g_cmdExecId = id;
    return 0;
}

HWTEST_F(PluginUnitTest, PluginAddCmd, TestSize.Level1)
{
    InitServiceSpace();
    SetPluginInterface();
    PluginManagerInit();
    PluginInterface *pluginInterface = GetPluginInterface();
    ASSERT_NE(pluginInterface, nullptr);
    const char *testName = "testCmd1";
    const char *cmdContent = "testCmd1 test1 test2 test3";
    int cmdExecId1 = pluginInterface->addCmdExecutor(testName, TestCmdExecutor);
    ASSERT_NE(cmdExecId1 > 0, 0);
    int cmdExecId2 = pluginInterface->addCmdExecutor("testCmd2", TestCmdExecutor);
    ASSERT_NE(cmdExecId2 > 0, 0);
    cmdExecId2 = pluginInterface->addCmdExecutor("testCmd3", TestCmdExecutor);
    ASSERT_NE(cmdExecId2 > 0, 0);
    int cmdExecId4 = pluginInterface->addCmdExecutor("testCmd4", TestCmdExecutor);
    ASSERT_NE(cmdExecId4 > 0, 0);

    int cmdIndex = 0;
    const char *cmdName = PluginGetCmdIndex(cmdContent, &cmdIndex);
    ASSERT_EQ(strcmp(cmdName, testName), 0);
    printf("TestCmdExecutor cmdIndex 0x%04x, name %s \n", cmdIndex, cmdName);

    // exec
    g_cmdExecId = -1;
    PluginExecCmdByName(cmdName, cmdContent);
    ASSERT_EQ(cmdExecId1, g_cmdExecId);
    g_cmdExecId = -1;
    PluginExecCmdByCmdIndex(cmdIndex, cmdContent);
    ASSERT_EQ(cmdExecId1, g_cmdExecId);

    // del
    pluginInterface->removeCmdExecutor("testCmd4", cmdExecId4);
}

static int PluginTestInit(void)
{
    PluginInterface *pluginInterface = GetPluginInterface();
    if (pluginInterface != nullptr && pluginInterface->addCmdExecutor != nullptr) {
        g_cmdExecId = pluginInterface->addCmdExecutor("testCmd4", TestCmdExecutor);
    }
    return 0;
}

static void PluginTestExit(void)
{
    PluginInterface *pluginInterface = GetPluginInterface();
    ASSERT_NE(pluginInterface, nullptr);
    pluginInterface->removeCmdExecutor("testCmd4", g_cmdExecId);
}

HWTEST_F(PluginUnitTest, PluginInstallTest, TestSize.Level1)
{
    PluginInterface *pluginInterface = GetPluginInterface();
    ASSERT_NE(pluginInterface, nullptr);
    const char *moduleName = "testplugin";
    pluginInterface->pluginRegister(moduleName,
        "/home/axw/init_ut/etc/init/plugin_param_test.cfg",
        PluginTestInit, PluginTestExit);
    PluginInstall(moduleName, NULL);
    PluginUninstall(moduleName);
}
}  // namespace init_ut
