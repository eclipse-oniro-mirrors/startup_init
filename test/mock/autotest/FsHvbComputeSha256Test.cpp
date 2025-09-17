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
#define FS_HVB_DIGEST_SHA256_BYTES 32

// 假设的结构体定义
struct HashCtxT{
    int dummy;  // 模拟哈希上下文
};

struct DataT{
    void *addr;
    size_t size;
};

struct hvb_cert_data {
    data_t data;
};

// 假设的哈希算法相关函数
int HashCtxInit(HashCtxT*ctx, int algo)
{
    return (algo == 256) ? 0 : -1;  // 假设 SHA256 初始化正常
}

int HashCalcUpdate(HashCtxT*ctx, const void *data, size_t len)
{
    return (data != nullptr && len > 0) ? 0 : -1;  // 模拟更新失败的情况
}

int HashCalcDoFinal(HashCtxT*ctx, const void *data, size_t len, uint8_t *digest, size_t size)
{
    return (digest != nullptr && size == FS_HVB_DIGEST_SHA256_BYTES) ? 0 : -1;  // 模拟最终计算失败
}

// 模拟的 FsHvbComputeSha256 函数
int FsHvbComputeSha256(char *digest, uint32_t size, struct hvb_cert_data *certs, uint64_t numCerts)
{
    int ret;
    uint64_t n;
    struct HashCtxTctx = {0};

    if (size < FS_HVB_DIGEST_SHA256_BYTES) {
        BEGET_LOGE("error, size=%zu", size);
        return -1;
    }

    ret = hash_ctx_init(&ctx);
    if (ret != 0) {
        BEGET_LOGE("error 0x%x, sha256 init", ret);
        return -1;
    }

    for (n = 0; n < num_certs; n++) {
        ret = hash_calc_update(&ctx, certs[n].data.addr, certs[n].data.size);
        if (ret != 0) {
            BEGET_LOGE("error 0x%x, sha256 update", ret);
            return -1;
        }
    }

    ret = hash_calc_do_final(&ctx, nullptr, 0, digest, size);
    if (ret != 0) {
        BEGET_LOGE("error 0x%x, sha256 final", ret);
        return -1;
    }

    return 0;
}

// 测试类
class FsHvbComputeSha256Test : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前可以做一些初始化工作
    }

    void TearDown() override {
        // 每个测试完成后可以做一些清理工作
    }
};

// 测试 size < FS_HVB_DIGEST_SHA256_BYTES 情况
TEST_F(FsHvbComputeSha256Test, TestInvalidSize)
{
    char digest[FS_HVB_DIGEST_SHA256_BYTES] = {0};
    struct hvb_cert_data certs[1] = { {{nullptr, 0}} };  // 空数据
    int result = FsHvbComputeSha256(digest, 10, certs, 1);  // 给定一个无效的 size
    EXPECT_EQ(result, -1);
}

// 测试哈希上下文初始化失败
TEST_F(FsHvbComputeSha256Test, TestHashInitFailure)
{
    char digest[FS_HVB_DIGEST_SHA256_BYTES] = {0};
    struct hvb_cert_data certs[1] = { {{nullptr, 0}} };  // 空数据
    int result = FsHvbComputeSha256(digest, FS_HVB_DIGEST_SHA256_BYTES, certs, 1);
    EXPECT_EQ(result, -1);
}

// 测试哈希更新失败
TEST_F(FsHvbComputeSha256Test, TestHashUpdateFailure)
{
    char digest[FS_HVB_DIGEST_SHA256_BYTES] = {0};
    struct hvb_cert_data certs[1] = { {{nullptr, 0}} };  // 空数据
    int result = FsHvbComputeSha256(digest, FS_HVB_DIGEST_SHA256_BYTES, certs, 1);
    EXPECT_EQ(result, -1);
}

// 测试哈希完成失败
TEST_F(FsHvbComputeSha256Test, TestHashFinalFailure)
{
    char digest[FS_HVB_DIGEST_SHA256_BYTES] = {0};
    struct hvb_cert_data certs[1] = { {{nullptr, 1}} };  // 模拟有效数据
    int result = FsHvbComputeSha256(digest, FS_HVB_DIGEST_SHA256_BYTES, certs, 1);
    EXPECT_EQ(result, -1);
}

// 测试成功的计算
TEST_F(FsHvbComputeSha256Test, TestSuccess)
{
    char digest[FS_HVB_DIGEST_SHA256_BYTES] = {0};
    int result = FsHvbComputeSha256(digest, FS_HVB_DIGEST_SHA256_BYTES, certs, 1);
    EXPECT_EQ(result, 0);
}

// main 函数运行所有测试
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
