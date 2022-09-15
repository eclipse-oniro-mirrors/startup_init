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
#include <cstdio>
#include <gtest/gtest.h>
#include "modulemgr.h"

using namespace testing::ext;
using namespace std;

namespace initModuleTest {
class ModuleMgrModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

static void TestModuleDump(const MODULE_INFO *moduleInfo)
{
    printf("%s\n", moduleInfo->name);
}

HWTEST_F(ModuleMgrModuleTest, ModuleMrgTest, TestSize.Level1)
{
    int cnt;
    MODULE_MGR *moduleMgr = nullptr;
    moduleMgr = ModuleMgrCreate("init");

    // Install one module
    ModuleMgrInstall(moduleMgr, "bootchart", 0, NULL);
    ModuleMgrDestroy(moduleMgr);

    // Scan all modules
    moduleMgr = ModuleMgrScan("init");
    ASSERT_NE(moduleMgr, nullptr);

    ModuleMgrTraversal(moduleMgr, NULL, TestModuleDump);

    ModuleMgrUninstall(moduleMgr, NULL);
    cnt = ModuleMgrGetCnt(moduleMgr);
    EXPECT_EQ(cnt, 0);
    ModuleMgrGetArgs();
    ModuleMgrDestroy(moduleMgr);
}
}  // namespace init_ut
