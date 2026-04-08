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
 * @file begetctl_partitionslot_test.cpp
 * @brief begetctl partitionslot 命令单元测试
 *
 * 测试覆盖：
 * - partitionslot getslot - 获取当前分区槽位
 * - partitionslot getsuffix [slot] - 获取槽位后缀
 * - partitionslot setactive [slot] - 设置活动槽位
 * - partitionslot setunboot [slot] - 设置槽位不可启动
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
 * @brief partitionslot 命令测试Fixture
 *
 * 测试分区槽位管理命令：
 * 1. getslot - 获取当前分区槽位
 * 2. getsuffix [slot] - 获取槽位后缀
 * 3. setactive [slot] - 设置活动槽位
 * 4. setunboot [slot] - 设置槽位不可启动
 */
class BegetctlPartitionSlotTest : public testing::Test {
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
// partitionslot getslot 命令测试
// ============================================================================

/**
 * @test PartitionSlot_GetSlot_ExecutesSuccessfully
 * @brief 测试获取当前分区槽位
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_GetSlot_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "getslot";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot getslot 命令应该成功执行";
}

// ============================================================================
// partitionslot getsuffix 命令测试
// ============================================================================

/**
 * @test PartitionSlot_GetSuffix_WithValidSlot_ExecutesSuccessfully
 * @brief 测试获取指定槽位的后缀
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_GetSuffix_WithValidSlot_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "getsuffix";
    char arg2[] = "1";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot getsuffix [slot] 命令应该成功执行";
}

/**
 * @test PartitionSlot_GetSuffix_NoArguments_HandlesGracefully
 * @brief 测试无参数的 getsuffix 命令（边界条件）
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_GetSuffix_NoArguments_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "getsuffix";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot getsuffix 无参数命令应该执行完成";
}

// ============================================================================
// partitionslot setactive 命令测试
// ============================================================================

/**
 * @test PartitionSlot_SetActive_WithValidSlot_ExecutesSuccessfully
 * @brief 测试设置活动分区槽位
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_SetActive_WithValidSlot_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "setactive";
    char arg2[] = "1";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot setactive [slot] 命令应该成功执行";
}

/**
 * @test PartitionSlot_SetActive_NoArguments_HandlesGracefully
 * @brief 测试无参数的 setactive 命令（边界条件）
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_SetActive_NoArguments_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "setactive";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot setactive 无参数命令应该执行完成";
}

// ============================================================================
// partitionslot setunboot 命令测试
// ============================================================================

/**
 * @test PartitionSlot_SetUnboot_WithValidSlot_ExecutesSuccessfully
 * @brief 测试设置分区槽位为不可启动
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_SetUnboot_WithValidSlot_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "setunboot";
    char arg2[] = "2";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot setunboot [slot] 命令应该成功执行";
}

/**
 * @test PartitionSlot_SetUnboot_NoArguments_HandlesGracefully
 * @brief 测试无参数的 setunboot 命令（边界条件）
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlPartitionSlotTest, PartitionSlot_SetUnboot_NoArguments_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "partitionslot";
    char arg1[] = "setunboot";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "partitionslot setunboot 无参数命令应该执行完成";
}

} // namespace init_ut
