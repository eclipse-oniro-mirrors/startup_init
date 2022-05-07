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
#include "begetctl.h"
#include "param_stub.h"
#include "securec.h"
#include "shell.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class BegetctlUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        PrepareInitUnitTestEnv();
    };
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(BegetctlUnitTest, TestShellInit, TestSize.Level0)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param"
    };
    BShellEnvDirectExecute(GetShellHandle(), 1, (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellLs, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "ls"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellLsWithR, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "ls", "-r"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellLsGet, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "get"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellSet, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "set", "aaaaa", "1234567"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellGetWithKey, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "get", "aaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellWait, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait", "aaaaa"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}

HWTEST_F(BegetctlUnitTest, TestShellWaitWithKey, TestSize.Level1)
{
    BShellParamCmdRegister(GetShellHandle(), 0);
    const char *args[] = {
        "param", "wait", "aaaaa", "12*", "30"
    };
    BShellEnvDirectExecute(GetShellHandle(), sizeof(args) / sizeof(args[0]), (char **)args);
}
}  // namespace init_ut
