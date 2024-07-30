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

#include "fs_hvb.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class HookUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(HookUnitTest, Init_HookMgrAdd_001, TestSize.Level0)
{
    HOOK_MGR *hookMgr;
    int stage = 0;
    int prio = 0;
    OhosHook hook;
    int ret = HookMgrAdd(hookMgr, stage, prio, hook);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(HookUnitTest, Init_HookMgrAddEx_001, TestSize.Level0)
{
    HOOK_MGR *hookMgr;
    const HOOK_INFO *hookInfo;
    int ret = HookMgrAddEx(hookMgr, hookInfo);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(HookUnitTest, Init_HookMgrDel_001, TestSize.Level0)
{
    HOOK_MGR *hookMgr;
    int stage;
    OhosHook hook;
    HookMgrDel(hookMgr, stage, hook);
}

HWTEST_F(HookUnitTest, Init_HookMgrExecute_001, TestSize.Level0)
{
    HOOK_MGR *hookMgr;
    int stage;
    void *executionContext;
    const HOOK_EXEC_OPTIONS *extraArgs;
    int ret = HookMgrExecute(hookMgr, stage, executionContext, extraArgs);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(HookUnitTest, Init_HookMgrTraversal_001, TestSize.Level0)
{
    HOOK_MGR *hookMgr;
    void *traversalCookie;
    OhosHookTraversal traversal;
    HookMgrTraversal(HOOK_MGR *hookMgr, void *traversalCookie, OhosHookTraversal traversal);
}
}