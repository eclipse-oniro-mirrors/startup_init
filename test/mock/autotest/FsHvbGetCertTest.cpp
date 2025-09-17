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
#include "FsHvbGetCert.h"  // 假设头文件名为 FsHvbGetCert.h

// 模拟外部依赖函数
class MockFsHvbFindVabPartition {
public:
    MOCK_METHOD(int, FsHvbFindVabPartition, (const char* devName, HvbDeviceParam* devPara), ());
};

class MockHvbCertParser {
public:
    MOCK_METHOD(int, hvb_cert_parser, (struct hvb_cert* cert, const struct hvb_cert_data* data), ());
};

class MockGetBootSlots {
public:
    MOCK_METHOD(int, GetBootSlots, (), ());
};

class FsHvbGetCertTest : public ::testing::Test {
protected:
    MockFsHvbFindVabPartition mockFsHvbFindVabPartition;
    MockHvbCertParser mockHvbCertParser;
    MockGetBootSlots mockGetBootSlots;

    void SetUp() override {
        // 可以做一些初始化工作
    }

    void TearDown() override {
        // 清理工作
    }
};

// Test: devName 不存在，返回错误
TEST_F(FsHvbGetCertTest, TestDevNameNotFound)
{
    struct hvb_cert cert;
    struct hvb_verified_data vd;
    // 假设 devName 无法通过 FsHvbFindVabPartition 获取
    EXPECT_CALL(mockFsHvbFindVabPartition, FsHvbFindVabPartition(_, _))
        .WillOnce(Return(-1));  // 模拟找不到 partition

    int result = FsHvbGetCert(&cert, "testPartition", &vd);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示错误
}

// Test: devName 后缀无效，返回错误
TEST_F(FsHvbGetCertTest, TestInvalidDevNameSuffix)
{
    struct hvb_cert cert;
    struct hvb_verified_data vd;
    HvbDeviceParam devPara = {};
    
    // 假设 devName 需要去除后缀 "_a" 或 "_b"
    EXPECT_CALL(mockFsHvbFindVabPartition, FsHvbFindVabPartition(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(devPara), Return(0)));  // 模拟获取 devPara

    EXPECT_CALL(mockGetBootSlots, GetBootSlots()).WillOnce(Return(2));  // 模拟 bootSlots 为 2
    
    int result = FsHvbGetCert(&cert, "testPartition_a", &vd);  // 使用带有后缀的 devName
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功去除后缀并找到匹配分区
}

// Test: devName 后缀无效，未找到匹配分区
TEST_F(FsHvbGetCertTest, TestDevNameSuffixNoMatch)
{
    struct hvb_cert cert;
    struct hvb_verified_data vd;
    struct hvb_cert_data certData[] = { {"testPartition", "data1"}, {"testPartition_b", "data2"} };
    vd.certs = certData;
        HvbDeviceParam devPara = {};
    
    // 假设 devName 需要去除后缀 "_a" 或 "_b"
    EXPECT_CALL(mockFsHvbFindVabPartition, FsHvbFindVabPartition(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(devPara), Return(0)));  // 模拟获取 devPara

    EXPECT_CALL(mockGetBootSlots, GetBootSlots()).WillOnce(Return(2));  // 模拟 bootSlots 为 2

    // devName 后缀去除后，没有匹配项
    int result = FsHvbGetCert(&cert, "testPartition_a", &vd);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示未找到分区
}

// Test: 匹配分区成功
TEST_F(FsHvbGetCertTest, TestCertFound)
{
    struct hvb_cert cert;
    struct hvb_verified_data vd;
    struct hvb_cert_data certData[] = { {"testPartition", "data1"}, {"testPartition_b", "data2"} };
    vd.certs = certData;
        HvbDeviceParam devPara = {};
    
    // 模拟成功找到分区
    EXPECT_CALL(mockFsHvbFindVabPartition, FsHvbFindVabPartition(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(devPara), Return(0)));  // 模拟获取 devPara

    EXPECT_CALL(mockGetBootSlots, GetBootSlots()).WillOnce(Return(1));  // 模拟 bootSlots 为 1

    // 匹配分区成功
    EXPECT_CALL(mockHvbCertParser, hvb_cert_parser(_, _))
        .WillOnce(Return(0));  // 模拟解析成功

    int result = FsHvbGetCert(&cert, "testPartition", &vd);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功找到并解析证书
}

// Test: 找到分区但解析失败
TEST_F(FsHvbGetCertTest, TestCertParseFail)
{
    struct hvb_cert cert;
    struct hvb_verified_data vd;
    struct hvb_cert_data certData[] = { {"testPartition", "data1"}, {"testPartition_b", "data2"} };
    vd.certs = certData;
        HvbDeviceParam devPara = {};
    
    // 模拟成功找到分区
    EXPECT_CALL(mockFsHvbFindVabPartition, FsHvbFindVabPartition(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(devPara), Return(0)));  // 模拟获取 devPara

    EXPECT_CALL(mockGetBootSlots, GetBootSlots()).WillOnce(Return(1));  // 模拟 bootSlots 为 1

    // 匹配分区成功，但解析失败
    EXPECT_CALL(mockHvbCertParser, hvb_cert_parser(_, _))
        .WillOnce(Return(-1));  // 模拟解析失败

    int result = FsHvbGetCert(&cert, "testPartition", &vd);
    EXPECT_EQ(result, -1);  // 验证返回 -1，表示解析失败
}
