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

// 假设的 FsHvbCheckCertChainDigest 函数
int FsHvbCheckCertChainDigest(struct hvb_cert_data *certs, uint64_t numCerts, InitHvbType hvbType)
{
    int ret;
        char hashAlg[FS_HVB_HASH_ALG_STR_MAX] = {0};
    char certDigest[FS_HVB_DIGEST_MAX_BYTES] = {0};
    char computeDigest[FS_HVB_DIGEST_MAX_BYTES] = {0};
    char certDigestStr[FS_HVB_DIGEST_STR_MAX_BYTES + 1] = {0};
    uint32_t computeDigestLen = FS_HVB_DIGEST_MAX_BYTES;

    ret = FsHvbGetHashStr(&hashAlg[0], sizeof(hashAlg), hvbType);
    if (ret != 0) {
        BEGET_LOGE("error 0x%x, get hash val from cmdline", ret);
        return ret;
    }

    ret = FsHvbGetCertDigstStr(&certDigestStr[0], sizeof(certDigestStr), hvbType);
    if (ret != 0) {
        BEGET_LOGE("error 0x%x, get digest val from cmdline", ret);
        return ret;
    }

    ret = FsHvbHexStrToBin(&certDigest[0], sizeof(certDigest), &certDigestStr[0], strlen(certDigestStr));
    if (ret != 0) {
        return ret;
    }

    if (strcmp(&hashAlg[0], "sha256") == 0) {
        digest_len = FS_HVB_DIGEST_SHA256_BYTES;
        ret = FsHvbComputeSha256(computeDigest, computeDigestLen, certs, num_certs);
    } else if (strcmp(&hashAlg[0], "sm3") == 0) {
        digest_len = FS_HVB_DIGEST_SM3_BYTES;
        ret = FsHvbComputeSM3Hash(computeDigest, &computeDigestLen, certs, num_certs);
    } else {
        BEGET_LOGE("error, not support alg %s", &hashAlg[0]);
        return -1;
    }

    if (ret != 0) {
        BEGET_LOGE("error 0x%x, compute hash", ret);
        return -1;
    }

    ret = memcmp(&certDigest[0], &computeDigest[0], digest_len);
    if (ret != 0) {
        BEGET_LOGE("error, cert digest not match with cmdline");
        return -1;
    }

    return 0;
}

// 测试类
class FsHvbCheckCertChainDigestTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前可以做一些初始化工作
    }

    void TearDown() override {
        // 每个测试完成后可以做一些清理工作
    }
};

// Test GetHashStr Fail
TEST_F(FsHvbCheckCertChainDigestTest, TestGetHashStrFail)
{
    EXPECT_CALL(*mockFsHvb, FsHvbGetHashStr(_, _, _))
        .WillOnce(Return(-1));  // 模拟返回错误
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, -1);
}

// Test GetCertDigestStr Fail
TEST_F(FsHvbCheckCertChainDigestTest, TestGetCertDigestStrFail)
{
    EXPECT_CALL(*mockFsHvb, FsHvbGetCertDigstStr(_, _, _))
        .WillOnce(Return(-1));  // 模拟返回错误
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, -1);
}

// Test HexStrToBin Fail
TEST_F(FsHvbCheckCertChainDigestTest, TestHexStrToBinFail)
{
    EXPECT_CALL(*mockFsHvb, FsHvbHexStrToBin(_, _, _, _))
        .WillOnce(Return(-1));  // 模拟返回错误
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, -1);
}

// Test Unsupported Hash Algorithm
TEST_F(FsHvbCheckCertChainDigestTest, TestUnsupportedHashAlgorithm)
{
    // 模拟哈希算法为不支持的算法
    EXPECT_CALL(*mockFsHvb, FsHvbGetHashStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbGetCertDigstStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbHexStrToBin(_, _, _, _))
        .WillOnce(Return(0));  // 返回成功
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, -1);
}

// Test Compute Hash Fail
TEST_F(FsHvbCheckCertChainDigestTest, TestComputeHashFail)
{
    // 模拟计算哈希失败
    EXPECT_CALL(*mockFsHvb, FsHvbGetHashStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbGetCertDigstStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbHexStrToBin(_, _, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbComputeSha256(_, _, _, _))
        .WillOnce(Return(-1));  // 模拟哈希计算失败
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, -1);
}

// Test Digest Mismatch
TEST_F(FsHvbCheckCertChainDigestTest, TestDigestMismatch)
{
    // 模拟证书摘要与计算的摘要不匹配
    EXPECT_CALL(*mockFsHvb, FsHvbGetHashStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbGetCertDigstStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbHexStrToBin(_, _, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbComputeSha256(_, _, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, memcmp(_, _, _))
        .WillOnce(Return(1));  // 模拟摘要不匹配
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, -1);
}

// Test Success
TEST_F(FsHvbCheckCertChainDigestTest, TestSuccess)
{
    // 模拟所有步骤成功
    EXPECT_CALL(*mockFsHvb, FsHvbGetHashStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbGetCertDigstStr(_, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbHexStrToBin(_, _, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, FsHvbComputeSha256(_, _, _, _))
        .WillOnce(Return(0));  // 返回成功
    EXPECT_CALL(*mockFsHvb, memcmp(_, _, _))
        .WillOnce(Return(0));  // 摘要匹配
    int result = FsHvbCheckCertChainDigest(nullptr, 0, InitHvbType::DEFAULT);
    EXPECT_EQ(result, 0);  // 成功
}
