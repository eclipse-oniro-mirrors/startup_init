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

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "erofs_overlay_common.h"
#include "func_wrapper.h"

using namespace testing::ext;

extern "C" {
#define ASM_REAL_PREFIX "_" "_real_"
int RealGetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length)
    __asm__(ASM_REAL_PREFIX "GetMapperAddrForMerge");
int RealMkdir(const char *pathname, mode_t mode) __asm__(ASM_REAL_PREFIX "mkdir");
int RealRmdir(const char *pathname) __asm__(ASM_REAL_PREFIX "rmdir");
#undef ASM_REAL_PREFIX
}

namespace init_ut {
namespace {
constexpr mode_t TEST_DIR_MODE = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
const char *TEST_DATA_DIR = STARTUP_INIT_UT_PATH "/data";
const char *TEST_EROFS_IMAGE = STARTUP_INIT_UT_PATH "/data/erofs_common_image";
const char *TEST_EROFS_SHORT = STARTUP_INIT_UT_PATH "/data/erofs_common_short";
const char *TEST_EROFS_DIR = STARTUP_INIT_UT_PATH "/data/erofs_common_dir";

bool EnsureDir(const std::string &dir)
{
    if (dir.empty()) {
        return false;
    }
    size_t pos = dir[0] == '/' ? 1 : 0;
    while (pos <= dir.size()) {
        size_t next = dir.find('/', pos);
        std::string part = dir.substr(0, next);
        if (!part.empty() && RealMkdir(part.c_str(), TEST_DIR_MODE) != 0 && errno != EEXIST) {
            return false;
        }
        if (next == std::string::npos) {
            break;
        }
        pos = next + 1;
    }
    return true;
}

bool EnsureTestDir(void)
{
    return EnsureDir(TEST_DATA_DIR);
}

bool TruncateFile(FILE *file, uint64_t size)
{
    return fflush(file) == 0 && ftruncate(fileno(file), static_cast<off_t>(size)) == 0;
}

bool WriteDataAt(FILE *file, uint64_t offset, const void *data, size_t size)
{
    if (fseeko(file, static_cast<off_t>(offset), SEEK_SET) != 0) {
        return false;
    }
    return fwrite(data, 1, size, file) == size;
}

bool CreateSizedFile(const char *fileName, uint64_t size)
{
    if (!EnsureTestDir()) {
        return false;
    }
    FILE *file = fopen(fileName, "wb+");
    if (file == nullptr) {
        return false;
    }
    bool ret = TruncateFile(file, size);
    ret = (fclose(file) == 0) && ret;
    return ret;
}

bool CreateErofsImage(const char *fileName, uint32_t blocks, uint64_t totalSize,
    bool hasExtHeader = false, uint64_t partSize = 0)
{
    if (!EnsureTestDir()) {
        return false;
    }
    FILE *file = fopen(fileName, "wb+");
    if (file == nullptr) {
        return false;
    }

    struct erofs_super_block super {};
    super.magic = EROFS_SUPER_MAGIC;
    super.blocks = blocks;
    bool ret = WriteDataAt(file, EROFS_SUPER_BLOCK_START_POSITION, &super, sizeof(super));
    if (ret && hasExtHeader) {
        struct extheader_v1 header {};
        header.magic_number = EXTHDR_MAGIC;
        header.part_size = partSize;
        ret = WriteDataAt(file, static_cast<uint64_t>(blocks) * BLOCK_SIZE_UINT, &header, sizeof(header));
    }

    if (ret) {
        ret = TruncateFile(file, totalSize);
    }
    ret = (fclose(file) == 0) && ret;
    return ret;
}

bool CreateExtHeaderFile(const char *fileName, uint64_t offset, uint32_t magic, uint64_t partSize)
{
    if (!EnsureTestDir()) {
        return false;
    }
    FILE *file = fopen(fileName, "wb+");
    if (file == nullptr) {
        return false;
    }
    struct extheader_v1 header {};
    header.magic_number = magic;
    header.part_size = partSize;
    bool ret = WriteDataAt(file, offset, &header, sizeof(header));
    ret = (fclose(file) == 0) && ret;
    return ret;
}

void CleanupTestFiles(void)
{
    remove(TEST_EROFS_IMAGE);
    remove(TEST_EROFS_SHORT);
    RealRmdir(TEST_EROFS_DIR);
}
} // namespace

class RemountErofsOverlayCommonUnitTest : public testing::Test {
public:
    void SetUp() override
    {
        UpdateOpenFunc(nullptr);
        UpdateCloseFunc(nullptr);
        UpdateAccessFunc(nullptr);
        UpdateMkdirFunc(nullptr);
        UpdateIoctlFunc(nullptr);
        CleanupTestFiles();
    }

