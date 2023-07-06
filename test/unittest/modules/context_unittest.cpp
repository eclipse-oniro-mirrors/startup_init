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

extern "C" {
struct ForkArgs {
    int (*childFunc)(const SubInitForkArg *arg);
    SubInitForkArg args;
};
static pid_t g_pid = 1000;
static pthread_t g_thread = 0;

static void *ThreadFunc(void *arg)
{
    printf("Create thread %d \n", gettid());
    struct ForkArgs *forkArg = static_cast<struct ForkArgs *>(arg);
    forkArg->childFunc(&forkArg->args);
    printf("Exit thread %d %d \n", forkArg->args.type, gettid());
    free(forkArg);
    g_thread = 0;
    return nullptr;
}

pid_t SubInitFork(int (*childFunc)(const SubInitForkArg *arg), const SubInitForkArg *args)
{
    if (g_pid >= 0) {
        struct ForkArgs *forkArg = static_cast<struct ForkArgs *>(malloc(sizeof(struct ForkArgs)));
        if (forkArg == nullptr) {
            return -1;
        }
        forkArg->childFunc = childFunc;
        forkArg->args.socket[0] = args->socket[0];
        forkArg->args.socket[1] = args->socket[1];
        forkArg->args.type = args->type;
        int ret = pthread_create(&g_thread, nullptr, ThreadFunc, forkArg);
        if (ret != 0) {
            printf("Failed to create thread %d \n", errno);
            return -1;
        }
        usleep(100); // 100 wait
    }
    g_pid++;
    return g_pid;
}
}

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
    g_pid = -3; // -3  test data
    int ret = StartSubInit(INIT_CONTEXT_CHIPSET);
    EXPECT_NE(ret, 0);
    ret = StartSubInit(INIT_CONTEXT_MAIN);
    EXPECT_NE(ret, 0);
    g_pid = 100; // 100 test data
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
    ret = ExecuteCmdInSubInit(&context, "mkdir", nullptr);
    EXPECT_EQ(ret, 0);
    context.type = INIT_CONTEXT_MAIN;
    ret = ExecuteCmdInSubInit(&context, "mkdir", STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_NE(ret, 0);

    // fail
    ret = ExecuteCmdInSubInit(nullptr, "mkdir", STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_EQ(ret, -1);
    ret = ExecuteCmdInSubInit(&context, nullptr, STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_EQ(ret, -1);
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_03, TestSize.Level1)
{
    int ret = StartSubInit(INIT_CONTEXT_CHIPSET);
    EXPECT_EQ(ret, 0);
    if (g_thread != 0) {
        pthread_join(g_thread, nullptr);
        g_thread = 0;
    }
    SubInitInfo *subInfo = GetSubInitInfo(INIT_CONTEXT_CHIPSET);
    if (subInfo == nullptr) {
        EXPECT_EQ(1, 0);
    } else {
        EXPECT_EQ(2, subInfo->state);
        StopSubInit(subInfo->subPid);
    }
    subInfo = GetSubInitInfo(INIT_CONTEXT_MAIN);
    if (subInfo != nullptr) {
        EXPECT_EQ(1, 0);
    }
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_04, TestSize.Level1)
{
    int ret = StartSubInit(INIT_CONTEXT_CHIPSET);
    EXPECT_EQ(ret, 0);
    if (g_thread != 0) {
        pthread_join(g_thread, nullptr);
        g_thread = 0;
    }
    SubInitInfo *subInfo = GetSubInitInfo(INIT_CONTEXT_CHIPSET);
    if (subInfo != nullptr) {
        EXPECT_EQ(2, subInfo->state);
        StopSubInit(subInfo->subPid);
    } else {
        EXPECT_EQ(1, 0);
    }
    // close
    subInfo = GetSubInitInfo(INIT_CONTEXT_CHIPSET);
    if (subInfo != nullptr) {
        EXPECT_EQ(0, subInfo->state);
    }

    SubInitContext *subContext = GetSubInitContext(INIT_CONTEXT_CHIPSET);
    if (subContext == nullptr) {
        EXPECT_EQ(0, -1);
        return;
    }
    ret = subContext->executeCmdInSubInit(INIT_CONTEXT_CHIPSET, "mkdir-2", STARTUP_INIT_UT_PATH"/testsubcontext");
    EXPECT_NE(ret, 0);
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

HWTEST_F(InitContextUnitTest, InitSubContextTest_09, TestSize.Level1)
{
    int ret = SetSubInitContext(nullptr, "serviceName");
    EXPECT_EQ(ret, -1);
    ConfigContext context = { INIT_CONTEXT_MAIN };
    ret = SetSubInitContext(&context, "serviceName");
    EXPECT_EQ(ret, 0);
    context = { INIT_CONTEXT_CHIPSET };
    ret = SetSubInitContext(&context, "serviceName");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(InitContextUnitTest, InitSubContextTest_10, TestSize.Level1)
{
    int ret = InitSubInitContext(INIT_CONTEXT_MAIN, nullptr);
    EXPECT_EQ(ret, -1);
    ret = InitSubInitContext(INIT_CONTEXT_CHIPSET, nullptr);
    EXPECT_EQ(ret, -1);

    SubInitContext *subContext = GetSubInitContext(INIT_CONTEXT_CHIPSET);
    if (subContext == nullptr) {
        EXPECT_EQ(0, -1);
        return;
    }
    ret = subContext->startSubInit(INIT_CONTEXT_MAIN);
    EXPECT_NE(ret, 0);
    ret = subContext->executeCmdInSubInit(INIT_CONTEXT_CHIPSET, nullptr, nullptr);
    EXPECT_NE(ret, 0);
    ret = subContext->executeCmdInSubInit(INIT_CONTEXT_MAIN, nullptr, nullptr);
    EXPECT_NE(ret, 0);
}
} // namespace init_ut
