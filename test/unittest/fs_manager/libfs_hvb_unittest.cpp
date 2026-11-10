/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <gtest/gtest.h>
#include <cstring>
#include "libfs_hvb_mock.h"
#include "fs_hvb.h"
#include "fs_hvb_image_patch.h"
#include "init_utils.h"
#include "fs_manager/ext4_super_block.h"
#include "fs_manager/erofs_super_block.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class FsHvbUnitTest : public testing::Test {
public:
    void SetUp() override
    {
        HvbMockReset();
        FsHvbFinal(MAIN_HVB);
        FsHvbFinal(EXT_HVB);
        for (const char *key : {"VERIFY_VALUE", "HASH_VALUE", "FSDM_VALUE", "CERT_VALUE", "BOOTSLOTS_VALUE",
            "CURRENTSLOT_VALUE", "HVB_SNAPSHOT_FORWARD", "HVB_SNAPSHOT_REVERSE", "HVB_QUICKFIX",
            "HVB_QUICKFIX_CERT", "FSDM_NULL_PATH"}) {
            unsetenv(key);
        }
        remove(BOOT_CMD_LINE);
        remove(STARTUP_INIT_UT_PATH "/libfs_hvb/module.img");
    }
    void TearDown() override
    {
        SetUp();
    }
    static void TearDownTestCase(void) {};
};

void CreateTestFile(const char *fileName, const char *data)
{
    CheckAndCreateDir(fileName);
    printf("PrepareParamTestData for %s\n", fileName);
    FILE *tmpFile = fopen(fileName, "a+");
    if (tmpFile != nullptr) {
        fprintf(tmpFile, "%s", data);
        (void)fflush(tmpFile);
        fclose(tmpFile);
    }
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_001, TestSize.Level0)
{
    const char *cmdLine;
    int ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, HVB_ERROR_INVALID_ARGUMENT);

    setenv("VERIFY_VALUE", "PartFail", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, HVB_ERROR_UNSUPPORTED_VERSION);

    setenv("VERIFY_VALUE", "Succeed", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    cmdLine = "ohos.boot.hvb.hash_algo=test "; // HVB_CMDLINE_HASH_ALG
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    cmdLine = "ohos.boot.hvb.digest=1 "; // HVB_CMDLINE_CERT_DIGEST
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);
    remove(BOOT_CMD_LINE);

    cmdLine = "ohos.boot.hvb.hash_algo=test ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=1234 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);
    remove(BOOT_CMD_LINE);

    setenv("HASH_VALUE", "InitFail", 1); // sha256
    cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "UpdateFail", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "FinalFail", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "AllSucceed", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, 0);
    remove(BOOT_CMD_LINE);
    ret = FsHvbFinal(MAIN_HVB); //clear vd
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_002, TestSize.Level0)
{
    const char *cmdLine;
    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "InitFail", 1); // sm3
    cmdLine = "ohos.boot.hvb.hash_algo=sm3 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    int ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "UpdateFail", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "FinalFail", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "AllSucceed", 1);
    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_001, TestSize.Level0)
{
    FstabItem fsItem;
    char testStr[10] = "testStr";
    char testDev[] = "/dev/block/platform/xxx/by-name/boot";
    fsItem.deviceName = (char *)malloc(sizeof(testDev) + 1);
    memcpy_s(fsItem.deviceName, sizeof(testDev) + 1, &testDev[0], sizeof(testDev));
    fsItem.mountPoint = &testStr[0];
    fsItem.fsType = &testStr[0];
    fsItem.mountOptions = &testStr[0];
    fsItem.fsManagerFlags = 1;
    fsItem.next = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    CreateTestFile(BOOT_CMD_LINE, "ohos.boot.hvb.hash_algo=sha256 ohos.boot.hvb.digest=01 ");
    ASSERT_EQ(FsHvbInit(MAIN_HVB), 0);
    int ret = FsHvbSetupHashtree(nullptr);
    EXPECT_EQ(ret, -1);

    setenv("FSDM_VALUE", "InitFail", 1);
    ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_EQ(ret, -1);

    setenv("FSDM_VALUE", "CreateFail", 1);
    ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_EQ(ret, -1);

    setenv("FSDM_VALUE", "AllSucceed", 1);
    ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_EQ(ret, 0);

    unsetenv("FSDM_VALUE");
    free(fsItem.deviceName);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbFinal_001, TestSize.Level0)
{
    int ret = FsHvbFinal(MAIN_HVB);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbGetOps_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    EXPECT_NE(ops, nullptr);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbGetValueFromCmdLine_001, TestSize.Level0)
{
    const char *cmdLine;
    char hashAlg[32] = { 0 };

    int ret = FsHvbGetValueFromCmdLine(nullptr, sizeof(hashAlg), "ohos.boot.hvb.hash_algo");
    EXPECT_EQ(ret, -1);
    ret = FsHvbGetValueFromCmdLine(&hashAlg[0], sizeof(hashAlg), nullptr);
    EXPECT_EQ(ret, -1);

    cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbGetValueFromCmdLine(&hashAlg[0], sizeof(hashAlg), "ohos.boot.hvb.hash_algo");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(strcmp(hashAlg, "sha256"), 0);

    remove(BOOT_CMD_LINE);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_001, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "test";
    struct hvb_cert cert = {};
    uint8_t digest = 53;
    uint8_t salt = 55;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(nullptr, devName, &cert);
    EXPECT_EQ(ret, -1);
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, nullptr, &cert);
    EXPECT_EQ(ret, -1);
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, devName, nullptr);
    EXPECT_EQ(ret, -1);

    cert.image_len = 512;
    cert.data_block_size = 0;
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);
    cert.data_block_size = 8;
    cert.hash_block_size = 0;
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);

    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 4;
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);

    cert.hash_algo = 0;
    cert.hash_payload.digest = &digest;
    cert.digest_size = 1;
    cert.hash_payload.salt = &salt;
    cert.salt_size = 1;
    cert.fec_size = 1;
    cert.fec_num_roots = 1;
    cert.fec_offset = 8;
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);

    cert.hash_algo = 2;
    FsHvbDestoryVerityTarget(&target);
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);
    FsHvbDestoryVerityTarget(&target);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbDestoryVerityTarget_001, TestSize.Level0)
{
    DmVerityTarget target = {};

    target.paras = static_cast<char *>(calloc(1, 10));
    ASSERT_NE(target.paras, nullptr);
    FsHvbDestoryVerityTarget(&target);
    ASSERT_EQ(target.paras, nullptr);
    FsHvbDestoryVerityTarget(&target);
    EXPECT_EQ(target.paras, nullptr);
}

