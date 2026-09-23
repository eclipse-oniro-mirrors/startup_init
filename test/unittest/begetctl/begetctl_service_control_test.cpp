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
 * @file begetctl_service_control_test.cpp
 * @brief begetctl service_control 命令单元测试
 *
 * 测试覆盖：
 * - service_control start/stop [service] - 启动/停止服务
 * - start_service/stop_service [service] - 简写服务控制
 * - timer_start/stop [service] [timeout] - 定时服务控制
 *
 * 改进点：
 * - 使用 AAA 模式（Arrange-Act-Assert）
 * - 描述性测试命名
 * - 测试隔离
 * - 边界条件覆盖
 */

#include "begetctl.h"
#include "init/service_control_test.h"
#include "init_param.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

namespace {
char g_serviceCtrlParamName[PARAM_NAME_LEN_MAX] = {0};
char g_serviceCtrlParamValue[PARAM_VALUE_LEN_MAX] = {0};

int RecordServiceCtrlParam(const char *name, const char *value)
{
    if (name == nullptr || value == nullptr) {
        return -1;
    }
    if (strncpy_s(g_serviceCtrlParamName, sizeof(g_serviceCtrlParamName), name,
        sizeof(g_serviceCtrlParamName) - 1) != EOK ||
        strncpy_s(g_serviceCtrlParamValue, sizeof(g_serviceCtrlParamValue), value,
        sizeof(g_serviceCtrlParamValue) - 1) != EOK) {
        return -1;
    }
    return 0;
}

void ResetServiceCtrlParam()
{
    (void)memset_s(g_serviceCtrlParamName, sizeof(g_serviceCtrlParamName), 0, sizeof(g_serviceCtrlParamName));
    (void)memset_s(g_serviceCtrlParamValue, sizeof(g_serviceCtrlParamValue), 0, sizeof(g_serviceCtrlParamValue));
}
} // namespace

/**
 * @brief service_control 命令测试Fixture
 *
 * 测试服务控制命令：
 * 1. service_control start/stop - 启动/停止服务
 * 2. start_service/stop_service - 简写形式
 * 3. timer_start/timer_stop - 定时服务控制
 */
class BegetctlServiceControlTest : public testing::Test {
protected:
    void SetUp() override
    {
        // Arrange: 每个测试前初始化Shell环境
        BShellParamCmdRegister(GetShellHandle(), 0);
        TestSetServiceControlParamFunc(nullptr);
        ResetServiceCtrlParam();
#ifdef SUPPORT_SA_MULTI_USER
        TestSetByUserSetParamFunc(nullptr);
#endif
    }

    void TearDown() override
    {
        // Cleanup: 清理测试状态
        TestSetServiceControlParamFunc(nullptr);
#ifdef SUPPORT_SA_MULTI_USER
        TestSetByUserSetParamFunc(nullptr);
#endif
    }
};

// ============================================================================
// service_control stop 命令测试
// ============================================================================

/**
 * @test ServiceControl_Stop_WithServiceName_ExecutesSuccessfully
 * @brief 测试停止服务
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, ServiceControl_Stop_WithServiceName_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "stop";
    char arg2[] = "test";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "service_control stop [服务名] 命令应该成功执行";
}

// ============================================================================
// service_control start 命令测试
// ============================================================================

/**
 * @test ServiceControl_Start_WithServiceName_ExecutesSuccessfully
 * @brief 测试启动服务
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, ServiceControl_Start_WithServiceName_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "start";
    char arg2[] = "test";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "service_control start [服务名] 命令应该成功执行";
}

// ============================================================================
// stop_service 命令测试
// ============================================================================

/**
 * @test StopService_WithServiceName_ExecutesSuccessfully
 * @brief 测试使用 stop_service 命令停止服务
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, StopService_WithServiceName_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "stop_service";
    char arg1[] = "test";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "stop_service [服务名] 命令应该成功执行";
}

// ============================================================================
// start_service 命令测试
// ============================================================================

/**
 * @test StartService_WithServiceName_ExecutesSuccessfully
 * @brief 测试使用 start_service 命令启动服务
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, StartService_WithServiceName_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "start_service";
    char arg1[] = "test";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "start_service [服务名] 命令应该成功执行";
}

/**
 * @test StartOnDemand_WithServiceName_WritesStartParameter
 * @brief 测试 service_control start_ondemand 命令启动按需服务
 *
 * 预期：命令返回0，发送按需启动控制参数及服务名
 */
HWTEST_F(BegetctlServiceControlTest, StartOnDemand_WithServiceName_WritesStartParameter, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "start_ondemand";
    char arg2[] = "init_ondemand_ut";
    char *args[] = {arg0, arg1, arg2, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start_ondemand");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_ondemand_ut");
}

