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
 * @file begetctl_param_advanced_test.cpp
 * @brief begetctl param 高级命令单元测试
 *
 * 测试覆盖：
 * - param dump [verbose|index] - 转储参数信息
 * - param save - 保存参数
 * - param cd [path] - 切换参数目录
 * - param pwd - 打印当前参数目录
 * - param cat [name] - 查看参数值
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
 * @brief param 高级命令测试Fixture
 *
 * 测试 param 命令的高级功能：
 * 1. param dump [verbose|index] - 转储参数信息
 * 2. param save - 保存参数
 * 3. param cd [path] - 切换参数目录
 * 4. param pwd - 打印当前参数目录
 * 5. param cat [name] - 查看参数值
 */
class BegetctlParamAdvancedTest : public testing::Test {
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
// param dump 命令测试
// ============================================================================

/**
 * @test ParamDump_Verbose_ExecutesSuccessfully
 * @brief 测试转储详细参数信息
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamDump_Verbose_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "dump";
    char arg2[] = "verbose";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param dump verbose 命令应该成功执行";
}

/**
 * @test ParamDump_ByIndex_ExecutesSuccessfully
 * @brief 测试按索引转储参数信息
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamDump_ByIndex_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "dump";
    char arg2[] = "1";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param dump [索引] 命令应该成功执行";
}

// ============================================================================
// param save 命令测试
// ============================================================================

/**
 * @test ParamSave_ExecutesSuccessfully
 * @brief 测试保存参数
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamSave_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "save";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param save 命令应该成功执行";
}

// ============================================================================
// param cd 命令测试
// ============================================================================

/**
 * @test ParamCd_WithValidPath_ExecutesSuccessfully
 * @brief 测试切换参数目录
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamCd_WithValidPath_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    BShellEnvSetParam(GetShellHandle(), PARAM_REVERESD_NAME_CURR_PARAMETER, "", PARAM_STRING, (void*)"");
    char arg0[] = "param";
    char arg1[] = "cd";
    char arg2[] = "ohos.";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param cd [路径] 命令应该成功执行";
}

/**
 * @test ParamCd_WithEmptyPath_HandlesGracefully
 * @brief 测试切换到空路径
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamCd_WithEmptyPath_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "cd";
    char arg2[] = "";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param cd 空路径应该被优雅处理";
}

// ============================================================================
// param pwd 命令测试
// ============================================================================

/**
 * @test ParamPwd_ExecutesSuccessfully
 * @brief 测试打印当前参数目录
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamPwd_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "pwd";
    char* args[] = {arg0, arg1, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 2, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param pwd 命令应该成功执行";
}

// ============================================================================
// param cat 命令测试
// ============================================================================

/**
 * @test ParamCat_WithValidParam_ExecutesSuccessfully
 * @brief 测试查看参数值
 *
 * 预期：命令成功执行，返回0
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamCat_WithValidParam_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange
    SystemWriteParam("test.param.cat", "test_value");
    char arg0[] = "param";
    char arg1[] = "cat";
    char arg2[] = "test.param.cat";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param cat [参数名] 命令应该成功执行";
}

/**
 * @test ParamCat_WithInvalidParam_HandlesGracefully
 * @brief 测试查看不存在的参数
 *
 * 预期：命令执行完成，不崩溃
 */
HWTEST_F(BegetctlParamAdvancedTest, ParamCat_WithInvalidParam_HandlesGracefully, TestSize.Level1)
{
    // Arrange
    char arg0[] = "param";
    char arg1[] = "cat";
    char arg2[] = "non.exist.param";
    char* args[] = {arg0, arg1, arg2, nullptr};

    // Act
    int ret = BShellEnvDirectExecute(GetShellHandle(), 3, args);

    // Assert
    EXPECT_EQ(ret, 0) << "param cat 不存在的参数应该被优雅处理";
}

} // namespace init_ut
