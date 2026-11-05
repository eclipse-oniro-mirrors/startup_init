/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>
#include <gtest/gtest.h>
#include "fs_hvb.h"
#include "fs_hvb_image_patch.h"
#include "init_utils.h"
#include "libfs_hvb_mock.h"
#include "libfs_hvb_test_adapter.h"
#include "securec.h"

using namespace testing::ext;

namespace init_ut {
namespace {
constexpr const char *TEST_ROOT = STARTUP_INIT_UT_PATH "/libfs_hvb/";
constexpr uint64_t TEST_IMAGE_SIZE = 8192;
constexpr uint64_t TEST_FEC_ROOTS = 2;
constexpr std::array<const char *, 11> MOCK_ENV = {"VERIFY_VALUE", "HASH_VALUE", "FSDM_VALUE", "CERT_VALUE",
    "BOOTSLOTS_VALUE", "CURRENTSLOT_VALUE", "HVB_SNAPSHOT_FORWARD", "HVB_SNAPSHOT_REVERSE", "HVB_QUICKFIX",
    "HVB_QUICKFIX_CERT", "FSDM_NULL_PATH"};

struct OwnedTarget {
    DmVerityTarget value = {};
    ~OwnedTarget() { FsHvbDestoryVerityTarget(&value); }
};

struct DmFailureCase {
    const char *mode;
    bool nullPath;
};

constexpr DmFailureCase DM_FAILURE_CASES[] = {
    {"CreateFail", false},
    {"InitFail", false},
    {"InitFail", true},
};

void SetDmFailure(const DmFailureCase &failure)
{
    setenv("FSDM_VALUE", failure.mode, 1);
    if (failure.nullPath) {
        setenv("FSDM_NULL_PATH", "1", 1);
    } else {
        unsetenv("FSDM_NULL_PATH");
    }
}

struct OwnedItem {
    FstabItem value = {};
    explicit OwnedItem(const char *name) { value.deviceName = strdup(name); }
    ~OwnedItem() { free(value.deviceName); }
};

struct hvb_cert MakeCert(bool fec = false)
{
    static uint8_t digest[] = {0xab, 0xcd};
    static uint8_t salt[] = {0xef};
    struct hvb_cert cert = {};
    cert.image_len = TEST_IMAGE_SIZE;
    cert.data_block_size = SZ_4KB;
    cert.hash_block_size = SZ_4KB;
    cert.hashtree_offset = TEST_IMAGE_SIZE;
    cert.hash_algo = 0;
    cert.hash_payload.digest = digest;
    cert.digest_size = sizeof(digest);
    cert.hash_payload.salt = salt;
    cert.salt_size = sizeof(salt);
    cert.fec_size = fec ? SZ_4KB : 0;
    cert.fec_num_roots = TEST_FEC_ROOTS;
    cert.fec_offset = TEST_IMAGE_SIZE;
    return cert;
}

void RemoveTestFile(const char *path)
{
    if (std::remove(path) != 0) {
        const int error = errno;
        EXPECT_EQ(error, ENOENT) << "Could not remove test file " << path;
    }
}

template<typename Function>
int InjectFailure(HvbMockFailure function, unsigned int call, Function operation)
{
    HvbMockFailAt(function, call);
    int result = operation();
    unsigned int hits = HvbMockFailureHits();
    HvbMockFailAt(HVB_FAIL_NONE, 0);
    EXPECT_EQ(hits, 1u) << "failure was not reached, function=" << function << ", call=" << call;
    return result;
}
} // namespace

class FsHvbBranchTest : public testing::Test {
protected:
    std::vector<std::string> files_;

    void SetUp() override
    {
        HvbMockReset();
        FsHvbFinal(MAIN_HVB);
        FsHvbFinal(EXT_HVB);
        for (const char *key : MOCK_ENV) {
            unsetenv(key);
        }
        RemoveTestFile(BOOT_CMD_LINE);
    }

    void TearDown() override
    {
        HvbMockFailAt(HVB_FAIL_NONE, 0);
        FsHvbFinal(MAIN_HVB);
        FsHvbFinal(EXT_HVB);
        for (const auto &path : files_) {
            RemoveTestFile(path.c_str());
        }
        RemoveTestFile(BOOT_CMD_LINE);
        for (const char *key : MOCK_ENV) {
            unsetenv(key);
        }
        HvbMockReset();
    }

    bool WriteFile(const std::string &path, const void *data, size_t size)
    {
        CheckAndCreateDir(path.c_str());
        FILE *file = fopen(path.c_str(), "wb");
        if (file == nullptr) {
            return false;
        }
        files_.push_back(path);
        size_t written = fwrite(data, 1, size, file);
        int result = fclose(file);
        return written == size && result == 0;
    }

    bool Cmdline(const std::string &line)
    {
        return WriteFile(BOOT_CMD_LINE, line.data(), line.size());
    }

