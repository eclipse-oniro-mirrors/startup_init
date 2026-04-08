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
 * @file begetctl_bootchart_test.cpp
 * @brief begetctl bootchart 命令单元测试
 *
 * 测试覆盖：
 * - bootchart enable - 启用 bootchart
 * - bootchart disable - 禁用 bootchart
 * - bootchart start - 启动 bootchart
 * - bootchart stop - 停止 bootchart
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
 * @brief bootchart 命令测试Fixture
 *
 * 测试启动图表命令：
 * 1. enable - 启用 bootchart
 * 2. disable - 禁用 bootchart
 * 3. start - 启动 bootchart
 * 4. stop - 停止 bootchart
 */
class BegetctlBootChartTest : public testing::Test {
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
// bootchart enable 命令测试
// ============================================================================

/**
 * @test BootChart_Enable_ExecutesSuccessfully
 * @brief 测试启用 bootchart
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlBootChartTest, BootChart_Enable_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    SystemWriteParam("persist.init.bootchart.enabled", "1");
    char arg0[] = "bootchart";
    char arg1[] = "enable";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "bootchart enable 命令应该成功执行";
}

// ============================================================================
// bootchart disable 命令测试
// ============================================================================

/**
 * @test BootChart_Disable_ExecutesSuccessfully
 * @brief 测试禁用 bootchart
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlBootChartTest, BootChart_Disable_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    SystemWriteParam("persist.init.bootchart.enabled", "0");
    char arg0[] = "bootchart";
    char arg1[] = "disable";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "bootchart disable 命令应该成功执行";
}

// ============================================================================
// bootchart start 命令测试
// ============================================================================

/**
 * @test BootChart_Start_ExecutesSuccessfully
 * @brief 测试启动 bootchart
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlBootChartTest, BootChart_Start_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "bootchart";
    char arg1[] = "start";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "bootchart start 命令应该成功执行";
}

// ============================================================================
// bootchart stop 命令测试
// ============================================================================

/**
 * @test BootChart_Stop_ExecutesSuccessfully
 * @brief 测试停止 bootchart
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlBootChartTest, BootChart_Stop_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "bootchart";
    char arg1[] = "stop";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "bootchart stop 命令应该成功执行";
}

// ============================================================================
// bootchart 默认命令测试
// ============================================================================

/**
 * @test BootChart_Default_ExecutesSuccessfully
 * @brief 测试无参数的 bootchart 命令（显示帮助或当前状态）
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlBootChartTest, BootChart_Default_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "bootchart";
    char* args[] = {arg0, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 1, args);

    // Assert
    EXPECT_EQ(ret, 0) << "bootchart 无参数命令应该成功执行";
}

} // namespace init_ut