/**
 * @test StartOnDemand_WithExtraArguments_ForwardsArguments
 * @brief 测试 service_control start_ondemand 命令传递扩展参数
 *
 * 预期：命令返回0，按顺序拼接并发送服务名和扩展参数
 */
HWTEST_F(BegetctlServiceControlTest, StartOnDemand_WithExtraArguments_ForwardsArguments, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "start_ondemand";
    char arg2[] = "init_ondemand_ut";
    char arg3[] = "event";
    char arg4[] = "payload";
    char *args[] = {arg0, arg1, arg2, arg3, arg4, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 5, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start_ondemand");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_ondemand_ut|event|payload");
}

/**
 * @test StartOnDemand_WithoutServiceName_DoesNotWriteParameter
 * @brief 测试 service_control start_ondemand 命令缺少服务名
 *
 * 预期：命令返回0，不发送服务控制参数
 */
HWTEST_F(BegetctlServiceControlTest, StartOnDemand_WithoutServiceName_DoesNotWriteParameter, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "start_ondemand";
    char *args[] = {arg0, arg1, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "");
    EXPECT_STREQ(g_serviceCtrlParamValue, "");
}

/**
 * @test StartOnDemandService_WithServiceName_WritesStartParameter
 * @brief 测试 start_ondemand_service 简写命令启动按需服务
 *
 * 预期：命令返回0，发送按需启动控制参数及服务名
 */
HWTEST_F(BegetctlServiceControlTest, StartOnDemandService_WithServiceName_WritesStartParameter, TestSize.Level1)
{
    // Arrange
    char arg0[] = "start_ondemand_service";
    char arg1[] = "init_ondemand_ut";
    char *args[] = {arg0, arg1, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start_ondemand");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_ondemand_ut");
}

/**
 * @test StartOnDemandService_WithExtraArguments_ForwardsArguments
 * @brief 测试 start_ondemand_service 简写命令传递扩展参数
 *
 * 预期：命令返回0，按顺序拼接并发送服务名和扩展参数
 */
HWTEST_F(BegetctlServiceControlTest, StartOnDemandService_WithExtraArguments_ForwardsArguments, TestSize.Level1)
{
    // Arrange
    char arg0[] = "start_ondemand_service";
    char arg1[] = "init_ondemand_ut";
    char arg2[] = "event";
    char arg3[] = "payload";
    char *args[] = {arg0, arg1, arg2, arg3, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start_ondemand");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_ondemand_ut|event|payload");
}

/**
 * @test StartOnDemandService_WithoutServiceName_DoesNotWriteParameter
 * @brief 测试 start_ondemand_service 简写命令缺少服务名
 *
 * 预期：命令返回0，不发送服务控制参数
 */
HWTEST_F(BegetctlServiceControlTest, StartOnDemandService_WithoutServiceName_DoesNotWriteParameter, TestSize.Level1)
{
    // Arrange
    char arg0[] = "start_ondemand_service";
    char *args[] = {arg0, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "");
    EXPECT_STREQ(g_serviceCtrlParamValue, "");
}

/**
 * @test ServiceControl_Start_WritesOrdinaryStartParameter
 * @brief 测试普通 service_control start 命令不受按需启动入口影响
 *
 * 预期：命令返回0，仍发送 ohos.ctl.start 控制参数及服务名
 */
HWTEST_F(BegetctlServiceControlTest, ServiceControl_Start_WritesOrdinaryStartParameter, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "start";
    char arg2[] = "init_ondemand_ut";
    char *args[] = {arg0, arg1, arg2, nullptr};

    TestSetServiceControlParamFunc(RecordServiceCtrlParam);

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_ondemand_ut");
}

#ifdef SUPPORT_SA_MULTI_USER
HWTEST_F(BegetctlServiceControlTest, StartWithUserId_WritesByUserStartParameter, TestSize.Level1)
{
    char arg0[] = "startwithuserid";
    char arg1[] = "init_by_user_ut";
    char arg2[] = "401";
    char arg3[] = "boot_event";
    char *args[] = {arg0, arg1, arg2, arg3, nullptr};

    TestSetByUserSetParamFunc(RecordServiceCtrlParam);
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start.userid");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_by_user_ut|401|boot_event");
}

/*
 * @tc.name: StartWithUserId_001
 * @tc.desc: Verify that missing and malformed user IDs are rejected without writing the control parameter.
 * @tc.type: FUNC
 */