    bool Image(const std::string &name, const std::vector<uint8_t> &data)
    {
        return WriteFile(std::string(TEST_ROOT) + name, data.data(), data.size());
    }

    int Prepare(InitHvbType type = MAIN_HVB)
    {
        setenv("VERIFY_VALUE", "Succeed", 1);
        setenv("HASH_VALUE", "AllSucceed", 1);
        setenv("FSDM_VALUE", "AllSucceed", 1);
        if (!Cmdline("ohos.boot.hvb.hash_algo=sha256 ohos.boot.hvb.digest=01 ohos.boot.hvb.ext_rvt=01 ")) {
            return -1;
        }
        return FsHvbInit(type);
    }

    int Read(const char *partition, int64_t offset, size_t size, void *buffer, uint64_t *count)
    {
        struct hvb_ops *ops = FsHvbGetOps();
        return ops->read_partition(ops, partition, offset, size, buffer, count);
    }
};

HWTEST_F(FsHvbBranchTest, VerityTableContentsAndAlgorithms, TestSize.Level0)
{
    const unsigned int algorithms[] = {0, 2, 3};
    const char *names[] = {"sha256", "sha512", "sm3"};
    for (size_t i = 0; i < 3; ++i) {
        for (bool fec : {false, true}) {
            SCOPED_TRACE(names[i]);
            SCOPED_TRACE(fec);
            OwnedTarget target;
            auto cert = MakeCert(fec);
            cert.hash_algo = algorithms[i];
            ASSERT_EQ(FsHvbConstructVerityTarget(&target.value, "boot", &cert), 0);
            std::string expected = "1 /dev/block/by-name/boot /dev/block/by-name/boot 4096 4096 2 2 ";
            expected += names[i];
            expected += " abcd ef";
            if (fec) {
                expected += " use_fec_from_device /dev/block/by-name/boot fec_roots 2 fec_blocks 2 fec_start 2";
            }
            EXPECT_STREQ(target.value.paras, expected.c_str());
            EXPECT_EQ(target.value.paras_len, expected.size());
            EXPECT_EQ(target.value.start, 0u);
            EXPECT_EQ(target.value.length, 16u);
        }
    }
}

HWTEST_F(FsHvbBranchTest, VerityTableBufferBoundaryMatrix, TestSize.Level0)
{
    std::array<uint8_t, 320> bytes = {};
    bytes.fill(0xab);
    // Exercise every append boundary with both parities and short/long numbers.
    // Success must produce a terminated table; failure must remain destructible.
    for (const char *device : {"boot", "/dev/test", "/dev/long-device-name-for-fec-boundaries"}) {
        for (bool largeNumbers : {false, true}) {
            for (size_t size = 0; size <= 270; ++size) {
                SCOPED_TRACE(device);
                SCOPED_TRACE(largeNumbers);
                SCOPED_TRACE(size);
                OwnedTarget target;
                auto cert = MakeCert(true);
                cert.hash_payload.digest = bytes.data();
                cert.digest_size = size;
                if (largeNumbers) {
                    cert.fec_num_roots = std::numeric_limits<uint64_t>::max();
                    cert.fec_offset = std::numeric_limits<uint64_t>::max();
                    cert.data_block_size = 1;
                    cert.hash_block_size = 1;
                }
                std::string path = strchr(device, '/') == nullptr
                    ? std::string("/dev/block/by-name/") + device : device;
                std::string table = "1 " + path + " " + path;
                table += largeNumbers ? " 1 1 8192 8192 sha256 " : " 4096 4096 2 2 sha256 ";
                table += std::string(size * 2, 'a') + " ef use_fec_from_device " + path + " fec_roots ";
                table += largeNumbers
                    ? "18446744073709551615 fec_blocks 18446744073709551615 "
                      "fec_start 18446744073709551615"
                    : "2 fec_blocks 2 fec_start 2";
                int result = FsHvbConstructVerityTarget(&target.value, device, &cert);
                // 512 bytes must also hold the appended separator and terminator.
                EXPECT_EQ(result, table.size() <= 510 ? 0 : -1);
                if (result == 0) {
                    ASSERT_NE(target.value.paras, nullptr);
                    EXPECT_EQ(strlen(target.value.paras), target.value.paras_len);
                    EXPECT_LT(target.value.paras_len, 512u);
                    EXPECT_NE(target.value.paras[target.value.paras_len - 1], ' ');
                }
            }
        }
    }
}

HWTEST_F(FsHvbBranchTest, VeritySaltOverflowAndEmptyPayloads, TestSize.Level0)
{
    std::array<uint8_t, 300> bytes = {};
    for (size_t size : {0u, 1u, 200u, 300u}) {
        OwnedTarget target;
        auto cert = MakeCert();
        cert.hash_payload.salt = bytes.data();
        cert.salt_size = size;
        EXPECT_EQ(FsHvbConstructVerityTarget(&target.value, "boot", &cert), size == 300 ? -1 : 0);
    }
}

HWTEST_F(FsHvbBranchTest, VerityDevicePathLengthBoundary, TestSize.Level0)
{
    for (size_t size : {126u, 127u, 128u, 200u}) {
        OwnedTarget target;
        auto cert = MakeCert();
        std::string device = "/" + std::string(size - 1, 'a');
        EXPECT_EQ(FsHvbConstructVerityTarget(&target.value, device.c_str(), &cert), size <= 127 ? 0 : -1);
    }
}

HWTEST_F(FsHvbBranchTest, VerityAllocationFailures, TestSize.Level0)
{
    for (unsigned int call = 1; call <= 3; ++call) {
        OwnedTarget target;
        auto cert = MakeCert();
        int result = InjectFailure(HVB_FAIL_CALLOC, call, [&] {
            return FsHvbConstructVerityTarget(&target.value, "/dev/test", &cert);
        });
        EXPECT_EQ(result, -1);
    }
}

HWTEST_F(FsHvbBranchTest, VerityAllNumericFormattingFailures, TestSize.Level0)
{
    for (unsigned int call = 1; call <= 8; ++call) {
        OwnedTarget target;
        auto cert = MakeCert(true);
        EXPECT_EQ(InjectFailure(HVB_FAIL_SPRINTF, call, [&] {
            return FsHvbConstructVerityTarget(&target.value, "/dev/test", &cert);
        }), -1);
    }
}

HWTEST_F(FsHvbBranchTest, VerityAllAppendCopyFailures, TestSize.Level0)
{
    for (unsigned int call = 1; call <= 18; ++call) {
        OwnedTarget target;
        auto cert = MakeCert(true);
        EXPECT_EQ(InjectFailure(HVB_FAIL_MEMCPY, call, [&] {
            return FsHvbConstructVerityTarget(&target.value, "/dev/test", &cert);
        }), -1);
    }
}

HWTEST_F(FsHvbBranchTest, VerifyExtNullArgumentsAndUninitializedMain, TestSize.Level0)
{
    char *path = nullptr;
    EXPECT_EQ(VerifyExtHvbImage(nullptr, "boot", &path), -1);
    EXPECT_EQ(VerifyExtHvbImage("/dev/test", nullptr, &path), -1);
    EXPECT_EQ(VerifyExtHvbImage("/dev/test", "boot", nullptr), -1);
    OwnedItem item("/dev/block/by-name/boot");
    EXPECT_EQ(FsHvbSetupHashtree(&item.value), -1);
    EXPECT_EQ(path, nullptr);
}

HWTEST_F(FsHvbBranchTest, HashAlgorithmsFailureAndRetryMatrix, TestSize.Level0)
{
    for (const char *algorithm : {"sha256", "sm3"}) {
        for (const char *failure : {"InitFail", "UpdateFail", "FinalFail", "AllSucceed"}) {
            SCOPED_TRACE(algorithm);
            SCOPED_TRACE(failure);
            FsHvbFinal(MAIN_HVB);
            setenv("VERIFY_VALUE", "Succeed", 1);
            setenv("HASH_VALUE", failure, 1);
            ASSERT_TRUE(Cmdline(std::string("ohos.boot.hvb.hash_algo=") + algorithm + " ohos.boot.hvb.digest=01 "));
            EXPECT_EQ(FsHvbInit(MAIN_HVB), strcmp(failure, "AllSucceed") == 0 ? 0 : -1);
            setenv("HASH_VALUE", "AllSucceed", 1);
            EXPECT_EQ(FsHvbInit(MAIN_HVB), 0);
        }
    }
}

HWTEST_F(FsHvbBranchTest, DigestValuesAndUnsupportedAlgorithm, TestSize.Level0)
{
    for (const char *digest : {"0", "00", "aa", "a0", "0/", "01"}) {
        SCOPED_TRACE(digest);
        FsHvbFinal(MAIN_HVB);
        setenv("VERIFY_VALUE", "Succeed", 1);
        setenv("HASH_VALUE", "AllSucceed", 1);
        ASSERT_TRUE(Cmdline(std::string("ohos.boot.hvb.hash_algo=sha256 ohos.boot.hvb.digest=") + digest + " "));
        EXPECT_EQ(FsHvbInit(MAIN_HVB), strcmp(digest, "01") == 0 ? 0 : -1);
    }
    FsHvbFinal(MAIN_HVB);
    ASSERT_TRUE(Cmdline("ohos.boot.hvb.hash_algo=sha512 ohos.boot.hvb.digest=01 "));
    EXPECT_EQ(FsHvbInit(MAIN_HVB), -1);
}

HWTEST_F(FsHvbBranchTest, ExtInitializationAndFinalizationAreIndependent, TestSize.Level0)
{
    ASSERT_EQ(Prepare(MAIN_HVB), 0);
    ASSERT_EQ(FsHvbInit(EXT_HVB), 0);
    EXPECT_EQ(FsHvbInit(EXT_HVB), 0);
    struct hvb_verified_data **mainData = nullptr;
    struct hvb_verified_data **extData = nullptr;
    ASSERT_EQ(HvbTestGetCurrVerifiedData(MAIN_HVB, &mainData), HVB_OK);
    ASSERT_EQ(HvbTestGetCurrVerifiedData(EXT_HVB, &extData), HVB_OK);
    ASSERT_NE(mainData, nullptr);
    ASSERT_NE(extData, nullptr);
    ASSERT_NE(mainData, extData);
    ASSERT_NE(*mainData, nullptr);
    ASSERT_NE(*extData, nullptr);
    const auto *savedMainData = *mainData;
    ASSERT_EQ(FsHvbFinal(EXT_HVB), 0);
    EXPECT_EQ(*mainData, savedMainData);
    EXPECT_EQ(*extData, nullptr);
    EXPECT_EQ(FsHvbInit(MAIN_HVB), 0);
    EXPECT_EQ(FsHvbFinal(MAIN_HVB), 0);
    EXPECT_EQ(*mainData, nullptr);
    EXPECT_EQ(FsHvbFinal(MAIN_HVB), 0);
}

HWTEST_F(FsHvbBranchTest, MainCertificateMatchingMatrix, TestSize.Level0)
{
    ASSERT_EQ(Prepare(), 0);
    for (const char *name : {"boot", "boot_a", "boot_b", "boot_c", "root", "missing", "ab"}) {
        SCOPED_TRACE(name);
        setenv("BOOTSLOTS_VALUE", "2", 1);
        OwnedItem item(name);
        int result = FsHvbSetupHashtree(&item.value);
        bool match = strcmp(name, "boot") == 0 || strcmp(name, "boot_a") == 0 || strcmp(name, "boot_b") == 0;
        EXPECT_EQ(result, match ? 0 : -1);
        if (match) {
            EXPECT_STREQ(item.value.deviceName, STARTUP_INIT_UT_PATH "/libfs_hvb/dm-test");
        }
    }
}

HWTEST_F(FsHvbBranchTest, CertificateAndDeviceManagerFailures, TestSize.Level0)
{
    ASSERT_EQ(Prepare(), 0);
    for (const char *mode : {"Fail", "ZeroDataBlock", "ZeroHashBlock", "InvalidHash"}) {
        SCOPED_TRACE(mode);
        setenv("CERT_VALUE", mode, 1);
        OwnedItem item("boot");
        EXPECT_EQ(FsHvbSetupHashtree(&item.value), -1);
        EXPECT_STREQ(item.value.deviceName, "boot");
    }
    unsetenv("CERT_VALUE");
    for (const auto &failure : DM_FAILURE_CASES) {
        SCOPED_TRACE(failure.mode);
        SCOPED_TRACE(failure.nullPath);
        SetDmFailure(failure);
        OwnedItem item("boot");
        EXPECT_EQ(FsHvbSetupHashtree(&item.value), -1);
        EXPECT_STREQ(item.value.deviceName, "boot");
    }
}

HWTEST_F(FsHvbBranchTest, MainBasenameAndSnapshotCopyFailures, TestSize.Level0)
{
    ASSERT_EQ(Prepare(), 0);
    OwnedItem item("boot");
    EXPECT_EQ(InjectFailure(HVB_FAIL_BASENAME, 1, [&] { return FsHvbSetupHashtree(&item.value); }), -1);
    setenv("HVB_SNAPSHOT_REVERSE", "boot", 1);
    EXPECT_EQ(InjectFailure(HVB_FAIL_MEMCPY, 1, [&] { return FsHvbSetupHashtree(&item.value); }), 0);
}

HWTEST_F(FsHvbBranchTest, SnapshotReverseAndPathPrefix, TestSize.Level0)
{
    ASSERT_EQ(Prepare(), 0);
    setenv("HVB_SNAPSHOT_REVERSE", "boot", 1);
    OwnedItem item("dm-test");
    EXPECT_EQ(FsHvbSetupHashtree(&item.value), 0);
    OwnedTarget target;
    auto cert = MakeCert();
    ASSERT_EQ(FsHvbConstructVerityTarget(&target.value, "dm-test", &cert), 0);
    EXPECT_EQ(std::string(target.value.paras).find("1 /dev/block/dm-test /dev/block/dm-test "), 0u);
}

HWTEST_F(FsHvbBranchTest, ExtHashtreeFailureMatrixAndRetry, TestSize.Level0)
{
    for (const char *mode : {"Fail", "ZeroDataBlock", "ZeroHashBlock", "InvalidHash"}) {
        ASSERT_EQ(Prepare(EXT_HVB), 0);
        setenv("CERT_VALUE", mode, 1);
        char *path = nullptr;
        EXPECT_EQ(VerifyExtHvbImage("/dev/test", "boot", &path), -1);
        EXPECT_EQ(path, nullptr);
        unsetenv("CERT_VALUE");
    }
    for (const auto &failure : DM_FAILURE_CASES) {
        SCOPED_TRACE(failure.mode);
        SCOPED_TRACE(failure.nullPath);
        ASSERT_EQ(Prepare(EXT_HVB), 0);
        SetDmFailure(failure);
        char *path = nullptr;
        EXPECT_EQ(VerifyExtHvbImage("/dev/test", "boot", &path), -1);
        EXPECT_EQ(path, nullptr);
    }
    unsetenv("FSDM_NULL_PATH");
    ASSERT_EQ(Prepare(EXT_HVB), 0);
    char *path = nullptr;
    EXPECT_EQ(VerifyExtHvbImage("/dev/test", "boot", &path), 0);
    EXPECT_STREQ(path, STARTUP_INIT_UT_PATH "/libfs_hvb/dm-test");
    free(path);
}

HWTEST_F(FsHvbBranchTest, ExtAllocationCopyAndFormattingFailures, TestSize.Level0)
{
    const HvbMockFailure failures[] = {HVB_FAIL_MALLOC, HVB_FAIL_MEMCPY, HVB_FAIL_CALLOC, HVB_FAIL_SNPRINTF};
    for (HvbMockFailure failure : failures) {
        ASSERT_EQ(Prepare(EXT_HVB), 0);
        char *path = nullptr;
        EXPECT_EQ(InjectFailure(failure, 1, [&] { return VerifyExtHvbImage("/dev/test", "boot", &path); }), -1);
        EXPECT_EQ(path, nullptr);
    }
    // EXT hash-algorithm copy occurs before any hashtree allocations.
    FsHvbFinal(EXT_HVB);
    EXPECT_EQ(InjectFailure(HVB_FAIL_MEMCPY, 1, [&] { return FsHvbInit(EXT_HVB); }), -1);
}

HWTEST_F(FsHvbBranchTest, QuickfixFallbackAndPatchedHashtree, TestSize.Level0)
{
    struct hvb_cert cert = {};
    char name[] = "patch";
    EXPECT_EQ(QuickfixIsImagePatch(name, QUICKFIX_COMPARE_SNAPSHOT_NAME), -1);
    EXPECT_EQ(QuickfixFsHvbGetImageCert(&cert, name), -1);
    ASSERT_EQ(Prepare(), 0);
    setenv("HVB_QUICKFIX", "patch", 1);
    {
        OwnedItem item("patch");
        EXPECT_EQ(FsHvbSetupHashtree(&item.value), -1);
    }
    setenv("HVB_QUICKFIX_CERT", "Succeed", 1);
    {
        OwnedItem item("patch");
        EXPECT_EQ(FsHvbSetupHashtree(&item.value), 0);
    }
    OwnedTarget target;
    cert = MakeCert();
    ASSERT_EQ(FsHvbConstructVerityTarget(&target.value, "patch", &cert), 0);
    EXPECT_EQ(std::string(target.value.paras).find("1 /dev/block/patch /dev/block/patch "), 0u);
}

HWTEST_F(FsHvbBranchTest, PartitionReadEveryNullArgument, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    uint8_t buffer = 0;
    uint64_t count = 99;
    EXPECT_EQ(ops->read_partition(nullptr, "boot", 0, 1, &buffer, &count), HVB_IO_ERROR_IO);
    EXPECT_EQ(ops->read_partition(ops, nullptr, 0, 1, &buffer, &count), HVB_IO_ERROR_IO);
    EXPECT_EQ(ops->read_partition(ops, "boot", 0, 1, nullptr, &count), HVB_IO_ERROR_IO);
    EXPECT_EQ(ops->read_partition(ops, "boot", 0, 1, &buffer, nullptr), HVB_IO_ERROR_IO);
    EXPECT_EQ(count, 99u);
}

HWTEST_F(FsHvbBranchTest, PartitionPrimaryPathAndReadLengths, TestSize.Level0)
{
    std::vector<uint8_t> image(4096, 0x5a);
    image[3] = 0x37;
    ASSERT_TRUE(Image("by-name/boot", image));
    std::array<uint8_t, 8> buffer = {};
    uint64_t count = 99;
    EXPECT_EQ(Read("boot", 3, 1, buffer.data(), &count), HVB_IO_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(buffer[0], 0x37);
    EXPECT_EQ(Read("boot", 0, 0, buffer.data(), &count), HVB_IO_OK);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(Read("boot", 4094, 8, buffer.data(), &count), HVB_IO_ERROR_IO);
    EXPECT_EQ(Read("boot", -1, 1, buffer.data(), &count), HVB_IO_OK);
    EXPECT_EQ(buffer[0], 0x5a);
    EXPECT_EQ(HvbMockOpenCount(), HvbMockCloseCount());
}

HWTEST_F(FsHvbBranchTest, PartitionAbSlotMatrix, TestSize.Level0)
{
    ASSERT_TRUE(Image("by-name/boot_a", {0x0a}));
    ASSERT_TRUE(Image("by-name/boot_b", {0x0b}));
    setenv("BOOTSLOTS_VALUE", "2", 1);
    for (const char *slot : {"0", "1", "2", "3", "999", "-1"}) {
        SCOPED_TRACE(slot);
        setenv("CURRENTSLOT_VALUE", slot, 1);
        uint8_t buffer = 0;
        uint64_t count = 0;
        EXPECT_EQ(Read("boot", 0, 1, &buffer, &count), HVB_IO_OK);
        EXPECT_EQ(buffer, strcmp(slot, "2") == 0 ? 0x0b : 0x0a);
        EXPECT_EQ(count, 1u);
    }
    for (const char *slots : {"-1", "0", "1"}) {
        setenv("BOOTSLOTS_VALUE", slots, 1);
        uint8_t buffer;
        uint64_t count;
        EXPECT_EQ(Read("boot", 0, 1, &buffer, &count), HVB_IO_ERROR_OOM);
    }
    EXPECT_EQ(HvbMockOpenCount(), HvbMockCloseCount());
}

HWTEST_F(FsHvbBranchTest, PartitionForwardSnapshotAndQuickfix, TestSize.Level0)
{
    ASSERT_TRUE(Image("snapshot", {0x42}));
    setenv("HVB_SNAPSHOT_FORWARD", "snapshot", 1);
    uint8_t buffer = 0;
    uint64_t count = 0;
    EXPECT_EQ(Read("boot", 0, 1, &buffer, &count), HVB_IO_OK);
    EXPECT_EQ(buffer, 0x42);
    unsetenv("HVB_SNAPSHOT_FORWARD");
    ASSERT_TRUE(Image("patch", {0x43}));
    setenv("HVB_QUICKFIX", "patch", 1);
    EXPECT_EQ(Read("patch", 0, 1, &buffer, &count), HVB_IO_OK);
    EXPECT_EQ(buffer, 0x43);
    EXPECT_EQ(HvbMockOpenCount(), HvbMockCloseCount());
}

HWTEST_F(FsHvbBranchTest, PartitionAllocationFailures, TestSize.Level0)
{
    uint8_t buffer;
    uint64_t count = 99;
    EXPECT_EQ(InjectFailure(HVB_FAIL_MALLOC, 1, [&] {
        return Read("module_update", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    EXPECT_EQ(InjectFailure(HVB_FAIL_CALLOC, 1, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    setenv("BOOTSLOTS_VALUE", "2", 1);
    EXPECT_EQ(InjectFailure(HVB_FAIL_CALLOC, 2, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    EXPECT_EQ(count, 99u);
}

HWTEST_F(FsHvbBranchTest, PartitionCopyFailures, TestSize.Level0)
{
    uint8_t buffer;
    uint64_t count = 99;
    EXPECT_EQ(InjectFailure(HVB_FAIL_MEMCPY, 1, [&] {
        return Read("module_update", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    EXPECT_EQ(InjectFailure(HVB_FAIL_MEMCPY, 1, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    EXPECT_EQ(count, 99u);
}

HWTEST_F(FsHvbBranchTest, PartitionFormattingFailures, TestSize.Level0)
{
    uint8_t buffer;
    uint64_t count;
    EXPECT_EQ(InjectFailure(HVB_FAIL_SNPRINTF, 1, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    setenv("BOOTSLOTS_VALUE", "2", 1);
    EXPECT_EQ(InjectFailure(HVB_FAIL_SNPRINTF, 2, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_OOM);
    setenv("HVB_SNAPSHOT_FORWARD", "snapshot", 1);
    ASSERT_TRUE(Image("by-name/boot", {0x22}));
    EXPECT_EQ(InjectFailure(HVB_FAIL_SNPRINTF, 1, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_OK);
    unsetenv("HVB_SNAPSHOT_FORWARD");
    setenv("HVB_QUICKFIX", "boot", 1);
    EXPECT_EQ(InjectFailure(HVB_FAIL_SNPRINTF, 1, [&] {
        return Read("boot", 0, 1, &buffer, &count);
    }), HVB_IO_OK);
}

HWTEST_F(FsHvbBranchTest, PartitionOpenReadAndSizeFailuresCloseDescriptors, TestSize.Level0)
{
    ASSERT_TRUE(Image("module.img", std::vector<uint8_t>(4096, 0x33)));
    uint8_t buffer;
    uint64_t count = 99;
    EXPECT_EQ(InjectFailure(HVB_FAIL_OPEN, 1, [&] {
        return Read("module_update", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_IO);
    EXPECT_EQ(InjectFailure(HVB_FAIL_READ, 1, [&] {
        return Read("module_update", 0, 1, &buffer, &count);
    }), HVB_IO_ERROR_IO);
    EXPECT_EQ(InjectFailure(HVB_FAIL_READ, 1, [&] {
        return Read("module_update", -1, 1, &buffer, &count);
    }), HVB_IO_ERROR_IO);
    EXPECT_EQ(InjectFailure(HVB_FAIL_LSEEK64, 2, [&] {
        return Read("module_update", -1, 1, &buffer, &count);
    }), HVB_IO_ERROR_IO);
    EXPECT_EQ(HvbMockOpenCount(), HvbMockCloseCount());
    EXPECT_EQ(count, 99u);
}

HWTEST_F(FsHvbBranchTest, PartitionFilesystemHeaderMatrix, TestSize.Level0)
{
    for (bool erofs : {false, true}) {
        for (bool validExtension : {false, true}) {
            SCOPED_TRACE(erofs);
            SCOPED_TRACE(validExtension);
            std::vector<uint8_t> image(SZ_4KB + sizeof(ExtheaderV1) + 1, 0);
            if (erofs) {
                struct erofs_super_block super = {};
                super.magic = EROFS_SUPER_MAGIC;
                super.blocks = 1;
                ASSERT_EQ(memcpy_s(image.data() + SZ_1KB, image.size() - SZ_1KB, &super, sizeof(super)), EOK);
            } else {
                ext4_super_block super = {};
                super.s_magic = EXT4_SUPER_MAGIC;
                super.s_blocks_count_lo = 1;
                ASSERT_EQ(memcpy_s(image.data() + SZ_1KB, image.size() - SZ_1KB, &super, sizeof(super)), EOK);
            }
            ExtheaderV1 header = {};
            header.magicNumber = validExtension ? EXTHDR_MAGIC : 0;
            header.partSize = SZ_4KB;
            ASSERT_EQ(memcpy_s(image.data() + SZ_4KB, image.size() - SZ_4KB, &header, sizeof(header)), EOK);
            image[SZ_4KB - 1] = 0x41;
            image.back() = 0x42;
            ASSERT_TRUE(Image("module.img", image));
            uint8_t buffer = 0;
            uint64_t count = 0;
            ASSERT_EQ(Read("module_update", -1, 1, &buffer, &count), HVB_IO_OK);
            EXPECT_EQ(buffer, validExtension ? 0x41 : 0x42);
            EXPECT_EQ(count, 1u);
        }
    }
    ASSERT_TRUE(Image("module.img", std::vector<uint8_t>(100, 0)));
    uint8_t buffer;
    uint64_t count;
    EXPECT_EQ(Read("module_update", -1, 1, &buffer, &count), HVB_IO_ERROR_IO);
    EXPECT_EQ(HvbMockOpenCount(), HvbMockCloseCount());
}

HWTEST_F(FsHvbBranchTest, CmdlineDestinationBoundaries, TestSize.Level0)
{
    ASSERT_TRUE(Cmdline("ohos.boot.hvb.hash_algo=sha256 "));
    char buffer[7] = {};
    EXPECT_NE(FsHvbGetValueFromCmdLine(buffer, 0, "ohos.boot.hvb.hash_algo"), 0);
    EXPECT_NE(FsHvbGetValueFromCmdLine(buffer, 6, "ohos.boot.hvb.hash_algo"), 0);
    EXPECT_EQ(FsHvbGetValueFromCmdLine(buffer, sizeof(buffer), "ohos.boot.hvb.hash_algo"), 0);
    EXPECT_STREQ(buffer, "sha256");
}

HWTEST_F(FsHvbBranchTest, PrivateVerifiedDataAndDigestSelectors, TestSize.Level0)
{
    struct hvb_verified_data **data = nullptr;
    EXPECT_EQ(HvbTestGetCurrVerifiedData(MAIN_HVB, nullptr), HVB_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(HvbTestGetCurrVerifiedData(MAIN_HVB, &data), HVB_OK);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(*data, nullptr);
    EXPECT_EQ(HvbTestGetCurrVerifiedData(EXT_HVB, &data), HVB_OK);
    EXPECT_EQ(HvbTestGetCurrVerifiedData(static_cast<InitHvbType>(99), &data), HVB_ERROR_INVALID_ARGUMENT);
    char buffer[32] = {};
    EXPECT_EQ(HvbTestGetHashStr(buffer, sizeof(buffer), static_cast<InitHvbType>(99)), -1);
    EXPECT_EQ(HvbTestGetDigestStr(buffer, sizeof(buffer), static_cast<InitHvbType>(99)), -1);
    EXPECT_EQ(HvbTestGetHashStr(buffer, 2, EXT_HVB), -1);
    ASSERT_EQ(HvbTestGetHashStr(buffer, sizeof(buffer), EXT_HVB), 0);
    EXPECT_STREQ(buffer, "sha256");
    ASSERT_TRUE(Cmdline("ohos.boot.hvb.digest=01 ohos.boot.hvb.ext_rvt=02 "));
    ASSERT_EQ(HvbTestGetDigestStr(buffer, sizeof(buffer), MAIN_HVB), 0);
    EXPECT_STREQ(buffer, "01");
    ASSERT_EQ(HvbTestGetDigestStr(buffer, sizeof(buffer), EXT_HVB), 0);
    EXPECT_STREQ(buffer, "02");
}

HWTEST_F(FsHvbBranchTest, PrivateHashArgumentAndOutputBoundaries, TestSize.Level0)
{
    char digest[64] = {};
    struct hvb_cert_data cert = {};
    uint32_t size = 32;
    setenv("HASH_VALUE", "AllSucceed", 1);
    EXPECT_EQ(HvbTestSha256(digest, 31, &cert, 1), -1);
    EXPECT_EQ(HvbTestSha256(digest, 32, &cert, 0), 0);
    EXPECT_EQ(digest[0], 1);
    EXPECT_EQ(HvbTestSm3(nullptr, &size, &cert, 1), -1);
    EXPECT_EQ(HvbTestSm3(digest, &size, nullptr, 1), -1);
    EXPECT_EQ(HvbTestSm3(digest, nullptr, &cert, 1), -1);
    size = 31;
    EXPECT_EQ(HvbTestSm3(digest, &size, &cert, 1), -1);
    size = 32;
    EXPECT_EQ(HvbTestSm3(digest, &size, &cert, 0), 0);
    EXPECT_EQ(digest[0], 1);
}

HWTEST_F(FsHvbBranchTest, PrivateHexDestinationCapacity, TestSize.Level0)
{
    char output[2] = {};
    char input[] = "ab01";
    EXPECT_EQ(HvbTestHexToBin(output, 1, input, 4), -1);
    EXPECT_EQ(HvbTestHexToBin(output, sizeof(output), input, 3), -1);
    EXPECT_EQ(HvbTestHexToBin(output, sizeof(output), input, 4), 0);
    EXPECT_EQ(static_cast<uint8_t>(output[0]), 0xab);
    EXPECT_EQ(static_cast<uint8_t>(output[1]), 1);
    EXPECT_EQ(HvbTestHexToBin(output, sizeof(output), input, 0), 0);
}

HWTEST_F(FsHvbBranchTest, PrivateFecNullAndBlockSizeChecks, TestSize.Level0)
{
    auto cert = MakeCert(true);
    char path[] = "/dev/test";
    char buffer[512] = {};
    char *cursor = buffer;
    char *end = buffer + sizeof(buffer);
    EXPECT_EQ(HvbTestAddFec(nullptr, path, &cursor, end), -1);
    EXPECT_EQ(HvbTestAddFec(&cert, nullptr, &cursor, end), -1);
    EXPECT_EQ(HvbTestAddFec(&cert, path, nullptr, end), -1);
    EXPECT_EQ(HvbTestAddFec(&cert, path, &cursor, nullptr), -1);
    cert.data_block_size = 0;
    EXPECT_EQ(HvbTestAddFec(&cert, path, &cursor, end), -1);
    cursor = buffer;
    cert.data_block_size = 4096;
    cert.hash_block_size = 0;
    EXPECT_EQ(HvbTestAddFec(&cert, path, &cursor, end), -1);
    cursor = buffer;
    cert.hash_block_size = 4096;
    EXPECT_EQ(HvbTestAddFec(&cert, path, &cursor, end), 0);
    *cursor = '\0';
    EXPECT_STREQ(buffer, "use_fec_from_device /dev/test fec_roots 2 fec_blocks 2 fec_start 2 ");
}

HWTEST_F(FsHvbBranchTest, PrivateSnapshotAndExtStateChecks, TestSize.Level0)
{
    EXPECT_EQ(HvbTestFindVab("boot", nullptr), -1);
    char *path = nullptr;
    EXPECT_EQ(HvbTestExtSetup("/dev/test", "boot", &path), -1);
    EXPECT_EQ(path, nullptr);
}

HWTEST_F(FsHvbBranchTest, PrivatePartitionArgumentChecks, TestSize.Level0)
{
    EXPECT_EQ(HvbTestImageSize(-1, "boot"), -1);
    size_t index = 99;
    EXPECT_FALSE(HvbTestIsExtPartition(nullptr, 0, &index));
    EXPECT_FALSE(HvbTestIsExtPartition("boot", 4, nullptr));
    EXPECT_FALSE(HvbTestIsExtPartition("boot", 4, &index));
    EXPECT_TRUE(HvbTestIsExtPartition("module_update", strlen("module_update"), &index));
    EXPECT_EQ(index, 0u);
    EXPECT_EQ(HvbTestExtPath(1), nullptr);
    char *path = HvbTestExtPath(0);
    ASSERT_NE(path, nullptr);
    EXPECT_STREQ(path, "/data/module_update/active/ModuleTrain/module.img");
    free(path);
}
} // namespace init_ut
