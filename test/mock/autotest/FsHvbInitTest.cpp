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
#include "FsHvbInit.h"  // 假设头文件名为 FsHvbInit.h

// 模拟外部依赖函数
class MockHvbFunctions {
public:
    MOCK_METHOD(int, GetCurrVerifiedData, (InitHvbType, struct hvb_verified_data***), ());
    MOCK_METHOD(int, hvb_chain_verify, (void*, const char*, void*, struct hvb_verified_data**), ());
    MOCK_METHOD(int, FsHvbCheckCertChainDigest, (struct hvb_cert_data*, uint64_t, InitHvbType), ());
    MOCK_METHOD(void, hvb_chain_verify_data_free, (struct hvb_verified_data*), ());
};

// FsHvbInit 函数的测试类
class FsHvbInitTest : public ::testing::Test {
protected:
    MockHvbFunctions mockHvb;
    
    void SetUp() override {
        // 可以做一些初始化工作
    }

    void TearDown() override {
        // 清理工作
    }
};

// Test: GetCurrVerifiedData 返回错误
TEST_F(FsHvbInitTest, TestGetCurrVerifiedDataFail)
{
    EXPECT_CALL(mockHvb, GetCurrVerifiedData(_, _))
        .WillOnce(Return(HVB_ERR));  // 模拟 GetCurrVerifiedData 错误
    
    int result = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(result, HVB_ERR);  // 验证返回值为错误码
}

// Test: 当前验证数据已存在 (currVd != nullptr)
TEST_F(FsHvbInitTest, TestAlreadyInitialized)
{
    struct hvb_verified_data* currVd = reinterpret_cast<struct hvb_verified_data*>(1);  // 非 nullptr 值模拟已初始化
    EXPECT_CALL(mockHvb, GetCurrVerifiedData(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(currVd), Return(HVB_OK)));  // 模拟 GetCurrVerifiedData 成功并返回非 nullptr
    
    int result = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(result, 0);  // 已初始化，返回成功
}

// Test: hvb_chain_verify 返回错误
TEST_F(FsHvbInitTest, TestHvbChainVerifyFail)
{
    struct hvb_verified_data* currVd = nullptr;
    EXPECT_CALL(mockHvb, GetCurrVerifiedData(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(&currVd), Return(HVB_OK)));  // 返回 nullptr 值
    EXPECT_CALL(mockHvb, hvb_chain_verify(_, _, _, _))
        .WillOnce(Return(HVB_ERR));  // 模拟 hvb_chain_verify 错误

    int result = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(result, HVB_ERR);  // 返回错误码
}

// Test: hvb_chain_verify 返回成功，但 currVd 为空
TEST_F(FsHvbInitTest, TestHvbChainVerifySuccessButNullCurrVd)
{
    struct hvb_verified_data* currVd = nullptr;
    EXPECT_CALL(mockHvb, GetCurrVerifiedData(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(&currVd), Return(HVB_OK)));  // 返回 nullptr 值
    EXPECT_CALL(mockHvb, hvb_chain_verify(_, _, _, _))
        .WillOnce(Return(HVB_OK));  // 模拟成功但 currVd 为 nullptr
    
    int result = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(result, HVB_ERR);  // 返回错误，验证数据为 nullptr
}

// Test: hvb_chain_verify 返回成功，currVd 非空，FsHvbCheckCertChainDigest 返回错误
TEST_F(FsHvbInitTest, TestFsHvbCheckCertChainDigestFail)
{
    struct hvb_verified_data* currVd = reinterpret_cast<struct hvb_verified_data*>(1);  // 非 nullptr 值
    EXPECT_CALL(mockHvb, GetCurrVerifiedData(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(&currVd), Return(HVB_OK)));  // 返回非 nullptr 值
    EXPECT_CALL(mockHvb, hvb_chain_verify(_, _, _, _))
        .WillOnce(Return(HVB_OK));  // 模拟链验证成功
    EXPECT_CALL(mockHvb, FsHvbCheckCertChainDigest(_, _, _))
        .WillOnce(Return(HVB_ERR));  // 模拟证书链哈希校验失败

    int result = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(result, HVB_ERR);  // 返回错误码
}

// Test: hvb_chain_verify 成功，FsHvbCheckCertChainDigest 成功
TEST_F(FsHvbInitTest, TestSuccess)
{
    struct hvb_verified_data* currVd = reinterpret_cast<struct hvb_verified_data*>(1);  // 非 nullptr 值
    EXPECT_CALL(mockHvb, GetCurrVerifiedData(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(&currVd), Return(HVB_OK)));  // 返回非 nullptr 值
    EXPECT_CALL(mockHvb, hvb_chain_verify(_, _, _, _))
        .WillOnce(Return(HVB_OK));  // 模拟链验证成功
    EXPECT_CALL(mockHvb, FsHvbCheckCertChainDigest(_, _, _))
        .WillOnce(Return(0));  // 模拟证书链哈希校验成功

    int result = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(result, 0);  // 成功返回 0
}