    void TearDown() override
    {
        CleanupTestFiles();
        UpdateOpenFunc(nullptr);
        UpdateCloseFunc(nullptr);
        UpdateAccessFunc(nullptr);
        UpdateMkdirFunc(nullptr);
        UpdateIoctlFunc(nullptr);
    }
};

HWTEST_F(RemountErofsOverlayCommonUnitTest, AlignTo_001, TestSize.Level0)
{
    EXPECT_EQ(AlignTo(4096, 4096), 4096);
    EXPECT_EQ(AlignTo(4097, 4096), 8192);
    EXPECT_EQ(AlignTo(4097, 0), 4097);
}

HWTEST_F(RemountErofsOverlayCommonUnitTest, LookupErofsEnd_001, TestSize.Level0)
{
    EXPECT_EQ(LookupErofsEnd(STARTUP_INIT_UT_PATH "/data/notexist_erofs_image"), 0);

    ASSERT_TRUE(CreateSizedFile(TEST_EROFS_SHORT, sizeof(struct erofs_super_block) - 1));
    EXPECT_EQ(LookupErofsEnd(TEST_EROFS_SHORT), 0);

    struct erofs_super_block badSuper {};
    badSuper.magic = EXT4_SUPER_MAGIC;
    ASSERT_TRUE(CreateSizedFile(TEST_EROFS_IMAGE, EROFS_SUPER_BLOCK_START_POSITION + sizeof(badSuper)));
    FILE *file = fopen(TEST_EROFS_IMAGE, "rb+");
    ASSERT_NE(file, nullptr);
    ASSERT_TRUE(WriteDataAt(file, EROFS_SUPER_BLOCK_START_POSITION, &badSuper, sizeof(badSuper)));
    ASSERT_EQ(fclose(file), 0);
    EXPECT_EQ(LookupErofsEnd(TEST_EROFS_IMAGE), 0);

    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, 3, 3 * BLOCK_SIZE_UINT));
    EXPECT_EQ(LookupErofsEnd(TEST_EROFS_IMAGE), 3 * BLOCK_SIZE_UINT);
}

HWTEST_F(RemountErofsOverlayCommonUnitTest, GetImgSize_001, TestSize.Level0)
{
    constexpr uint64_t offset = 8192;
    constexpr uint64_t partSize = 123456;
    EXPECT_EQ(GetImgSize(STARTUP_INIT_UT_PATH "/data/notexist_ext_header", offset), 0);

    ASSERT_TRUE(CreateSizedFile(TEST_EROFS_IMAGE, 1));
    EXPECT_EQ(GetImgSize(TEST_EROFS_IMAGE, UINT64_MAX), 0);

    ASSERT_TRUE(CreateSizedFile(TEST_EROFS_SHORT, offset + sizeof(struct extheader_v1) - 1));
    EXPECT_EQ(GetImgSize(TEST_EROFS_SHORT, offset), 0);

    ASSERT_TRUE(CreateExtHeaderFile(TEST_EROFS_IMAGE, offset, EXT4_SUPER_MAGIC, partSize));
    EXPECT_EQ(GetImgSize(TEST_EROFS_IMAGE, offset), 0);

    ASSERT_TRUE(CreateExtHeaderFile(TEST_EROFS_IMAGE, offset, EXTHDR_MAGIC, partSize));
    EXPECT_EQ(GetImgSize(TEST_EROFS_IMAGE, offset), partSize);
}

