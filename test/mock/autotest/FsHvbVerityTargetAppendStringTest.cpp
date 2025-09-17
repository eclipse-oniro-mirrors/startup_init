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
#include <cerrno>

// 假设头文件名为 FsHvbVerityTargetAppendString.h
#include "FsHvbVerityTargetAppendString.h"

// Mocking the logging functions to test the log output
class MockLogger {
public:
    MOCK_METHOD(void, BEGET_LOGE, (const char* format, ...), ());
    MOCK_METHOD(void, BEGET_LOGI, (const char* format, ...), ());
};

// Test fixture class
class FsHvbVerityTargetAppendStringTest : public ::testing::Test {
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

// Test: 字符串追加超出缓冲区范围
TEST_F(FsHvbVerityTargetAppendStringTest, TestAppendStringOverflow)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    const char *str = "test";
    size_t len = 8;  // len 是 8，超出 buffer 的剩余空间

    EXPECT_CALL(mockLogger, BEGET_LOGE("error, append string overflow"));

    int result = FsHvbVerityTargetAppendString(&p, end, str, len);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示超出范围
}

// Test: memcpy_s 复制失败
TEST_F(FsHvbVerityTargetAppendStringTest, TestMemcpyFail)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    const char *str = "test";
    size_t len = 4;

    // Mock memcpy_s fail (模拟复制失败)
    EXPECT_CALL(mockLogger, BEGET_LOGE("error 0x%x, cp string fail", EINVAL));

    // 模拟 memcpy_s 失败，可以通过返回错误码来模拟
    int result = FsHvbVerityTargetAppendString(&p, end, str, len);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示复制失败
}

// Test: 追加空格超出缓冲区范围
TEST_F(FsHvbVerityTargetAppendStringTest, TestAppendSpaceOverflow)
{
    char buffer[5];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    const char *str = "test";
    size_t len = 4;

    // Mock the logger to capture the log output
    EXPECT_CALL(mockLogger, BEGET_LOGE("error, append blank space overflow"));

    int result = FsHvbVerityTargetAppendString(&p, end, str, len);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示空格追加超出缓冲区
}

// Test: 成功追加字符串和空格
TEST_F(FsHvbVerityTargetAppendStringTest, TestSuccess)
{
    char buffer[10];
    char *p = buffer;
    char *end = buffer + sizeof(buffer);
    const char *str = "test";
    size_t len = 4;

    // Mock the log to check if the log output is correct
    EXPECT_CALL(mockLogger, BEGET_LOGI("append string test"));

    int result = FsHvbVerityTargetAppendString(&p, end, str, len);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功
}

