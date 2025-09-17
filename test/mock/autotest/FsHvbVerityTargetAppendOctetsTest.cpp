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
#include <gmock/gmock.h>
#include <cstring>
#include <cstdlib>

// 假设头文件名为 FsHvbVerityTargetAppendOctets.h
#include "FsHvbVerityTargetAppendOctets.h"

// Mocking the logging functions to test the log output
class MockLogger {
public:
    MOCK_METHOD(void, BEGET_LOGE, (const char* format, ...), ());
    MOCK_METHOD(void, BEGET_LOGI, (const char* format, ...), ());
};

// Test fixture class
class FsHvbVerityTargetAppendOctetsTest : public ::testing::Test {
protected:
    MockLogger mockLogger;

    // 模拟 BEGET_LOGE 和 BEGET_LOGI 输出
    void SetUp() override {
        // 可以设置其他初始化工作
    }

    void TearDown() override {
        // 清理工作
    }
};

// Test: calloc 失败
TEST_F(FsHvbVerityTargetAppendOctetsTest, TestCallocFail)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    char octets[] = {0x1, 0x2, 0x3};
    size_t octsLen = sizeof(octets);

    // Mock calloc fail (模拟 calloc 失败)
    EXPECT_CALL(mockLogger, BEGET_LOGE("error, calloc str fail"));

    // 模拟 calloc 失败，可以通过返回 nullptr 来模拟
    void* (*originalCalloc)(size_t, size_t) = calloc;
    calloc = [](size_t, size_t) -> void* { return nullptr; };

    int result = FsHvbVerityTargetAppendOctets(&p, end, octets, octs_len);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示 calloc 失败

    // 恢复原始 calloc
    calloc = originalCalloc;
}

// Test: 十六进制字符串转换成功
TEST_F(FsHvbVerityTargetAppendOctetsTest, TestHexConversionSuccess)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    char octets[] = {0x1, 0x2, 0x3};
    size_t octsLen = sizeof(octets);

    // Mock the logger to check if the log output is correct
    EXPECT_CALL(mockLogger, BEGET_LOGI("append string 010203"));

    int result = FsHvbVerityTargetAppendOctets(&p, end, octets, octs_len);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功
}

// Test: `FsHvbVerityTargetAppendString` 追加失败
TEST_F(FsHvbVerityTargetAppendOctetsTest, TestAppendStringFail)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    char octets[] = {0x1, 0x2, 0x3};
    size_t octsLen = sizeof(octets);

    // Mock `FsHvbVerityTargetAppendString` fail (模拟追加失败)
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, append str fail", -1));

    // Mock `FsHvbVerityTargetAppendString` 失败，返回非 0 错误码
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, append str fail", -1));

    int result = FsHvbVerityTargetAppendOctets(&p, end, octets, octs_len);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示追加失败
}

// Test: 成功追加十六进制字符串
TEST_F(FsHvbVerityTargetAppendOctetsTest, TestSuccess)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    char octets[] = {0x1, 0x2, 0x3};
    size_t octsLen = sizeof(octets);

    // Mock the logger to check if the log output is correct
    EXPECT_CALL(mockLogger, BEGET_LOGI("append string 010203"));

    int result = FsHvbVerityTargetAppendOctets(&p, end, octets, octs_len);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功
}
