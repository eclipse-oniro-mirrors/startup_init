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
    const char *args[] = {
        "param"
    };
    BShellEnvDirectExecute(GetShellHandle(), 1, const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLs_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "ls"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLsWithR_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "ls", "-r"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLsGet_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "get"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellSet_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "set", "aaaaa", "1234567"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellGetWithKey_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "get", "aaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellWait_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait", "aaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, Init_TestShellWaitFalse_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellWaitWithKey_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait", "aaaaa", "12*", "30"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, Init_TestShellParamShell_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "shell"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, Init_TestShellLsWithvalue_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "..a", PARAM_STRING, (void *)"..a");
    const char *args[] = {
        "param", "ls", PARAM_REVERESD_NAME_CURR_PARAMETER
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, Init_TestShellLsWithvalueExist_001, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "#", PARAM_STRING, (void *)"#");
    const char *args[] = {
        "param", "ls", "-r", PARAM_REVERESD_NAME_CURR_PARAMETER
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_001, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "getslot"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_002, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "getsuffix", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_003, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setactive", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_004, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setunboot", "2"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_005, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setactive"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_006, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setunboot"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestPartitionSlot_007, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "getsuffix"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_001, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_002, TestSize.Level1)
{
    const char *args[] = {
        "get", "log", "level"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_003, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_004, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level", "1000"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestShellLog_005, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level", "a"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_001, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "enable"
    };
    SystemWriteParam("persist.init.bootchart.enabled", "1");
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_002, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "start"
    };

    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_003, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "stop"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_004, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "disable"
    };
    SystemWriteParam("persist.init.bootchart.enabled", "0");
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestBootChart_005, TestSize.Level1)
{
    const char *args[] = {
        "bootchart"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_001, TestSize.Level1)
{
    const char *args[] = {
        "bootevent", "enable"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_002, TestSize.Level1)
{
    const char *args[] = {
        "bootevent", "disable"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_003, TestSize.Level1)
{
    const char *args[] = {
        "dump_service", "all"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_004, TestSize.Level1)
{
    const char *args[] = {
        "dump_service", "param_watcher"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpService_005, TestSize.Level1)
{
    const char *args[] = {
        "dump_service", "parameter-service", "trigger"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpNwebSpawn_001, TestSize.Level1)
{
    const char *args[] = {
        "dump_nwebspawn", ""
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestDumpAppspawn_001, TestSize.Level1)
{
    const char *args[] = {
        "dump_appspawn", ""
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_001, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo", BOOT_CMD_LINE
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_002, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo1111", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_003, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo", ""
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_004, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestMiscDaemon_005, TestSize.Level1)
{
    // clear misc logo
    const char *args[] = {
        "misc_daemon", "--write_logo", "sssssssss"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_001, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "install", "testModule"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_002, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "uninstall", "testModule"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_003, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "list", "testModule"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_004, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "install"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_005, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "list"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestModulectl_006, TestSize.Level1)
{
    BShellEnvDirectExecute(GetShellHandle(), 0, nullptr);
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_001, TestSize.Level1)
{
    const char *args[] = {
        "service_control", "stop", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_002, TestSize.Level1)
{
    const char *args[] = {
        "service_control", "start", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_003, TestSize.Level1)
{
    const char *args[] = {
        "stop_service", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_004, TestSize.Level1)
{
    const char *args[] = {
        "start_service", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_005, TestSize.Level1)
{
    const char *args[] = {
        "timer_stop", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_006, TestSize.Level1)
{
    const char *args[] = {
        "timer_stop"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_007, TestSize.Level1)
{
    const char *args[] = {
        "timer_start", "test-service", "10"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_008, TestSize.Level1)
{
    const char *args[] = {
        "timer_start", "test-service",
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_009, TestSize.Level1)
{
    const char *args[] = {
        "timer_start", "test-service", "ww"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestServiceControl_010, TestSize.Level1)
{
    const char *args[] = {
        "xxxxxxxxxxxxxx", "test-service", "ww"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_001, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_002, TestSize.Level1)
{
    const char *args[] = {
        "getloglevel"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_003, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel", "a"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_004, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSetLogLevel_005, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel"
    };
    BShellEnvDirectExecute(GetShellHandle(), 0, const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSandbox_001, TestSize.Level1)
{
    const char *args[] = {
        "sandbox", "-s", "test", "-n", "test2", "-p", "test3", "-h", "?"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestSandbox_002, TestSize.Level1)
{
    const char *args[] = {
        "sandbox", "-b", "1008"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_001, TestSize.Level1)
{
    const char *args[] = {
        "reboot"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_002, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "shutdown"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_003, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "charge"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_004, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "updater"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_005, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "updater:aaaaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_006, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "flashd:aaaaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_007, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "flashd"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_008, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "suspend"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestReboot_009, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "222222222"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestGid_001, TestSize.Level1)
{
    const char *args[] = {
        "dac", "gid", "logd"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestGid_002, TestSize.Level1)
{
    const char *args[] = {
        "dac"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestUid_001, TestSize.Level1)
{
    const char *args[] = {
        "dac", "uid", ""
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, Init_TestUid_002, TestSize.Level1)
{
    const char *args[] = {
        "dac", "uid", ""
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
}  // namespace init_ut