HWTEST_F(FsHvbUnitTest, Init_VerifyExtHvbImage_001, TestSize.Level0)
{
    const char *cmdLine;
    const char *devPath = "/dev/block/loop0";
    char *outPath = nullptr;
    int ret = VerifyExtHvbImage(devPath, "boot", &outPath);
    EXPECT_FALSE(ret == 0);

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("FSDM_VALUE", "AllSucceed", 1);
    cmdLine = "ohos.boot.hvb.ext_rvt=01 "; // HVB_CMDLINE_EXT_CERT_DIGEST
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = VerifyExtHvbImage(devPath, "module_update", &outPath);
    EXPECT_EQ(ret, -1);

    ret = VerifyExtHvbImage(devPath, "boot", &outPath);
    EXPECT_EQ(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("FSDM_VALUE");
    free(outPath);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetExt4Size_001, TestSize.Level0)
{
    ext4_super_block superBlock = {0};
    uint64_t imageSize;

    bool ret = CheckAndGetExt4Size(nullptr, &imageSize, "boot");
    EXPECT_FALSE(ret);

    ret = CheckAndGetExt4Size((const char*)&superBlock, &imageSize, "boot");
    EXPECT_FALSE(ret);

    superBlock.s_magic = EXT4_SUPER_MAGIC;
    ret = CheckAndGetExt4Size((const char*)&superBlock, &imageSize, "boot");
    EXPECT_TRUE(ret);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetErofsSize_001, TestSize.Level0)
{
    struct erofs_super_block superBlock = {0};
    uint64_t imageSize;

    bool ret = CheckAndGetErofsSize(nullptr, &imageSize, "boot");
    EXPECT_FALSE(ret);

    ret = CheckAndGetErofsSize((const char*)&superBlock, &imageSize, "boot");
    EXPECT_FALSE(ret);

    superBlock.magic = EROFS_SUPER_MAGIC;
    ret = CheckAndGetErofsSize((const char*)&superBlock, &imageSize, "boot");
    EXPECT_TRUE(ret);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetExtheaderSize_001, TestSize.Level0)
{
    uint64_t imageSize;
    bool ret = CheckAndGetExtheaderSize(-1, 0, &imageSize, "boot");
    EXPECT_FALSE(ret);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    int ret = ops->read_partition(ops, "boot_not_exit", 0, 1, nullptr, nullptr);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    void *buf = malloc(1);
    uint64_t outNumRead;
    ret = ops->read_partition(ops, "boot_not_exit", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_OOM);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbWriteToPartition_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    int ret = ops->write_partition(ops, "boot", 0, 1, buf);
    EXPECT_EQ(ret, HVB_IO_OK);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbInvaldateKey_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    bool outIsTrusted;
    int ret = ops->valid_rvt_key(ops, nullptr, 0, nullptr, 0, nullptr);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    ret = ops->valid_rvt_key(ops, nullptr, 0, nullptr, 0, &outIsTrusted);
    EXPECT_EQ(ret, HVB_IO_OK);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadRollbackIdx_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    int ret = ops->read_rollback(ops, 0, nullptr);
    uint64_t outRollbackIndex;
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    ret = ops->read_rollback(ops, 0, &outRollbackIndex);
    EXPECT_EQ(ret, HVB_IO_OK);
}

HWTEST_F(FsHvbUnitTest, Init_HvbWriteRollbackIdx_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    int ret = ops->write_rollback(ops, 0, 0);
    EXPECT_EQ(ret, HVB_IO_OK);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadLockState_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    bool locked;
    const char *cmdLine;
    int ret = ops->read_lock_state(ops, &locked);
    EXPECT_EQ(ret, HVB_IO_ERROR_NO_SUCH_VALUE);

    cmdLine = "ohos.boot.hvb.device_state=locked ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = ops->read_lock_state(ops, &locked);
    EXPECT_EQ(ret, HVB_IO_OK);
    EXPECT_EQ(locked, true);
    remove(BOOT_CMD_LINE);

    cmdLine = "ohos.boot.hvb.device_state=unlocked ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = ops->read_lock_state(ops, &locked);
    EXPECT_EQ(ret, HVB_IO_OK);
    EXPECT_EQ(locked, false);
    remove(BOOT_CMD_LINE);

    cmdLine = "ohos.boot.hvb.device_state=undefined ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = ops->read_lock_state(ops, &locked);
    EXPECT_EQ(ret, HVB_IO_ERROR_NO_SUCH_VALUE);
    remove(BOOT_CMD_LINE);
}

HWTEST_F(FsHvbUnitTest, Init_HvbGetSizeOfPartition_001, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    int ret = ops->get_partiton_size(ops, "boot", nullptr);
    uint64_t size;
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    ret = ops->get_partiton_size(ops, "boot", &size);
    EXPECT_EQ(ret, HVB_IO_OK);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadLockState_002, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    bool locked;
    int ret = ops->read_lock_state(nullptr, &locked);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    ret = ops->read_lock_state(ops, nullptr);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_002, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "test";
    struct hvb_cert cert = {};
    uint8_t digest = 53;
    uint8_t salt = 55;

    cert.image_len = 512;
    cert.data_block_size = 8;
    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 3;
    cert.hash_payload.digest = &digest;
    cert.digest_size = 1;
    cert.hash_payload.salt = &salt;
    cert.salt_size = 1;
    cert.fec_size = 0;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);
    FsHvbDestoryVerityTarget(&target);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_003, TestSize.Level0)
{
    const char *cmdLine;
    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    int ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, 0);

    ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, 0);

    remove(BOOT_CMD_LINE);
    FsHvbFinal(MAIN_HVB);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_004, TestSize.Level0)
{
    const char *cmdLine;
    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=02 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    int ret = FsHvbInit(MAIN_HVB);
    EXPECT_EQ(ret, -1);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbFinal_002, TestSize.Level0)
{
    int ret = FsHvbFinal((InitHvbType)99);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetExtheaderSize_002, TestSize.Level0)
{
    uint64_t imageSize = 0;
    ExtheaderV1 header = {};
    header.magicNumber = EXTHDR_MAGIC;
    header.partSize = 4096;

    FILE *fp = tmpfile();
    EXPECT_NE(fp, nullptr);
    fwrite(&header, sizeof(header), 1, fp);
    fflush(fp);
    int fd = fileno(fp);
    bool ret = CheckAndGetExtheaderSize(fd, 0, &imageSize, "test");
    EXPECT_TRUE(ret);
    EXPECT_EQ(imageSize, 4096);

    header.magicNumber = 0;
    rewind(fp);
    fwrite(&header, sizeof(header), 1, fp);
    fflush(fp);
    ret = CheckAndGetExtheaderSize(fd, 0, &imageSize, "test");
    EXPECT_FALSE(ret);

    fclose(fp);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_003, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "test";
    struct hvb_cert cert = {};
    uint8_t digest = 53;
    uint8_t salt = 55;

    cert.image_len = 512;
    cert.data_block_size = 8;
    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 0;
    cert.hash_payload.digest = &digest;
    cert.digest_size = 1;
    cert.hash_payload.salt = &salt;
    cert.salt_size = 1;
    cert.fec_size = 1;
    cert.fec_num_roots = 1;
    cert.fec_offset = 8;
    cert.data_block_size = 0;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);
    FsHvbDestoryVerityTarget(&target);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_005, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "/dev/block/test";
    struct hvb_cert cert = {};
    uint8_t digest = 53;
    uint8_t salt = 55;

    cert.image_len = 512;
    cert.data_block_size = 8;
    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 0;
    cert.hash_payload.digest = &digest;
    cert.digest_size = 1;
    cert.hash_payload.salt = &salt;
    cert.salt_size = 1;
    cert.fec_size = 0;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);
    FsHvbDestoryVerityTarget(&target);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbDestoryVerityTarget_002, TestSize.Level0)
{
    FsHvbDestoryVerityTarget(nullptr);

    DmVerityTarget target = {};
    FsHvbDestoryVerityTarget(&target);
    ASSERT_EQ(target.paras, nullptr);
    FsHvbDestoryVerityTarget(&target);
    EXPECT_EQ(target.paras, nullptr);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbGetValueFromCmdLine_002, TestSize.Level0)
{
    char hashAlg[32] = {0};

    const char *cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    int ret = FsHvbGetValueFromCmdLine(&hashAlg[0], sizeof(hashAlg), "ohos.boot.hvb.notexist");
    EXPECT_NE(ret, 0);

    remove(BOOT_CMD_LINE);
}

