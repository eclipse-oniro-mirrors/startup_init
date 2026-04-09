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
 * @file begetctl_advanced_coverage_test.cpp
 * @brief begetctl 高级覆盖率测试 - 边界条件、错误路径、特殊场景
 *
 * 目的：将 begetctl 模块的代码覆盖率提升至 90%+
 * 覆盖范围：
 * - 边界条件测试
 * - 错误处理路径
 * - 特殊参数组合
 * - 极限输入测试
 */

#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

/**
 * @brief 高级覆盖率测试Fixture
 */
class BegetctlAdvancedCoverageTest : public testing::Test {
protected:
    void SetUp() override
    {
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override {}
};

// ============================================================================
// Bootchart 高级测试 - 覆盖更多分支
// ============================================================================

/**
 * @test BootChart_Start_WhenNotEnabled_ShowsMessage
 * @brief 测试在 bootchart 未启用时执行 start 命令
 * 覆盖：bootchartCmdStart 中的错误消息分支
 */
HWTEST_F(BegetctlAdvancedCoverageTest, BootChart_Start_WhenNotEnabled_ShowsMessage, TestSize.Level1)
{
    // Arrange: 确保 bootchart 未启用
    SystemWriteParam("persist.init.bootchart.enabled", "0");
    char arg0[] = "bootchart";
    char arg1[] = "start";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test BootChart_WithInvalidParameter_HandlesGracefully
 * @brief 测试 bootchart 无效参数处理
 */
HWTEST_F(BegetctlAdvancedCoverageTest, BootChart_WithInvalidParameter_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "bootchart";
    char arg1[] = "invalid_cmd";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Dump Service 高级测试
// ============================================================================

/**
 * @test DumpService_ParameterService_Trigger_ExecutesSuccessfully
 * @brief 测试 parameter_service trigger 命令的完整路径
 * 覆盖：main_cmd 中 DUMP_SERVICE_BOOTEVENT_CMD_ARGS 分支
 */
HWTEST_F(BegetctlAdvancedCoverageTest, DumpService_ParameterService_Trigger_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "dump_service";
    char arg1[] = "parameter_service";
    char arg2[] = "trigger";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test DumpService_WithLongServiceName_HandlesCorrectly
 * @brief 测试长服务名处理（边界条件）
 */
HWTEST_F(BegetctlAdvancedCoverageTest, DumpService_WithLongServiceName_HandlesCorrectly, TestSize.Level1)
{
    // Arrange: 使用较长的服务名
    char arg0[] = "dump_service";
    char arg1[] = "very_long_service_name_that_tests_boundary";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test DumpService_WithSpecialCharacters_HandlesCorrectly
 * @brief 测试包含特殊字符的服务名
 */
HWTEST_F(BegetctlAdvancedCoverageTest, DumpService_WithSpecialCharacters_HandlesCorrectly, TestSize.Level1)
{
    // Arrange: 服务名包含下划线和数字
    char arg0[] = "dump_service";
    char arg1[] = "test_service_123";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Modulectl 高级测试
// ============================================================================

/**
 * @test Modulectl_Uninstall_WithModuleName_ExecutesSuccessfully
 * @brief 测试 modulectl uninstall 命令
 * 覆盖：ModuleInstallCmd 分支
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Modulectl_Uninstall_WithModuleName_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "modulectl";
    char arg1[] = "uninstall";
    char arg2[] = "test_module";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Modulectl_List_WithNoArgs_ExecutesSuccessfully
 * @brief 测试 modulectl list 无参数执行
 * 覆盖：ModuleDisplayCmd 分支
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Modulectl_List_WithNoArgs_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "modulectl";
    char arg1[] = "list";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Modulectl_WithInvalidSubCommand_HandlesGracefully
 * @brief 测试 modulectl 无效子命令
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Modulectl_WithInvalidSubCommand_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "modulectl";
    char arg1[] = "invalid_subcmd";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Service Control 高级测试
// ============================================================================

/**
 * @test ServiceControl_TimerStart_WithNumericTimeout_ExecutesSuccessfully
 * @brief 测试 timer_start 带数字超时参数
 * 覆盖：main_cmd 中 strtoull 分支
 */
HWTEST_F(BegetctlAdvancedCoverageTest, ServiceControl_TimerStart_WithNumericTimeout_ExecutesSuccessfully,
    TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_start";
    char arg1[] = "test_service";
    char arg2[] = "5000";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test ServiceControl_TimerStart_WithZeroTimeout_HandlesCorrectly
 * @brief 测试 timer_start 零超时（边界条件）
 */
HWTEST_F(BegetctlAdvancedCoverageTest, ServiceControl_TimerStart_WithZeroTimeout_HandlesCorrectly, TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_start";
    char arg1[] = "test_service";
    char arg2[] = "0";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test ServiceControl_StartService_WithExtraArgs_ExecutesSuccessfully
 * @brief 测试 start_service 带额外参数
 * 覆盖：main_cmd 中 ServiceControlWithExtra 调用
 */
HWTEST_F(BegetctlAdvancedCoverageTest, ServiceControl_StartService_WithExtraArgs_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "start_service";
    char arg1[] = "test_service";
    char arg2[] = "extra_arg";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test ServiceControl_StopService_WithExtraArgs_ExecutesSuccessfully
 * @brief 测试 stop_service 带额外参数
 */
HWTEST_F(BegetctlAdvancedCoverageTest, ServiceControl_StopService_WithExtraArgs_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "stop_service";
    char arg1[] = "test_service";
    char arg2[] = "extra_arg";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// DAC 高级测试
// ============================================================================

/**
 * @test Dac_Uid_WithRootUser_ExecutesSuccessfully
 * @brief 测试获取 root 用户 UID
 * 覆盖：GetUidByName 中 getpwnam 调用
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Dac_Uid_WithRootUser_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "dac";
    char arg1[] = "uid";
    char arg2[] = "root";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Dac_Uid_WithSystemUser_ExecutesSuccessfully
 * @brief 测试获取 system 用户 UID
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Dac_Uid_WithSystemUser_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "dac";
    char arg1[] = "uid";
    char arg2[] = "system";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Dac_Gid_WithSystemGroup_ExecutesSuccessfully
 * @brief 测试获取 system 组 GID
 * 覆盖：GetGidByName 中 getgrnam 调用
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Dac_Gid_WithSystemGroup_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "dac";
    char arg1[] = "gid";
    char arg2[] = "system";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Dac_Gid_WithShellGroup_ExecutesSuccessfully
 * @brief 测试获取 shell 组 GID
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Dac_Gid_WithShellGroup_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "dac";
    char arg1[] = "gid";
    char arg2[] = "shell";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Log Level 高级测试
// ============================================================================

/**
 * @test SetLogLevel_AllValidLevels_ExecutesSuccessfully
 * @brief 测试所有有效的日志级别
 * 覆盖：SetInitLogLevelFromParam 中所有有效级别分支
 */
HWTEST_F(BegetctlAdvancedCoverageTest, SetLogLevel_Level0_Debug_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange: DEBUG level = 0
    char arg0[] = "setloglevel";
    char arg1[] = "0";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlAdvancedCoverageTest, SetLogLevel_Level2_Warning_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange: WARNING level = 2
    char arg0[] = "setloglevel";
    char arg1[] = "2";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlAdvancedCoverageTest, SetLogLevel_Level3_Error_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange: ERROR level = 3
    char arg0[] = "setloglevel";
    char arg1[] = "3";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

HWTEST_F(BegetctlAdvancedCoverageTest, SetLogLevel_Level4_Fatal_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange: FATAL level = 4
    char arg0[] = "setloglevel";
    char arg1[] = "4";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test SetLogLevel_WithInvalidNumber_HandlesGracefully
 * @brief 测试超出范围的日志级别
 * 覆盖：SetInitLogLevelFromParam 中无效级别处理
 */
HWTEST_F(BegetctlAdvancedCoverageTest, SetLogLevel_WithInvalidNumber_HandlesGracefully, TestSize.Level1)
{
    // Arrange: 超出范围的级别值
    char arg0[] = "setloglevel";
    char arg1[] = "999";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test SetLogLevel_WithNegativeNumber_HandlesGracefully
 * @brief 测试负数日志级别
 */
HWTEST_F(BegetctlAdvancedCoverageTest, SetLogLevel_WithNegativeNumber_HandlesGracefully, TestSize.Level1)
{
    // Arrange: 负数级别值
    char arg0[] = "setloglevel";
    char arg1[] = "-1";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Reboot 高级测试
// ============================================================================

/**
 * @test Reboot_WithUpdaterOptions_ExecutesSuccessfully
 * @brief 测试 updater:options 格式
 * 覆盖：init_cmd_reboot.c 中带选项的 reboot 命令
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Reboot_WithUpdaterOptions_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "reboot";
    char arg1[] = "updater:test_option";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Reboot_WithFlashdOptions_ExecutesSuccessfully
 * @brief 测试 flashd:options 格式
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Reboot_WithFlashdOptions_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "reboot";
    char arg1[] = "flashd:test_option";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Reboot_WithShutdownColon_ExecutesSuccessfully
 * @brief 测试 shutdown:options 格式
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Reboot_WithShutdownColon_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "reboot";
    char arg1[] = "shutdown:force";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Param Shell 高级测试
// ============================================================================

/**
 * @test ParamShell_WithEmptyParameter_HandlesGracefully
 * @brief 测试带空参数的 param shell
 * 覆盖：param_cmd.c 中的边界条件处理
 */
HWTEST_F(BegetctlAdvancedCoverageTest, ParamShell_WithEmptyParameter_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "get";
    char arg2[] = "";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Param_WithMultipleDashR_HandlesCorrectly
 * @brief 测试多个 -r 参数
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Param_WithMultipleDashR_HandlesCorrectly, TestSize.Level1)
{
    // Arrange
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "ls";
    char arg2[] = "-r";
    char arg3[] = "-r";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// BootEvent 高级测试
// ============================================================================

/**
 * @test BootEvent_Enable_MultipleTimes_ExecutesSuccessfully
 * @brief 测试多次执行 bootevent enable
 */
HWTEST_F(BegetctlAdvancedCoverageTest, BootEvent_Enable_MultipleTimes_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "bootevent";
    char arg1[] = "enable";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret1 = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    int ret2 = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
}

/**
 * @test BootEvent_Disable_MultipleTimes_ExecutesSuccessfully
 * @brief 测试多次执行 bootevent disable
 */
HWTEST_F(BegetctlAdvancedCoverageTest, BootEvent_Disable_MultipleTimes_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "bootevent";
    char arg1[] = "disable";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret1 = BShellEnvDirectExecute(GetShellHandle(), 2, args);
    int ret2 = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
}

// ============================================================================
// Appspawn Time 高级测试
// ============================================================================

/**
 * @test AppspawnTime_WithNoParameters_HandlesGracefully
 * @brief 测试 appspawn_time 无参数
 * 覆盖：appspawntime_cmd.c 中的 argc 检查
 */
HWTEST_F(BegetctlAdvancedCoverageTest, AppspawnTime_WithNoParameters_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "appspawn_time";
    char* args[] = {arg0, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Sandbox 高级测试
// ============================================================================

/**
 * @test Sandbox_WithMultipleOptions_ExecutesSuccessfully
 * @brief 测试 sandbox 带多个选项
 * 覆盖：sandbox.cpp 中的多选项处理
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Sandbox_WithAllOptions_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange: 使用所有可用选项
    char arg0[] = "sandbox";
    char arg1[] = "-s";
    char arg2[] = "test";
    char arg3[] = "-n";
    char arg4[] = "name";
    char arg5[] = "-p";
    char arg6[] = "pid";
    char* args[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 7, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Misc Daemon 高级测试
// ============================================================================

/**
 * @test MiscDaemon_WithDifferentOptions_ExecutesSuccessfully
 * @brief 测试 misc_daemon 不同选项
 */
HWTEST_F(BegetctlAdvancedCoverageTest, MiscDaemon_WithEmptyLogoValue_HandlesCorrectly, TestSize.Level1)
{
    // Arrange
    char arg0[] = "misc_daemon";
    char arg1[] = "--write_logo";
    char arg2[] = "";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// 边界条件和极限输入测试
// ============================================================================

/**
 * @test ServiceControl_WithVeryLongServiceName_HandlesCorrectly
 * @brief 测试超长服务名处理
 */
HWTEST_F(BegetctlAdvancedCoverageTest, ServiceControl_WithVeryLongServiceName_HandlesCorrectly, TestSize.Level1)
{
    // Arrange: 创建较长的服务名
    char arg0[] = "service_control";
    char arg1[] = "start";
    char arg2[] = "a_very_long_service_name_for_testing_boundary_conditions";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test Param_Set_WithVeryLongValue_HandlesCorrectly
 * @brief 测试超长参数值处理
 */
HWTEST_F(BegetctlAdvancedCoverageTest, Param_Set_WithVeryLongValue_HandlesCorrectly, TestSize.Level1)
{
    // Arrange
    BShellParamCmdRegister(GetShellHandle(), 0);
    char arg0[] = "param";
    char arg1[] = "set";
    char arg2[] = "test.long.parameter";
    char arg3[] = "this_is_a_very_long_value_for_testing_boundary_conditions_in_param_set";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

/**
 * @test PartitionSlot_WithInvalidSlot_HandlesGracefully
 * @brief 测试无效分区槽号
 */
HWTEST_F(BegetctlAdvancedCoverageTest, PartitionSlot_WithInvalidSlot_HandlesGracefully, TestSize.Level1)
{
    // Arrange: 无效槽号
    char arg0[] = "partitionslot";
    char arg1[] = "setactive";
    char arg2[] = "999";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
}

} // namespace init_ut
