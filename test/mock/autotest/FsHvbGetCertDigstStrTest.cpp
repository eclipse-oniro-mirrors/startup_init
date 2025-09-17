/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <gtest/gtest.h>
#include <cstring>
#include <iostream>

// 假设的错误类型和日志宏
enum HvbErrno {
    HVB_OK = 0,
    HVB_ERROR_INVALID_ARGUMENT = -1
};

enum InitHvbType {
    MAIN_HVB = 0,
    EXT_HVB = 1
};

// 假设的全局变量和常量
const char* HVB_CMDLINE_CERT_DIGEST = "cert_digest";
const char* HVB_CMDLINE_EXT_CERT_DIGEST = "ext_cert_digest";

// 模拟的 FsHvbGetValueFromCmdLine 函数
int FsHvbGetValueFromCmdLine(char *str, size_t size, const char *cmdLine)
{
    if (cmdLine == nullptr || str == nullptr) {
        return -1;
    }
    return 0;
}

// 函数定义
static int FsHvbGetCertDigstStr(char *str, size_t size, InitHvbType hvbType)
{
    if (hvbType == MAIN_HVB) {
        return FsHvbGetValueFromCmdLine(str, size, HVB_CMDLINE_CERT_DIGEST);
    }

    if (hvbType == EXT_HVB) {
        return FsHvbGetValueFromCmdLine(str, size, HVB_CMDLINE_EXT_CERT_DIGEST);
    }

    BEGET_LOGE("error, invalid hvbType");
    return -1;
}

// 测试类
class FsHvbGetCertDigstStrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前可以做一些初始化工作
    }

    void TearDown() override {
        // 每个测试完成后可以做一些清理工作
    }
};

// 测试 MAIN_HVB 情况
TEST_F(FsHvbGetCertDigstStrTest, TestMainHvb)
{
    char str[50] = {0};
    int result = FsHvbGetCertDigstStr(str, sizeof(str), MAIN_HVB);
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(str, "mocked_cert_digest");  // 确保 str 被赋值为 "mocked_cert_digest"
}

// 测试 EXT_HVB 情况
TEST_F(FsHvbGetCertDigstStrTest, TestExtHvb)
{
    char str[50] = {0};
    int result = FsHvbGetCertDigstStr(str, sizeof(str), EXT_HVB);
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(str, "mocked_ext_cert_digest");  // 确保 str 被赋值为 "mocked_ext_cert_digest"
}

// 测试无效 hvbType 情况
TEST_F(FsHvbGetCertDigstStrTest, TestInvalidHvbType)
{
    char str[50] = {0};
    int result = FsHvbGetCertDigstStr(str, sizeof(str), static_cast<InitHvbType>(999));
    EXPECT_EQ(result, -1);
}

// main 函数运行所有测试
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
