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

// 测试类
class FsHvbHexStrToBinTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前可以做一些初始化工作
    }

    void TearDown() override {
        // 每个测试完成后可以做一些清理工作
    }
};

// 测试有效的十六进制字符串
TEST_F(FsHvbHexStrToBinTest, TestValidHexString)
{
    char bin[4] = {0};
    char str[] = "a1b2";
    int result = FsHvbHexStrToBin(bin, sizeof(bin), str, sizeof(str) - 1);  // 去掉字符串末尾的空字符
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bin[0], 0xA1);
    EXPECT_EQ(bin[1], 0xB2);
}

// 测试十六进制字符串的长度为奇数
TEST_F(FsHvbHexStrToBinTest, TestOddLengthHexString)
{
    char bin[4] = {0};
    char str[] = "a1b";  // 长度是奇数
    int result = FsHvbHexStrToBin(bin, sizeof(bin), str, sizeof(str) - 1);  // 去掉字符串末尾的空字符
    EXPECT_EQ(result, -1);
}

// 测试目标二进制数组大小小于所需的大小
TEST_F(FsHvbHexStrToBinTest, TestBinSizeTooSmall)
{
    char bin[2] = {0};  // 目标大小是 1 字节
    char str[] = "a1b2";  // 需要 2 字节
    int result = FsHvbHexStrToBin(bin, sizeof(bin), str, sizeof(str) - 1);  // 去掉字符串末尾的空字符
    EXPECT_EQ(result, -1);
}

// 测试十六进制字符串包含无效字符
TEST_F(FsHvbHexStrToBinTest, TestInvalidHexChar)
{
    char bin[4] = {0};
    char str[] = "a1g2";  // 'g' 不是有效的十六进制字符
    int result = FsHvbHexStrToBin(bin, sizeof(bin), str, sizeof(str) - 1);  // 去掉字符串末尾的空字符
    EXPECT_EQ(result, -1);
}

// 测试成功的转换
TEST_F(FsHvbHexStrToBinTest, TestSuccess)
{
    char bin[4] = {0};
    char str[] = "1a2b";
    int result = FsHvbHexStrToBin(bin, sizeof(bin), str, sizeof(str) - 1);  // 去掉字符串末尾的空字符
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bin[0], 0x1A);
    EXPECT_EQ(bin[1], 0x2B);
}

// main 函数运行所有测试
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
