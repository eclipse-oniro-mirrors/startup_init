/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "init_context.h"

#include "param_stub.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class InitContextUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(InitContextUnitTest, InitSubContextTest_01, TestSize.Level1)
{
    int ret = StartSubInit(INIT_CONTEXT_CHIPSET);
    EXPECT_EQ(ret, 0);
    ret = StartSubInit(INIT_CONTEXT_MAIN);
    EXPECT_NE(ret, 0);
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_02, TestSize.Level1)
{
    ConfigContext context = { INIT_CONTEXT_CHIPSET };
    int ret = ExecuteCmdInSubInit(&context, "mkdir", STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_EQ(ret, 0);
    ret = ExecuteCmdInSubInit(&context, "mkdir", STARTUP_INIT_UT_PATH"/testsubcontext1");
    EXPECT_EQ(ret, 0);
    ret = ExecuteCmdInSubInit(&context, "mkdir", STARTUP_INIT_UT_PATH"/testsubcontext2");
    EXPECT_EQ(ret, 0);
    context.type = INIT_CONTEXT_MAIN;
    ret = ExecuteCmdInSubInit(&context, "mkdir", STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_NE(ret, 0);
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_03, TestSize.Level1)
{
    int ret = StartSubInit(INIT_CONTEXT_CHIPSET);
    EXPECT_EQ(ret, 0);
    SubInitInfo *subInfo = GetSubInitInfo(INIT_CONTEXT_CHIPSET);
    if (subInfo == NULL) {
        EXPECT_EQ(1, 0);
    } else {
        EXPECT_EQ(2, subInfo->state);
        StopSubInit(subInfo->subPid);
    }
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_04, TestSize.Level1)
{
    int ret = StartSubInit(INIT_CONTEXT_CHIPSET);
    EXPECT_EQ(ret, 0);
    SubInitInfo *subInfo = GetSubInitInfo(INIT_CONTEXT_CHIPSET);
    if (subInfo != NULL) {
        EXPECT_EQ(2, subInfo->state);
        StopSubInit(subInfo->subPid);
    } else {
        EXPECT_EQ(1, 0);
    }
    // close
    subInfo = GetSubInitInfo(INIT_CONTEXT_CHIPSET);
    if (subInfo != NULL) {
        EXPECT_EQ(0, subInfo->state);
    }
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_05, TestSize.Level1)
{
    ConfigContext context = { INIT_CONTEXT_CHIPSET };
    int ret = ExecuteCmdInSubInit(&context, "mkdir-2", STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_06, TestSize.Level1)
{
    ConfigContext context = { INIT_CONTEXT_CHIPSET };
    int index = 0;
    const char *cmd = GetMatchCmd("mkdir ", &index);
    if (cmd == nullptr || strstr(cmd, "mkdir ") == nullptr) {
        EXPECT_EQ(1, 0);
        return;
    }
    DoCmdByIndex(index, STARTUP_INIT_UT_PATH"/testsubcontext", &context);
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_07, TestSize.Level1)
{
    ConfigContext context = { INIT_CONTEXT_MAIN };
    int index = 0;
    const char *cmd = GetMatchCmd("mkdir ", &index);
    if (cmd == nullptr || strstr(cmd, "mkdir ") == nullptr) {
        EXPECT_EQ(1, 0);
        return;
    }
    DoCmdByIndex(index, STARTUP_INIT_UT_PATH"/testsubcontext", &context);
}
} // namespace init_ut
