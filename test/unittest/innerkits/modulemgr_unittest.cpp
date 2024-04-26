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
#include "bootstage.h"
#include "init_cmds.h"
#include "init_group_manager.h"
#include "init_hashmap.h"
#include "init_param.h"
#include "init_module_engine.h"
#include "init_cmdexecutor.h"
#include "param_stub.h"
#include "init_utils.h"
#include "securec.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class ModuleMgrUnitTest : public testing::Test {
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

HWTEST_F(ModuleMgrUnitTest, Init_ModuleMgrTest_PluginAddCmd001, TestSize.Level1)
{
    InitServiceSpace();
    const char *testName = "testCmd1";
    const char *cmdContent = "testCmd1 test1 test2 test3";
    const char *cmdContentNotValid = "testCmd1 t e s t 1 t e s t 2 t";
    int cmdExecId1 = AddCmdExecutor(testName, TestCmdExecutor);
    ASSERT_NE(cmdExecId1 > 0, 0);
    int cmdExecId2 = AddCmdExecutor("testCmd2", TestCmdExecutor);
    ASSERT_NE(cmdExecId2 > 0, 0);
    cmdExecId2 = AddCmdExecutor("testCmd3", TestCmdExecutor);
    ASSERT_NE(cmdExecId2 > 0, 0);
    int cmdExecId4 = AddCmdExecutor("testCmd4", TestCmdExecutor);
    ASSERT_NE(cmdExecId4 > 0, 0);
    PluginExecCmd("testCmd4", 0, nullptr);

    int cmdIndex = 0;
    const char *cmdName = PluginGetCmdIndex(cmdContent, &cmdIndex);
    ASSERT_EQ(strcmp(cmdName, testName), 0);
    printf("TestCmdExecutor cmdIndex 0x%04x, name %s \n", cmdIndex, cmdName);
    ASSERT_NE(GetPluginCmdNameByIndex(cmdIndex), nullptr);

    // exec
    g_cmdExecId = -1;
    PluginExecCmdByName(cmdName, cmdContent);
    ASSERT_EQ(cmdExecId1, g_cmdExecId);
    PluginExecCmdByName(cmdName, nullptr);
    PluginExecCmdByName(cmdName, cmdContentNotValid);
    g_cmdExecId = -1;
    PluginExecCmdByCmdIndex(cmdIndex, cmdContent, nullptr);
    ASSERT_EQ(cmdExecId1, g_cmdExecId);
    const char *argv[] = {"test.value"};
    PluginExecCmd("install", 1, argv);
    PluginExecCmd("uninstall", 1, argv);
    PluginExecCmd("setloglevel", 1, argv);

    // del
    RemoveCmdExecutor("testCmd4", cmdExecId4);
    AddCareContextCmdExecutor("", nullptr);
    RemoveCmdExecutor("testCmd4", -1);
}

static void TestModuleDump(const MODULE_INFO *moduleInfo)
{
    printf("%s\n", moduleInfo->name);
}

HWTEST_F(ModuleMgrUnitTest, Init_ModuleMgrTest_ModuleTraversal001, TestSize.Level1)
{
    // Create module manager
    MODULE_MGR *moduleMgr = ModuleMgrCreate("init");
    ASSERT_NE(moduleMgr, nullptr);
    int cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);
    // Install one module
    int ret = ModuleMgrInstall(moduleMgr, "libbootchart", 0, nullptr);
    ASSERT_EQ(ret, 0);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 1);
    ModuleMgrTraversal(nullptr, nullptr, nullptr);
    ModuleMgrTraversal(moduleMgr, nullptr, TestModuleDump);
    InitModuleMgrDump();

    InitModuleMgrInstall("/");

    // Scan all modules
    ModuleMgrScan(nullptr);
    ModuleMgrScan("/");
    moduleMgr = ModuleMgrScan("init");
    moduleMgr = ModuleMgrScan(STARTUP_INIT_UT_PATH MODULE_LIB_NAME "/autorun");
    ASSERT_NE(moduleMgr, nullptr);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_GE(cnt, 0);

    ModuleMgrUninstall(moduleMgr, nullptr);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);

    ModuleMgrGetArgs();
    ModuleMgrDestroy(moduleMgr);
}

HWTEST_F(ModuleMgrUnitTest, Init_ModuleMgrTest_ModuleAbnormal001, TestSize.Level1)
{
    int ret = InitModuleMgrInstall(nullptr);
    ASSERT_EQ(ret, -1);
}
}  // namespace init_ut
