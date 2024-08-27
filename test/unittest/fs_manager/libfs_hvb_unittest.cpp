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
    int ret = FsHvbInit();
    EXPECT_EQ(ret, HVB_ERROR_INVALID_ARGUMENT);

    setenv("VERIFY_VALUE", "PartFail", 1);
    ret = FsHvbInit();
    EXPECT_EQ(ret, HVB_ERROR_UNSUPPORTED_VERSION);

    setenv("VERIFY_VALUE", "Succeed", 1);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);

    cmdLine = "ohos.boot.hvb.hash_algo=test "; // HVB_CMDLINE_HASH_ALG
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);

    cmdLine = "ohos.boot.hvb.digest=1 "; // HVB_CMDLINE_CERT_DIGEST
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);
    remove(BOOT_CMD_LINE);

    cmdLine = "ohos.boot.hvb.hash_algo=test ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=1234 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);
    remove(BOOT_CMD_LINE);

    setenv("HASH_VALUE", "InitFail", 1);
    cmdLine = "ohos.boot.hvb.hash_algo=sha256 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    cmdLine = "ohos.boot.hvb.digest=12 ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "UpdateFail", 1);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "FinalFail", 1);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "AllSucceed", 1);
    ret = FsHvbInit();
    EXPECT_EQ(ret, -1);

    remove(BOOT_CMD_LINE);
    unsetenv("VERIFY_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_001, TestSize.Level0)
{
    FstabItem fsItem;

    int ret = FsHvbSetupHashtree(nullptr);
    EXPECT_EQ(ret, -1);

    ret = FsHvbSetupHashtree(&fsItem);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbFinal_001, TestSize.Level0)
{
    int ret = FsHvbFinal();
    EXPECT_EQ(ret, 0);
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
    cert.hash_algo = 2;
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, -1);

    cert.hash_algo = 0;
    cert.hash_payload.digest = (uint8_t *)"5";
    cert.digest_size = 1;
    cert.hash_payload.salt = (uint8_t *)"7";
    cert.salt_size = 1;
    cert.fec_size = 1;
    cert.fec_num_roots = 1;
    cert.fec_offset = 8;
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);

    cert.hash_algo = 1;
    ret = FsHvbConstructVerityTarget(&target, devName, &cert);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbDestoryVerityTarget_001, TestSize.Level0)
{
    DmVerityTarget target;

    target.paras = (char *)calloc(1, 10);
    FsHvbDestoryVerityTarget(&target);
}
}