HWTEST_F(BegetctlServiceControlTest, StartWithUserId_001, TestSize.Level2)
{
    char command[] = "startwithuserid";
    char service[] = "init_by_user_ut";
    char emptyUser[] = "";
    char trailingUser[] = "401x";
    char negativeUser[] = "-1";
    char overflowUser[] = "999999999999999999999999999999999999";
    char aboveMaxUser[] = "2147483648";

    TestSetByUserSetParamFunc(RecordServiceCtrlParam);
    char *missingArgs[] = {command, service, nullptr};
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 2, missingArgs), 0);

    char *emptyArgs[] = {command, service, emptyUser, nullptr};
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 3, emptyArgs), 0);
    char *trailingArgs[] = {command, service, trailingUser, nullptr};
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 3, trailingArgs), 0);
    char *negativeArgs[] = {command, service, negativeUser, nullptr};
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 3, negativeArgs), 0);
    char *overflowArgs[] = {command, service, overflowUser, nullptr};
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 3, overflowArgs), 0);
    char *aboveMaxArgs[] = {command, service, aboveMaxUser, nullptr};
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 3, aboveMaxArgs), 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "");
    EXPECT_STREQ(g_serviceCtrlParamValue, "");
}

/*
 * @tc.name: StartWithUserId_002
 * @tc.desc: Verify that INT32_MAX and multiple extension arguments are forwarded unchanged.
 * @tc.type: FUNC
 */
HWTEST_F(BegetctlServiceControlTest, StartWithUserId_002, TestSize.Level2)
{
    char command[] = "startwithuserid";
    char service[] = "init_by_user_ut";
    char maxUser[] = "2147483647";
    char firstExt[] = "first";
    char secondExt[] = "second";
    char *args[] = {command, service, maxUser, firstExt, secondExt, nullptr};

    TestSetByUserSetParamFunc(RecordServiceCtrlParam);
    EXPECT_EQ(BShellEnvDirectExecute(GetShellHandle(), 5, args), 0);
    EXPECT_STREQ(g_serviceCtrlParamName, "ohos.ctl.start.userid");
    EXPECT_STREQ(g_serviceCtrlParamValue, "init_by_user_ut|2147483647|first|second");
}
#endif

// ============================================================================
// timer_stop 命令测试
// ============================================================================

/**
 * @test TimerStop_WithServiceName_ExecutesSuccessfully
 * @brief 测试停止定时启动服务
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, TimerStop_WithServiceName_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_stop";
    char arg1[] = "test";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "timer_stop [服务名] 命令应该成功执行";
}

/**
 * @test TimerStop_NoArguments_HandlesGracefully
 * @brief 测试无参数的 timer_stop 命令（边界条件）
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlServiceControlTest, TimerStop_NoArguments_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_stop";
    char* args[] = {arg0, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);

    // Assert
    EXPECT_EQ(ret, 0) << "timer_stop 无参数命令应该执行完成";
}

// ============================================================================
// timer_start 命令测试
// ============================================================================

/**
 * @test TimerStart_WithTimeout_ExecutesSuccessfully
 * @brief 测试定时启动服务（带超时参数）
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, TimerStart_WithTimeout_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_start";
    char arg1[] = "test-service";
    char arg2[] = "10";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "timer_start [服务名] [超时] 命令应该成功执行";
}

/**
 * @test TimerStart_NoTimeout_ExecutesSuccessfully
 * @brief 测试定时启动服务（无超时参数）
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlServiceControlTest, TimerStart_NoTimeout_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_start";
    char arg1[] = "test-service";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "timer_start [服务名] 命令应该成功执行";
}

/**
 * @test TimerStart_InvalidTimeout_HandlesGracefully
 * @brief 测试定时启动服务（无效超时参数）
 *
 * 预期：命令执行，不崩溃
 */
HWTEST_F(BegetctlServiceControlTest, TimerStart_InvalidTimeout_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "timer_start";
    char arg1[] = "test-service";
    char arg2[] = "ww";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "timer_start 无效超时参数应该被优雅处理";
}

// ============================================================================
// 无效命令处理测试
// ============================================================================

/**
 * @test ServiceControl_InvalidCommand_HandlesGracefully
 * @brief 测试无效的 service_control 命令（错误处理）
 *
 * 预期：命令执行，不崩溃
 */
HWTEST_F(BegetctlServiceControlTest, ServiceControl_InvalidCommand_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "service_control";
    char arg1[] = "invalid_cmd";
    char arg2[] = "test";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert: 应该执行完成，不崩溃
    EXPECT_EQ(ret, 0) << "无效命令应该被优雅处理";
}

} // namespace init_ut
