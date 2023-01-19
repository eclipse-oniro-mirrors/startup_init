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
    static void SetUpTestCase(void)
    {
        PrepareInitUnitTestEnv();
    };
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(BegetctlUnitTest, TestShellInit, TestSize.Level0)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param"
    };
    BShellEnvDirectExecute(GetShellHandle(), 1, const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellLs, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "ls"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellLsWithR, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "ls", "-r"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellLsGet, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "get"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellSet, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "set", "aaaaa", "1234567"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellGetWithKey, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "get", "aaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellWait, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait", "aaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, TestShellWaitFalse, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestShellWaitWithKey, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait", "aaaaa", "12*", "30"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, TestShellParamShell, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "shell"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, TestShellLsWithvalue, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "..a", PARAM_STRING, (void *)"..a");
    const char *args[] = {
        "param", "ls", PARAM_REVERESD_NAME_CURR_PARAMETER
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
HWTEST_F(BegetctlUnitTest, TestShellLsWithvalueExist, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "#", PARAM_STRING, (void *)"#");
    const char *args[] = {
        "param", "ls", "-r", PARAM_REVERESD_NAME_CURR_PARAMETER
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_1, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "getslot"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_2, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "getsuffix", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_3, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setactive", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_4, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setunboot", "2"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_5, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setactive"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_6, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "setunboot"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestPartitionSlot_7, TestSize.Level1)
{
    const char *args[] = {
        "partitionslot", "getsuffix"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBegetctl_1, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBegetctl_2, TestSize.Level1)
{
    const char *args[] = {
        "get", "log", "level"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBegetctl_3, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBegetctl_4, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level", "1000"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBegetctl_5, TestSize.Level1)
{
    const char *args[] = {
        "set", "log", "level", "a"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBootChart_1, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "enable"
    };
    SystemWriteParam("persist.init.bootchart.enabled", "1");
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBootChart_2, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "start"
    };

    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBootChart_3, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "stop"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBootChart_4, TestSize.Level1)
{
    const char *args[] = {
        "bootchart", "disable"
    };
    SystemWriteParam("persist.init.bootchart.enabled", "0");
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestBootChart_5, TestSize.Level1)
{
    const char *args[] = {
        "bootchart"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestDumpService_1, TestSize.Level1)
{
    const char *args[] = {
        "bootevent", "enable"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestDumpService_2, TestSize.Level1)
{
    const char *args[] = {
        "bootevent", "disable"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestDumpService_3, TestSize.Level1)
{
    const char *args[] = {
        "dump_service", "all"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestDumpService_4, TestSize.Level1)
{
    const char *args[] = {
        "dump_service", "param_watcher"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestDumpService_5, TestSize.Level1)
{
    const char *args[] = {
        "dump_service", "parameter-service", "trigger"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestMiscDaemon, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo", BOOT_CMD_LINE
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestMiscDaemon_1, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo1111", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestMiscDaemon_2, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo", ""
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestMiscDaemon_3, TestSize.Level1)
{
    const char *args[] = {
        "misc_daemon", "--write_logo"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestMiscDaemon_4, TestSize.Level1)
{
    // clear misc logo
    const char *args[] = {
        "misc_daemon", "--write_logo", "sssssssss"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestModulectl_1, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "install", "testModule"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestModulectl_2, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "uninstall", "testModule"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestModulectl_3, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "list", "testModule"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestModulectl_4, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "install"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestModulectl_5, TestSize.Level1)
{
    const char *args[] = {
        "modulectl", "list"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_1, TestSize.Level1)
{
    const char *args[] = {
        "service_control", "stop", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_2, TestSize.Level1)
{
    const char *args[] = {
        "service_control", "start", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_3, TestSize.Level1)
{
    const char *args[] = {
        "stop_service", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_4, TestSize.Level1)
{
    const char *args[] = {
        "start_service", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_5, TestSize.Level1)
{
    const char *args[] = {
        "timer_stop", "test"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_6, TestSize.Level1)
{
    const char *args[] = {
        "timer_stop"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_7, TestSize.Level1)
{
    const char *args[] = {
        "timer_start", "test-service", "10"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_8, TestSize.Level1)
{
    const char *args[] = {
        "timer_start", "test-service",
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_9, TestSize.Level1)
{
    const char *args[] = {
        "timer_start", "test-service", "ww"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestServiceControl_10, TestSize.Level1)
{
    const char *args[] = {
        "xxxxxxxxxxxxxx", "test-service", "ww"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestSetLogLevel_1, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel", "1"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestSetLogLevel_2, TestSize.Level1)
{
    const char *args[] = {
        "getloglevel"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestSetLogLevel_3, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel", "a"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestSetLogLevel_4, TestSize.Level1)
{
    const char *args[] = {
        "setloglevel"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestSandbox, TestSize.Level1)
{
    const char *args[] = {
        "sandbox", "-s", "test", "-n", "test2", "-p", "test3", "-h", "?"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_1, TestSize.Level1)
{
    const char *args[] = {
        "reboot"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_2, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "shutdown"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_3, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "charge"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_4, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "updater"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_5, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "updater:aaaaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_6, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "flashd:aaaaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_7, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "flashd"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_8, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "suspend"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}

HWTEST_F(BegetctlUnitTest, TestReboot_9, TestSize.Level1)
{
    const char *args[] = {
        "reboot", "222222222"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), const_cast<char **>(args));
}
}  // namespace init_ut
