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

HWTEST_F(ModuleMgrUnitTest, PluginAddCmd, TestSize.Level1)
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
    PluginExecCmdByCmdIndex(cmdIndex, cmdContent);
    ASSERT_EQ(cmdExecId1, g_cmdExecId);
    const char *argv[] = {"test.value"};
    PluginExecCmd("install", 1, argv);
    PluginExecCmd("uninstall", 1, argv);
    PluginExecCmd("setloglevel", 1, argv);

    // del
    RemoveCmdExecutor("testCmd4", cmdExecId4);
}

HWTEST_F(ModuleMgrUnitTest, ModuleInstallTest, TestSize.Level1)
{
    int ret;
    int cnt;

    // Create module manager
    ASSERT_EQ(ModuleMgrCreate(nullptr), nullptr);
    ModuleMgrDestroy(nullptr);
    MODULE_MGR *moduleMgr = ModuleMgrCreate("init");
    ASSERT_NE(moduleMgr, nullptr);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);

    // Install one module
#ifdef SUPPORT_64BIT
    ret = ModuleMgrInstall(moduleMgr, "/system/lib64/init/libbootchart", 0, NULL);
#else
    ret = ModuleMgrInstall(moduleMgr, "/system/lib/init/libbootchart", 0, NULL);
#endif
    ASSERT_EQ(ret, 0);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 1);

    // Uninstall the module
    ModuleMgrUninstall(moduleMgr, "bootchart");
    InitModuleMgrUnInstall("bootchart");
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);

    // Install two module
    ret = ModuleMgrInstall(moduleMgr, "bootchart", 0, NULL);
    ASSERT_EQ(ret, 0);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 1);
    ret = ModuleMgrInstall(moduleMgr, "notexist", 0, NULL);
    ASSERT_NE(ret, 0);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 1);

    // Uninstall the module
    ModuleMgrUninstall(moduleMgr, "bootchart");
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);
    ModuleMgrUninstall(moduleMgr, "notexist");
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);

    ModuleMgrDestroy(moduleMgr);

    // test updater mode
    int fd = open("/bin/updater", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,  S_IRWXU);
    ASSERT_NE(fd, 0);
    ModuleMgrScan("init/autorun");
    unlink("/bin/updater");
    close(fd);
}

static void TestModuleDump(const MODULE_INFO *moduleInfo)
{
    printf("%s\n", moduleInfo->name);
}

HWTEST_F(ModuleMgrUnitTest, ModuleTraversalTest, TestSize.Level1)
{
    // Create module manager
    MODULE_MGR *moduleMgr = ModuleMgrCreate("init");
    ASSERT_NE(moduleMgr, nullptr);
    int cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);
    // Install one module
    int ret = ModuleMgrInstall(moduleMgr, "bootchart", 0, NULL);
    ASSERT_EQ(ret, 0);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 1);
    ModuleMgrTraversal(nullptr, nullptr, nullptr);
    ModuleMgrTraversal(moduleMgr, NULL, TestModuleDump);
    InitModuleMgrDump();

    // Scan all modules
    ModuleMgrScan(nullptr);

    moduleMgr = ModuleMgrScan("init/autorun");
    ASSERT_NE(moduleMgr, nullptr);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_GE(cnt, 0);

    ModuleMgrUninstall(moduleMgr, NULL);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);

    ModuleMgrGetArgs();
    ModuleMgrDestroy(moduleMgr);
}

HWTEST_F(ModuleMgrUnitTest, ModuleScanTest, TestSize.Level1)
{
    // Scan all modules test init
    MODULE_MGR *moduleMgr = ModuleMgrScan("init");
    ASSERT_NE(moduleMgr, nullptr);
    int cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_GE(cnt, 1);

    ModuleMgrUninstall(nullptr, nullptr);
    ModuleMgrUninstall(moduleMgr, NULL);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ASSERT_EQ(cnt, 0);
    ModuleMgrDestroy(moduleMgr);

    // scan /lib/init/
#ifdef SUPPORT_64BIT
    moduleMgr = ModuleMgrScan("/lib64/init");
#else
    moduleMgr = ModuleMgrScan("/lib/init");
#endif
    ASSERT_NE(moduleMgr, nullptr);
    ModuleMgrGetCnt(nullptr);
    cnt = ModuleMgrGetCnt(moduleMgr);
    ModuleMgrDestroy(moduleMgr);
    EXPECT_EQ(InitModuleMgrInstall(nullptr), -1);
}
}  // namespace init_ut
