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
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

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
    }

    void TearDown() override
    {
        // Cleanup: 清理测试状态
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
