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
#include "init_firststage_test.h"
#include <gtest/gtest.h>

#include <sys/stat.h>

using namespace testing;
using namespace testing::ext;
static int g_access = 0;


#ifdef __cplusplus
    extern "C" {
#endif
int ExecvStub(const char *pathname, char *const argv[])
{
    printf("do execv %s \n", pathname);
    return 0;
}

int CreateTestFile(const char *fileName, const char *data)
{
    FILE *tmpFile = fopen(fileName, "wr");
    if (tmpFile != nullptr) {
        fprintf(tmpFile, "%s", data);
        (void)fflush(tmpFile);
        fclose(tmpFile);
    }
    return tmpFile != nullptr;
}

int AccessStub(const char *pathname, int mode)
{
    printf("access %s \n", pathname);
    return g_access;
}

void CloseStdioStub(void)
{
    printf("close stdio \n");
}

#ifdef __cplusplus
    }
#endif

namespace OHOS {
class InitFirststageTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void InitFirststageTest::SetUpTestCase()
{
    GTEST_LOG_(INFO) << "InitFirststageTest SetUpTestCase";
}

void InitFirststageTest::TearDownTestCase()
{
    GTEST_LOG_(INFO) << "InitFirststageTest TearDownTestCase";
}

void InitFirststageTest::SetUp()
{
    GTEST_LOG_(INFO) << "InitFirststageTest SetUp";
}

void InitFirststageTest::TearDown()
{
    GTEST_LOG_(INFO) << "InitFirststageTest TearDown";
}

HWTEST_F(InitFirststageTest, AsanTest_001, TestSize.Level0)
{
    int ret = CreateTestFile("/log/asanTest", "test");
    StartSecondStageInit(0);
    g_access = -1;
    StartSecondStageInit(0);
    EXPECT_TRUE(ret);
}

}