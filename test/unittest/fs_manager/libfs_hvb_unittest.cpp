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
#include "fs_hvb.h"
#include "init_utils.h"
#include "fs_manager/ext4_super_block.h"
#include "fs_manager/erofs_super_block.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class FsHvbUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
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
    DmVerityTarget target;
    const char *devName = "test";
    struct hvb_cert cert;
    uint8_t digest = 53;
    uint8_t salt = 55;

    int ret = FsHvbConstructVerityTarget(nullptr, devName, &cert);
    EXPECT_EQ(ret, -1);
    ret = FsHvbConstructVerityTarget(&target, nullptr, &cert);
    EXPECT_EQ(ret, -1);
    ret = FsHvbConstructVerityTarget(&target, devName, nullptr);
    EXPECT_EQ(ret, -1);

    cert.image_len = 512;
    cert.data_block_size = 0;
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);
    cert.data_block_size = 8;
    cert.hash_block_size = 0;
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);

    cert.hash_block_size = 8;
    cert.hashtree_offset = 16;
    cert.hash_algo = 4;
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
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);

    cert.hash_algo = 2;
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbDestoryVerityTarget_001, TestSize.Level0)
{
    DmVerityTarget target;

    target.paras = static_cast<char *>(calloc(1, 10));
    EXPECT_NE(target.paras, nullptr);
    FsHvbDestoryVerityTarget(&target);
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
}