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

// 假设的全局变量
struct HvbVerifiedData {
    int data;
};

struct hvb_verified_data g_vd = {1};  // 假设的数据
struct hvb_verified_data g_extVd = {2};  // 假设的数据


// 函数定义
static enum HvbErrno GetCurrVerifiedData(InitHvbType hvbType, struct hvb_verified_data ***currVd)
{
    if (currVd == nullptr) {
        BEGET_LOGE("error, invalid input");
        return HVB_ERROR_INVALID_ARGUMENT;
    }
    if (hvbType == MAIN_HVB) {
        *currVd = &g_vd;
    } else if (hvbType == EXT_HVB) {
        *currVd = &g_extVd;
    } else {
        BEGET_LOGE("error, invalid hvb type");
        return HVB_ERROR_INVALID_ARGUMENT;
    }

    return HVB_OK;
}

// 测试类
class GetCurrVerifiedDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前可以做一些初始化工作
    }

    void TearDown() override {
        // 每个测试完成后可以做一些清理工作
    }
};

// 测试 currVd 为 nullptr 的情况
TEST_F(GetCurrVerifiedDataTest, TestNullCurrVd)
{
    struct hvb_verified_data **currVd = nullptr;
    EXPECT_EQ(GetCurrVerifiedData(MAIN_HVB, currVd), HVB_ERROR_INVALID_ARGUMENT);
}

// 测试 hvbType 为 MAIN_HVB 时的行为
TEST_F(GetCurrVerifiedDataTest, TestMainHvb)
{
    struct hvb_verified_data *currVd = nullptr;
    struct hvb_verified_data **currVdPtr = &currVd;
    
    EXPECT_EQ(GetCurrVerifiedData(MAIN_HVB, currVdPtr), HVB_OK);
    EXPECT_EQ(currVd->data, 1);  // 确保 currVd 指向 g_vd
}

// 测试 hvbType 为 EXT_HVB 时的行为
TEST_F(GetCurrVerifiedDataTest, TestExtHvb)
{
    struct hvb_verified_data *currVd = nullptr;
    struct hvb_verified_data **currVdPtr = &currVd;
    
    EXPECT_EQ(GetCurrVerifiedData(EXT_HVB, currVdPtr), HVB_OK);
}

// main 函数运行所有测试
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
