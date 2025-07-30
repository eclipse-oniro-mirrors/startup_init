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
#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class BegetctlUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(BegetctlUnitTest, Init_TestShellInit_001, TestSize.Level0)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLs_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "ls";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLsWithR_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "ls";
    char arg2[] = "-r";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLsGet_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "get";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellSet_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "set";
    char arg2[] = "aaaaa";
    char arg3[] = "1234567";
    char* args[] = { arg0, arg1, arg2, arg3, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellGetWithKey_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "get";
    char arg2[] = "aaaaa";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellWait_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "wait";
    char arg2[] = "aaaaa";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellWaitFalse_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "wait";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellWaitWithKey_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "wait";
    char arg2[] = "aaaaa";
    char arg3[] = "12*";
    char arg4[] = "30";
    char* args[] = { arg0, arg1, arg2, arg3, arg4, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellParamShell_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "shell";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLsWithvalue_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "..a", PARAM_STRING, (void *)"..a");
    char arg0[] = "param";
    char arg1[] = "ls";
    char arg2[] = PARAM_REVERESD_NAME_CURR_PARAMETER;
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLsWithvalueExist_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "#", PARAM_STRING, (void *)"#");
    char arg0[] = "param";
    char arg1[] = "ls";
    char arg2[] = "-r";
    char arg3[] = PARAM_REVERESD_NAME_CURR_PARAMETER;
    char* args[] = { arg0, arg1, arg2, arg3, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_001, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "getslot";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_002, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "getsuffix";
    char arg2[] = "1";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_003, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "setactive";
    char arg2[] = "1";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_004, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "setunboot";
    char arg2[] = "2";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_005, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "setactive";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_006, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "setunboot";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_007, TestSize.Level1)
{
    char arg0[] = "partitionslot";
    char arg1[] = "getsuffix";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_001, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "1";
    char* args[] = { arg0, arg1, arg2, arg3, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_002, TestSize.Level1)
{
    char arg0[] = "get";
    char arg1[] = "log";
    char arg2[] = "level";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_003, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_004, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "1000";
    char* args[] = { arg0, arg1, arg2, arg3, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_005, TestSize.Level1)
{
    char arg0[] = "set";
    char arg1[] = "log";
    char arg2[] = "level";
    char arg3[] = "a";
    char* args[] = { arg0, arg1, arg2, arg3, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_001, TestSize.Level1)
{
    char arg0[] = "bootchart";
    char arg1[] = "enable";
    char* args[] = { arg0, arg1, nullptr };
    SystemWriteParam("persist.init.bootchart.enabled", "1");
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_002, TestSize.Level1)
{
    char arg0[] = "bootchart";
    char arg1[] = "start";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_003, TestSize.Level1)
{
    char arg0[] = "bootchart";
    char arg1[] = "stop";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_004, TestSize.Level1)
{
    char arg0[] = "bootchart";
    char arg1[] = "disable";
    char* args[] = { arg0, arg1, nullptr };
    SystemWriteParam("persist.init.bootchart.enabled", "0");
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_005, TestSize.Level1)
{
    char arg0[] = "bootchart";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_001, TestSize.Level1)
{
    char arg0[] = "bootevent";
    char arg1[] = "enable";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_002, TestSize.Level1)
{
    char arg0[] = "bootevent";
    char arg1[] = "disable";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_003, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "all";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_004, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "param_watcher";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_005, TestSize.Level1)
{
    char arg0[] = "dump_service";
    char arg1[] = "parameter-service";
    char arg2[] = "trigger";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpNwebSpawn_001, TestSize.Level1)
{
    char arg0[] = "dump_nwebspawn";
    char arg1[] = "";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpAppspawn_001, TestSize.Level1)
{
    char arg0[] = "dump_appspawn";
    char arg1[] = "";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_001, TestSize.Level1)
{
    char arg0[] = "misc_daemon";
    char arg1[] = "--write_logo";
    char arg2[] = BOOT_CMD_LINE;
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_002, TestSize.Level1)
{
    char arg0[] = "misc_daemon";
    char arg1[] = "--write_logo1111";
    char arg2[] = "test";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_003, TestSize.Level1)
{
    char arg0[] = "misc_daemon";
    char arg1[] = "--write_logo";
    char arg2[] = "";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_004, TestSize.Level1)
{
    char arg0[] = "misc_daemon";
    char arg1[] = "--write_logo";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_005, TestSize.Level1)
{
    // clear misc logo
    char arg0[] = "misc_daemon";
    char arg1[] = "--write_logo";
    char arg2[] = "sssssssss";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_001, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "install";
    char arg2[] = "testModule";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_002, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "uninstall";
    char arg2[] = "testModule";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_003, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "list";
    char arg2[] = "testModule";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_004, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "install";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_005, TestSize.Level1)
{
    char arg0[] = "modulectl";
    char arg1[] = "list";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_006, TestSize.Level1)
{
    int ret = BShellEnvDirectExecute(GetShellHandle(), 0, nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_001, TestSize.Level1)
{
    char arg0[] = "service_control";
    char arg1[] = "stop";
    char arg2[] = "test";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_002, TestSize.Level1)
{
    char arg0[] = "service_control";
    char arg1[] = "start";
    char arg2[] = "test";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_003, TestSize.Level1)
{
    char arg0[] = "stop_service";
    char arg1[] = "test";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_004, TestSize.Level1)
{
    char arg0[] = "start_service";
    char arg1[] = "test";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_005, TestSize.Level1)
{
    char arg0[] = "timer_stop";
    char arg1[] = "test";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_006, TestSize.Level1)
{
    char arg0[] = "timer_stop";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_007, TestSize.Level1)
{
    char arg0[] = "timer_start";
    char arg1[] = "test-service";
    char arg2[] = "10";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_008, TestSize.Level1)
{
    char arg0[] = "timer_start";
    char arg1[] = "test-service";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_009, TestSize.Level1)
{
    char arg0[] = "timer_start";
    char arg1[] = "test-service";
    char arg2[] = "ww";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_010, TestSize.Level1)
{
    char arg0[] = "xxxxxxxxxxxxxx";
    char arg1[] = "test-service";
    char arg2[] = "ww";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_001, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char arg1[] = "1";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_002, TestSize.Level1)
{
    char arg0[] = "getloglevel";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_003, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char arg1[] = "a";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_004, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_005, TestSize.Level1)
{
    char arg0[] = "setloglevel";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), 0, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSandbox_001, TestSize.Level1)
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
    char* args[] = { arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestSandbox_002, TestSize.Level1)
{
    char arg0[] = "sandbox";
    char arg1[] = "-b";
    char arg2[] = "1008";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_001, TestSize.Level1)
{
    char arg0[] = "reboot";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_002, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "shutdown";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_003, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "charge";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_004, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "updater";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_005, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "updater:aaaaaaa";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_006, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "flashd:aaaaaaa";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_007, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "flashd";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_008, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "suspend";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_009, TestSize.Level1)
{
    char arg0[] = "reboot";
    char arg1[] = "222222222";
    char* args[] = { arg0, arg1, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestGid_001, TestSize.Level1)
{
    char arg0[] = "dac";
    char arg1[] = "gid";
    char arg2[] = "logd";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestGid_002, TestSize.Level1)
{
    char arg0[] = "dac";
    char* args[] = { arg0, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestUid_001, TestSize.Level1)
{
    char arg0[] = "dac";
    char arg1[] = "uid";
    char arg2[] = "";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlUnitTest, Init_TestUid_002, TestSize.Level1)
{
    char arg0[] = "dac";
    char arg1[] = "uid";
    char arg2[] = "";
    char* args[] = { arg0, arg1, arg2, nullptr };
    int ret = BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]) - 1, args);
    EXPECT_EQ(ret, 0);
}
}  // namespace init_ut
