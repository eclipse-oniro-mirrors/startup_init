/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cinttypes>
#include <thread>
#include <mutex>
#include <vector>
#include <gtest/gtest.h>
#include "hookmgr.h"
using namespace testing::ext;
using namespace std;

namespace initModuleTest {
#define STAGE_TEST_ONE 0
int g_publicData = 0;
mutex g_mutex;
HOOK_MGR *g_hookMgr = nullptr;
class HookMgrModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

struct HookExecCtx {
    int result;
    int retErr;
};

void HookExecFunc(int n)
{
    for (int i = 0; i < n; ++i) {
        HookMgrExecute(g_hookMgr, STAGE_TEST_ONE, nullptr, nullptr);
    }
}

static int OhosTestHookMultiThread(const HOOK_INFO *hookInfo, void *executionContext)
{
    lock_guard<std::mutex> lg(g_mutex);
    g_publicData++;
    return 0;
}

static int OhosTestHookMultiThreadAnother(const HOOK_INFO *hookInfo, void *executionContext)
{
    lock_guard<std::mutex> lg(g_mutex);
    g_publicData++;
    return -1;
}

static void OhosHookPrint(const HOOK_INFO *hookInfo, void *traversalCookie)
{
    printf("\tstage[%02d] prio[%02d].\n", hookInfo->stage, hookInfo->prio);
}

static void DumpAllHooks(HOOK_MGR *hookMgr)
{
    printf("----------All Hooks---------------\n");
    HookMgrTraversal(hookMgr, NULL, OhosHookPrint);
    printf("----------------------------------\n\n");
}

HWTEST_F(HookMgrModuleTest, HookMgrModuleTest, TestSize.Level1)
{
    int ret;
    int cnt;
    g_hookMgr = HookMgrCreate("moduleTestHook");
    ASSERT_NE(g_hookMgr, nullptr);

    ret = HookMgrAdd(g_hookMgr, STAGE_TEST_ONE, 0, OhosTestHookMultiThread);
    EXPECT_EQ(ret, 0);
    cnt = HookMgrGetHooksCnt(g_hookMgr, STAGE_TEST_ONE);
    EXPECT_EQ(cnt, 1);

    HOOK_INFO info;
    info.stage = STAGE_TEST_ONE;
    info.prio = 1;
    info.hook = OhosTestHookMultiThreadAnother;
    info.hookCookie = nullptr;
    HookMgrAddEx(g_hookMgr, &info);
    cnt = HookMgrGetHooksCnt(g_hookMgr, STAGE_TEST_ONE);
    EXPECT_EQ(cnt, 2);
    cnt = HookMgrGetStagesCnt(g_hookMgr);
    EXPECT_EQ(cnt, 1);
    DumpAllHooks(g_hookMgr);

    // test multiThread HookMgrExecute
    std::vector<std::thread> threads;
    for (int i = 1; i <= 10; ++i) {
        threads.push_back(std::thread(HookExecFunc, 10));
    }
    for (auto & th : threads) {
        th.join();
    }
    EXPECT_EQ(g_publicData, 200);

    HookMgrAdd(g_hookMgr, STAGE_TEST_ONE, 1, OhosTestHookMultiThreadAnother);
    cnt = HookMgrGetHooksCnt(g_hookMgr, STAGE_TEST_ONE);
    EXPECT_EQ(cnt, 2);

    HookMgrDel(g_hookMgr, STAGE_TEST_ONE, OhosTestHookMultiThreadAnother);
    cnt = HookMgrGetHooksCnt(g_hookMgr, STAGE_TEST_ONE);
    EXPECT_EQ(cnt, 1);
    HookMgrDestroy(g_hookMgr);
}

} // namespace init_ut
