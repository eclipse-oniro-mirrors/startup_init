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
 * @file begetctl_param_basic_test.cpp
 * @brief begetctl param 基础命令单元测试
 *
 * 测试覆盖：
 * - param ls: 列出参数
 * - param get: 获取参数值
 * - param set: 设置参数值
 * - param wait: 等待参数变化
 * - param shell: 进入参数shell模式
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
 * @brief param 基础命令测试Fixture
 *
 * 测试 param 命令的基础功能：
 * 1. param ls [-r] [name] - 列出参数
 * 2. param get [name] - 获取参数值
 * 3. param set name value - 设置参数值
 * 4. param wait name [value] [timeout] - 等待参数变化
 * 5. param shell - 进入参数shell模式
 */
class BegetctlParamBasicTest : public testing::Test {
protected:
    void SetUp() override
    {
        // Arrange: 每个测试前初始化Shell环境
        BShellParamCmdRegister(GetShellHandle(), 0);
    }

    void TearDown() override
    {
        // Cleanup: 清理测试状态（如需要）
    }
};

// ============================================================================
// param ls 命令测试
// ============================================================================

/**
 * @test ParamLs_NoArguments_ExecutesSuccessfully
 * @brief 测试无参数的 param ls 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamLs_NoArguments_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "ls";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param ls 命令应该成功执行";
}

/**
 * @test ParamLs_WithRecursiveFlag_ExecutesSuccessfully
 * @brief 测试带 -r 参数的 param ls 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamLs_WithRecursiveFlag_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "ls";
    char arg2[] = "-r";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param ls -r 命令应该成功执行";
}

/**
 * @test ParamLs_WithSpecificParameter_ExecutesSuccessfully
 * @brief 测试指定参数名的 param ls 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamLs_WithSpecificParameter_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER,
                      "test desc", PARAM_STRING, (void*)"..a");

    char arg0[] = "param";
    char arg1[] = "ls";
    char arg2[] = PARAM_REVERESD_NAME_CURR_PARAMETER;
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param ls [参数名] 命令应该成功执行";
}

// ============================================================================
// param get 命令测试
// ============================================================================

/**
 * @test ParamGet_NoArguments_ExecutesSuccessfully
 * @brief 测试无参数的 param get 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamGet_NoArguments_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "get";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param get 命令应该成功执行";
}

/**
 * @test ParamGet_WithValidKey_ExecutesSuccessfully
 * @brief 测试带有效键名的 param get 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamGet_WithValidKey_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "get";
    char arg2[] = "test.param.name";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param get [键名] 命令应该成功执行";
}

/**
 * @test ParamGet_WithEmptyKey_HandlesGracefully
 * @brief 测试空键名的 param get 命令（边界条件）
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlParamBasicTest, ParamGet_WithEmptyKey_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "get";
    char arg2[] = "";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param get '' 空键名命令应该执行完成";
}

// ============================================================================
// param set 命令测试
// ============================================================================

/**
 * @test ParamSet_WithValidKeyValue_ExecutesSuccessfully
 * @brief 测试设置有效键值对的 param set 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamSet_WithValidKeyValue_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "set";
    char arg2[] = "test.param.key";
    char arg3[] = "test_value";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param set [键] [值] 命令应该成功执行";
}

/**
 * @test ParamSet_WithSpecialCharacters_ExecutesSuccessfully
 * @brief 测试包含特殊字符的值的 param set 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamSet_WithSpecialCharacters_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "set";
    char arg2[] = "test.special";
    char arg3[] = "value!@#$%^&*()";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param set 设置特殊字符值应该成功执行";
}

// ============================================================================
// param wait 命令测试
// ============================================================================

/**
 * @test ParamWait_WithValidKey_ExecutesSuccessfully
 * @brief 测试等待参数变化的 param wait 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamWait_WithValidKey_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "wait";
    char arg2[] = "test.wait.param";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param wait [键名] 命令应该成功执行";
}

/**
 * @test ParamWait_NoArguments_ExecutesSuccessfully
 * @brief 测试无参数的 param wait 命令
 *
 * 预期：命令执行完成，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamWait_NoArguments_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "wait";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param wait 无参数命令应该执行完成";
}

/**
 * @test ParamWait_WithTimeout_ExecutesSuccessfully
 * @brief 测试带超时的 param wait 命令
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamWait_WithTimeout_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "wait";
    char arg2[] = "test.timeout.param";
    char arg3[] = "12*";  // 值模式
    char arg4[] = "30";   // 超时时间（秒）
    char* args[] = {arg0, arg1, arg2, arg3, arg4, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 5, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param wait [键] [模式] [超时] 命令应该成功执行";
}

// ============================================================================
// param shell 命令测试
// ============================================================================

/**
 * @test ParamShell_EntersShellMode_ExecutesSuccessfully
 * @brief 测试进入参数Shell模式
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamShell_EntersShellMode_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "shell";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param shell 命令应该成功执行";
}

// ============================================================================
// 错误处理测试
// ============================================================================

/**
 * @test ParamCommand_WithInvalidArgument_HandlesGracefully
 * @brief 测试无效参数处理
 *
 * 预期：命令执行，不崩溃
 */
HWTEST_F(BegetctlParamBasicTest, ParamCommand_WithInvalidArgument_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "invalid_command";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert: 应该执行完成，不崩溃
    EXPECT_GE(ret, 0) << "无效命令应该被优雅处理";
}

/**
 * @test ParamCommand_WithManyArguments_ExecutesSuccessfully
 * @brief 测试多参数命令（边界条件）
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamBasicTest, ParamCommand_WithManyArguments_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "set";
    char arg2[] = "a.b.c";
    char arg3[] = "value";
    char arg4[] = "extra";
    char arg5[] = "args";
    char* args[] = {arg0, arg1, arg2, arg3, arg4, arg5, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 6, args);

    // Assert
    EXPECT_EQ(ret, 0) << "多参数命令应该成功执行";
}

} // namespace init_ut