HWTEST_F(RemountErofsOverlayCommonUnitTest, GetFsSize_001, TestSize.Level0)
{
    EXPECT_EQ(GetFsSize(-1), 0);

    constexpr uint64_t fileSize = 65536;
    ASSERT_TRUE(CreateSizedFile(TEST_EROFS_IMAGE, fileSize));
    int fd = open(TEST_EROFS_IMAGE, O_RDONLY | O_LARGEFILE);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(GetFsSize(fd), fileSize);
    close(fd);

    ASSERT_TRUE(EnsureTestDir());
    RealRmdir(TEST_EROFS_DIR);
    ASSERT_EQ(RealMkdir(TEST_EROFS_DIR, TEST_DIR_MODE), 0);
    fd = open(TEST_EROFS_DIR, O_RDONLY | O_DIRECTORY);
    ASSERT_GE(fd, 0);
    errno = 0;
    EXPECT_EQ(GetFsSize(fd), 0);
    EXPECT_EQ(errno, EACCES);
    close(fd);
}

HWTEST_F(RemountErofsOverlayCommonUnitTest, GetBlockSize_001, TestSize.Level0)
{
    EXPECT_EQ(GetBlockSize(STARTUP_INIT_UT_PATH "/data/notexist_block"), 0);

    constexpr uint64_t fileSize = 32768;
    ASSERT_TRUE(CreateSizedFile(TEST_EROFS_IMAGE, fileSize));
    EXPECT_EQ(GetBlockSize(TEST_EROFS_IMAGE), fileSize);
}

HWTEST_F(RemountErofsOverlayCommonUnitTest, GetMapperAddr_001, TestSize.Level0)
{
    uint64_t start = 0;
    uint64_t length = 0;
    EXPECT_EQ(GetMapperAddr(STARTUP_INIT_UT_PATH "/data/notexist_mapper", &start, &length), -1);

    constexpr uint32_t blocks = 1;
    constexpr uint64_t erofsEnd = blocks * BLOCK_SIZE_UINT;
    constexpr uint64_t totalSize = erofsEnd + MIN_DM_SIZE + 4096;
    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, totalSize));
    EXPECT_EQ(GetMapperAddr(TEST_EROFS_IMAGE, &start, &length), 0);
    EXPECT_EQ(start, erofsEnd);
    EXPECT_EQ(length, totalSize - erofsEnd);

    constexpr uint64_t partSize = 5000;
    constexpr uint64_t alignedPartSize = 16384;
    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, alignedPartSize + MIN_DM_SIZE + 1, true, partSize));
    EXPECT_EQ(GetMapperAddr(TEST_EROFS_IMAGE, &start, &length), 0);
    EXPECT_EQ(start, alignedPartSize);
    EXPECT_EQ(length, MIN_DM_SIZE + 1);

    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, erofsEnd));
    EXPECT_EQ(GetMapperAddr(TEST_EROFS_IMAGE, &start, &length), -1);
    EXPECT_EQ(length, 0);

    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, erofsEnd + MIN_DM_SIZE - 1));
    EXPECT_EQ(GetMapperAddr(TEST_EROFS_IMAGE, &start, &length), -1);
    EXPECT_EQ(length, MIN_DM_SIZE - 1);
}

HWTEST_F(RemountErofsOverlayCommonUnitTest, GetMapperAddrForMerge_001, TestSize.Level0)
{
    uint64_t start = 0;
    uint64_t length = 0;
    EXPECT_EQ(RealGetMapperAddrForMerge(STARTUP_INIT_UT_PATH "/data/notexist_merge_mapper", &start, &length), -1);

    constexpr uint32_t blocks = 1;
    constexpr uint64_t erofsEnd = blocks * BLOCK_SIZE_UINT;
    constexpr uint64_t totalSize = erofsEnd + 4096;
    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, totalSize));
    EXPECT_EQ(RealGetMapperAddrForMerge(TEST_EROFS_IMAGE, &start, &length), 0);
    EXPECT_EQ(start, erofsEnd);
    EXPECT_EQ(length, totalSize - erofsEnd);

    constexpr uint64_t partSize = 5000;
    constexpr uint64_t alignedPartSize = 16384;
    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, alignedPartSize + 4096, true, partSize));
    EXPECT_EQ(RealGetMapperAddrForMerge(TEST_EROFS_IMAGE, &start, &length), 0);
    EXPECT_EQ(start, alignedPartSize);
    EXPECT_EQ(length, 4096);

    ASSERT_TRUE(CreateErofsImage(TEST_EROFS_IMAGE, blocks, erofsEnd));
    EXPECT_EQ(RealGetMapperAddrForMerge(TEST_EROFS_IMAGE, &start, &length), 0);
    EXPECT_EQ(start, erofsEnd);
    EXPECT_EQ(length, 0);
}
} // namespace init_ut