HWTEST_F(FsHvbUnitTest, Init_VerifyExtHvbImage_002, TestSize.Level0)
{
    const char *devPath = "/dev/block/loop0";
    char *outPath = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    const char *cmdLine = "ohos.boot.hvb.ext_rvt=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    int ret = VerifyExtHvbImage(devPath, "notexist", &outPath);
    EXPECT_NE(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    free(outPath);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_002, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    int ret = ops->read_partition(ops, "module_update", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_003, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    uint64_t outNumRead;
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char data[SZ_2KB] = {0};
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);

    void *buf = malloc(1);
    int ret = ops->read_partition(ops, "module_update", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_OK);
    free(buf);
    remove(path);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_004, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    uint64_t outNumRead;
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char data[SZ_2KB] = {0};
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);

    void *buf = malloc(1);
    int ret = ops->read_partition(ops, "module_update", -1, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_OK);
    free(buf);
    remove(path);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_005, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    uint64_t outNumRead;
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char fileData[SZ_4KB + sizeof(ExtheaderV1)] = {0};
    struct erofs_super_block *erofs = (struct erofs_super_block *)&fileData[SZ_1KB];
    erofs->magic = EROFS_SUPER_MAGIC;
    erofs->blocks = 1;
    ExtheaderV1 *ext = (ExtheaderV1 *)&fileData[SZ_4KB];
    ext->magicNumber = EXTHDR_MAGIC;
    ext->partSize = 4096;
    fwrite(fileData, 1, sizeof(fileData), fp);
    fclose(fp);

    void *buf = malloc(1);
    int ret = ops->read_partition(ops, "module_update", -1, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_OK);
    free(buf);
    remove(path);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_006, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    uint64_t outNumRead;
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char fileData[SZ_4KB + sizeof(ExtheaderV1)] = {0};
    ext4_super_block *ext4 = (ext4_super_block *)&fileData[SZ_1KB];
    ext4->s_magic = EXT4_SUPER_MAGIC;
    ext4->s_blocks_count_lo = 1;
    ExtheaderV1 *ext = (ExtheaderV1 *)&fileData[SZ_4KB];
    ext->magicNumber = EXTHDR_MAGIC;
    ext->partSize = 4096;
    fwrite(fileData, 1, sizeof(fileData), fp);
    fclose(fp);

    void *buf = malloc(1);
    int ret = ops->read_partition(ops, "module_update", -1, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_OK);
    free(buf);
    remove(path);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_005, TestSize.Level0)
{
    int ret = FsHvbInit((InitHvbType)99);
    EXPECT_NE(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_006, TestSize.Level0)
{
    char longDigest[257];
    memset_s(longDigest, sizeof(longDigest), '0', 256);
    longDigest[256] = '\0';
    char cmdLine[300];
    snprintf_s(cmdLine, sizeof(cmdLine), sizeof(cmdLine) - 1, "ohos.boot.hvb.digest=%s ", longDigest);
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    const char *hashCmd = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, hashCmd);

    int ret = FsHvbInit(MAIN_HVB);
    EXPECT_NE(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_002, TestSize.Level0)
{
    FstabItem fsItem;
    char testStr[10] = "testStr";
    char testDev[] = "/dev/block/platform/xxx/by-name/system";
    fsItem.deviceName = (char *)malloc(sizeof(testDev) + 1);
    memcpy_s(fsItem.deviceName, sizeof(testDev) + 1, &testDev[0], sizeof(testDev));
    fsItem.mountPoint = &testStr[0];
    fsItem.fsType = &testStr[0];
    fsItem.mountOptions = &testStr[0];
    fsItem.fsManagerFlags = 1;
    fsItem.next = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    const char *cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    FsHvbInit(MAIN_HVB);
    int ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_NE(ret, 0);

    FsHvbFinal(MAIN_HVB);
    remove(BOOT_CMD_LINE);
    free(fsItem.deviceName);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_006, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "test";
    struct hvb_cert cert = {};
    uint8_t bigDigest[200] = {0};
    uint8_t bigSalt[200] = {0};

    cert.image_len = 512;
    cert.data_block_size = 8;
    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 0;
    cert.hash_payload.digest = bigDigest;
    cert.digest_size = 200;
    cert.hash_payload.salt = bigSalt;
    cert.salt_size = 200;
    cert.fec_size = 0;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);
    FsHvbDestoryVerityTarget(&target);
}

