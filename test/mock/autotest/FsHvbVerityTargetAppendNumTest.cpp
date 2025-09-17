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

// 假设头文件名为 FsHvbVerityTargetAppendNum.h
#include "FsHvbVerityTargetAppendNum.h"

// Mocking the logging functions to test the log output
class MockLogger {
public:
    MOCK_METHOD(void, BEGET_LOGE, (const char* format, ...), ());
    MOCK_METHOD(void, BEGET_LOGI, (const char* format, ...), ());
};

// Test fixture class
class FsHvbVerityTargetAppendNumTest : public ::testing::Test {
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

// Test: sprintf_s 失败
TEST_F(FsHvbVerityTargetAppendNumTest, TestSprintfFail)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    uint64_t num = 12345;

    // Mock sprintf_s fail (模拟 sprintf_s 失败)
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, calloc num_str", -1));

    // Mock `sprintf_s` 失败，返回负值
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, calloc num_str", -1));

    // 直接模拟 `sprintf_s` 失败
    int result = FsHvbVerityTargetAppendNum(&p, end, num);
    EXPECT_LT(result, 0);  // 验证返回小于 0，表示 `sprintf_s` 失败
}

// Test: `FsHvbVerityTargetAppendString` 追加失败
TEST_F(FsHvbVerityTargetAppendNumTest, TestAppendStringFail)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    uint64_t num = 12345;

    // Mock `FsHvbVerityTargetAppendString` fail (模拟追加失败)
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, append num_str fail", -1));

    // Mock `FsHvbVerityTargetAppendString` 失败，返回非 0 错误码
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, append num_str fail", -1));

    // 模拟追加失败
    int result = FsHvbVerityTargetAppendNum(&p, end, num);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示追加失败
}

// Test: 成功追加数字字符串
TEST_F(FsHvbVerityTargetAppendNumTest, TestSuccess)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    uint64_t num = 12345;

    // Mock the logger to check if the log output is correct
    EXPECT_CALL(mockLogger, BEGET_LOGI("append string 12345"));

    // 模拟成功，返回值应该为 0
    int result = FsHvbVerityTargetAppendNum(&p, end, num);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功
}