HWTEST_F(FsHvbUnitTest, Init_VerifyExtHvbImage_003, TestSize.Level0)
{
    const char *devPath = "/dev/block/loop0";
    char *outPath = nullptr;
    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("FSDM_VALUE", "CreateFail", 1);
    const char *cmdLine = "ohos.boot.hvb.ext_rvt=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    int ret = VerifyExtHvbImage(devPath, "boot", &outPath);
    EXPECT_NE(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("FSDM_VALUE");
    free(outPath);
}

HWTEST_F(FsHvbUnitTest, Init_VerifyExtHvbImage_004, TestSize.Level0)
{
    const char *devPath = "/dev/block/loop0";
    char *outPath = nullptr;
    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("FSDM_VALUE", "InitFail", 1);
    const char *cmdLine = "ohos.boot.hvb.ext_rvt=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    int ret = VerifyExtHvbImage(devPath, "boot", &outPath);
    EXPECT_NE(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("FSDM_VALUE");
    free(outPath);
}

HWTEST_F(FsHvbUnitTest, Init_VerifyExtHvbImage_005, TestSize.Level0)
{
    char longDevPath[FS_HVB_MAX_PATH_LEN + 10];
    memset_s(longDevPath, sizeof(longDevPath), 'a', sizeof(longDevPath) - 1);
    longDevPath[sizeof(longDevPath) - 1] = '\0';
    char *outPath = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    const char *cmdLine = "ohos.boot.hvb.ext_rvt=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    int ret = VerifyExtHvbImage(longDevPath, "boot", &outPath);
    EXPECT_NE(ret, 0);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    free(outPath);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetExtheaderSize_003, TestSize.Level0)
{
    uint64_t imageSize = 0;
    FILE *fp = tmpfile();
    ASSERT_NE(fp, nullptr);
    char small = 'x';
    fwrite(&small, 1, 1, fp);
    fflush(fp);
    int fd = fileno(fp);
    bool ret = CheckAndGetExtheaderSize(fd, 0, &imageSize, "test");
    EXPECT_FALSE(ret);
    fclose(fp);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_003, TestSize.Level0)
{
    FstabItem fsItem;
    char testStr[10] = "testStr";
    char testDev[] = "/dev/block/platform/xxx/by-name/boot";
    fsItem.deviceName = (char *)malloc(sizeof(testDev) + 1);
    memcpy_s(fsItem.deviceName, sizeof(testDev) + 1, &testDev[0], sizeof(testDev));
    fsItem.mountPoint = &testStr[0];
    fsItem.fsType = &testStr[0];
    fsItem.mountOptions = &testStr[0];
    fsItem.fsManagerFlags = 1;
    fsItem.next = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("CERT_VALUE", "Fail", 1);
    const char *cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    FsHvbInit(MAIN_HVB);
    int ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_NE(ret, 0);

    FsHvbFinal(MAIN_HVB);
    remove(BOOT_CMD_LINE);
    free(fsItem.deviceName);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("CERT_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_008, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char small[100] = {0};
    fwrite(small, 1, sizeof(small), fp);
    fclose(fp);

    int ret = ops->read_partition(ops, "module_update", -1, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);

    remove(path);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_009, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char small[50] = {0};
    fwrite(small, 1, sizeof(small), fp);
    fclose(fp);

    void *buf = malloc(100);
    uint64_t outNumRead;
    int ret = ops->read_partition(ops, "module_update", 0, 100, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    free(buf);
    remove(path);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_012, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    int ret = ops->read_partition(ops, nullptr, 0, 1, nullptr, nullptr);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_013, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    const char *path = "/data/init_ut/libfs_hvb/module.img";
    CheckAndCreateDir(path);
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    char fileData[SZ_4KB + sizeof(ExtheaderV1)] = {0};
    struct erofs_super_block *erofs = (struct erofs_super_block *)&fileData[SZ_1KB];
    erofs->magic = EROFS_SUPER_MAGIC;
    erofs->blocks = 1;
    ExtheaderV1 *ext = (ExtheaderV1 *)&fileData[SZ_4KB];
    ext->magicNumber = 0;
    ext->partSize = 4096;
    fwrite(fileData, 1, sizeof(fileData), fp);
    fclose(fp);

    void *buf = malloc(1);
    uint64_t outNumRead;
    int ret = ops->read_partition(ops, "module_update", -1, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_OK);
    free(buf);
    remove(path);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetExt4Size_002, TestSize.Level0)
{
    uint64_t imageSize;
    bool ret = CheckAndGetExt4Size(nullptr, &imageSize, "boot");
    EXPECT_FALSE(ret);
    ret = CheckAndGetExt4Size("x", nullptr, "boot");
    EXPECT_FALSE(ret);
    ret = CheckAndGetExt4Size("x", &imageSize, nullptr);
    EXPECT_FALSE(ret);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetErofsSize_002, TestSize.Level0)
{
    uint64_t imageSize;
    bool ret = CheckAndGetErofsSize(nullptr, &imageSize, "boot");
    EXPECT_FALSE(ret);
    ret = CheckAndGetErofsSize("x", nullptr, "boot");
    EXPECT_FALSE(ret);
    ret = CheckAndGetErofsSize("x", &imageSize, nullptr);
    EXPECT_FALSE(ret);
}

HWTEST_F(FsHvbUnitTest, Init_CheckAndGetExtheaderSize_004, TestSize.Level0)
{
    uint64_t imageSize = 0;
    bool ret = CheckAndGetExtheaderSize(0, 0, &imageSize, nullptr);
    EXPECT_FALSE(ret);
    ret = CheckAndGetExtheaderSize(-1, 0, &imageSize, "test");
    EXPECT_FALSE(ret);
    ret = CheckAndGetExtheaderSize(0, 0, nullptr, "test");
    EXPECT_FALSE(ret);
    int pipefd[2];
    if (pipe(pipefd) == 0) {
        ret = CheckAndGetExtheaderSize(pipefd[0], 0, &imageSize, "test");
        EXPECT_FALSE(ret);
        close(pipefd[0]);
        close(pipefd[1]);
    }
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_007, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "test";
    struct hvb_cert cert = {};
    uint8_t digest = 53;
    uint8_t salt = 55;

    cert.image_len = 512;
    cert.data_block_size = 0;
    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 0;
    cert.hash_payload.digest = &digest;
    cert.digest_size = 1;
    cert.hash_payload.salt = &salt;
    cert.salt_size = 1;
    cert.fec_size = 1;
    cert.fec_num_roots = 1;
    cert.fec_offset = 8;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);
    FsHvbDestoryVerityTarget(&target);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_010, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    setenv("BOOTSLOTS_VALUE", "2", 1);
    setenv("CURRENTSLOT_VALUE", "1", 1);
    int ret = ops->read_partition(ops, "nonexist", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_OOM);
    unsetenv("BOOTSLOTS_VALUE");
    unsetenv("CURRENTSLOT_VALUE");
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_011, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    setenv("BOOTSLOTS_VALUE", "2", 1);
    setenv("CURRENTSLOT_VALUE", "0", 1);
    int ret = ops->read_partition(ops, "nonexist", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_OOM);
    unsetenv("BOOTSLOTS_VALUE");
    unsetenv("CURRENTSLOT_VALUE");
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_014, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    setenv("BOOTSLOTS_VALUE", "2", 1);
    setenv("CURRENTSLOT_VALUE", "999", 1);
    int ret = ops->read_partition(ops, "nonexist", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_OOM);
    unsetenv("BOOTSLOTS_VALUE");
    unsetenv("CURRENTSLOT_VALUE");
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_004, TestSize.Level0)
{
    FstabItem fsItem;
    char testStr[10] = "testStr";
    char testDev[] = "/dev/block/platform/xxx/by-name/boot_a";
    fsItem.deviceName = (char *)malloc(sizeof(testDev) + 1);
    memcpy_s(fsItem.deviceName, sizeof(testDev) + 1, &testDev[0], sizeof(testDev));
    fsItem.mountPoint = &testStr[0];
    fsItem.fsType = &testStr[0];
    fsItem.mountOptions = &testStr[0];
    fsItem.fsManagerFlags = 1;
    fsItem.next = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("FSDM_VALUE", "AllSucceed", 1);
    setenv("BOOTSLOTS_VALUE", "2", 1);
    const char *cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    FsHvbInit(MAIN_HVB);
    int ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_EQ(ret, 0);

    FsHvbFinal(MAIN_HVB);
    remove(BOOT_CMD_LINE);
    free(fsItem.deviceName);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("FSDM_VALUE");
    unsetenv("BOOTSLOTS_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_005, TestSize.Level0)
{
    FstabItem fsItem;
    char testStr[10] = "testStr";
    char testDev[] = "/dev/block/platform/xxx/by-name/boot_b";
    fsItem.deviceName = (char *)malloc(sizeof(testDev) + 1);
    memcpy_s(fsItem.deviceName, sizeof(testDev) + 1, &testDev[0], sizeof(testDev));
    fsItem.mountPoint = &testStr[0];
    fsItem.fsType = &testStr[0];
    fsItem.mountOptions = &testStr[0];
    fsItem.fsManagerFlags = 1;
    fsItem.next = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("FSDM_VALUE", "AllSucceed", 1);
    setenv("BOOTSLOTS_VALUE", "2", 1);
    const char *cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    FsHvbInit(MAIN_HVB);
    int ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_EQ(ret, 0);

    FsHvbFinal(MAIN_HVB);
    remove(BOOT_CMD_LINE);
    free(fsItem.deviceName);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("FSDM_VALUE");
    unsetenv("BOOTSLOTS_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_006, TestSize.Level0)
{
    FstabItem fsItem;
    char testStr[10] = "testStr";
    char testDev[] = "/dev/block/platform/xxx/by-name/ab";
    fsItem.deviceName = (char *)malloc(sizeof(testDev) + 1);
    memcpy_s(fsItem.deviceName, sizeof(testDev) + 1, &testDev[0], sizeof(testDev));
    fsItem.mountPoint = &testStr[0];
    fsItem.fsType = &testStr[0];
    fsItem.mountOptions = &testStr[0];
    fsItem.fsManagerFlags = 1;
    fsItem.next = nullptr;

    setenv("VERIFY_VALUE", "Succeed", 1);
    setenv("HASH_VALUE", "AllSucceed", 1);
    setenv("BOOTSLOTS_VALUE", "2", 1);
    const char *cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=01 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);

    FsHvbInit(MAIN_HVB);
    int ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_NE(ret, 0);

    FsHvbFinal(MAIN_HVB);
    remove(BOOT_CMD_LINE);
    free(fsItem.deviceName);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
    unsetenv("BOOTSLOTS_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_015, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    int ret = ops->read_partition(ops, "patch_test", 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_OOM);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_016, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    int ret = ops->read_partition(ops, "nonexist", 0, 1, buf, nullptr);
    EXPECT_EQ(ret, HVB_IO_ERROR_IO);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_HvbReadFromPartition_017, TestSize.Level0)
{
    struct hvb_ops *ops = FsHvbGetOps();
    void *buf = malloc(1);
    uint64_t outNumRead;
    char longName[257];
    memset_s(longName, sizeof(longName), 'a', 256);
    longName[256] = '\0';
    int ret = ops->read_partition(ops, longName, 0, 1, buf, &outNumRead);
    EXPECT_EQ(ret, HVB_IO_ERROR_OOM);
    free(buf);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_008, TestSize.Level0)
{
    DmVerityTarget target = {};
    const char *devName = "test";
    struct hvb_cert cert = {};
    uint8_t digest = 53;
    uint8_t salt = 55;

    cert.image_len = 512;
    cert.data_block_size = 8;
    cert.hash_block_size = 0;
    cert.hashtree_offset = 16;
    cert.hash_algo = 0;
    cert.hash_payload.digest = &digest;
    cert.digest_size = 1;
    cert.hash_payload.salt = &salt;
    cert.salt_size = 1;
    cert.fec_size = 1;
    cert.fec_num_roots = 1;
    cert.fec_offset = 8;

    FsHvbDestoryVerityTarget(&target);
    int ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);
    FsHvbDestoryVerityTarget(&target);
}
}