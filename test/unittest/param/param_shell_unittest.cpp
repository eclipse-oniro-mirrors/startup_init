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
#include <gtest/gtest.h>

#include "begetctl.h"
#include "securec.h"
#include "shell.h"
#include "shell_utils.h"
#include "shell_bas.h"
#include "init_param.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class ParamShellUnitTest : public testing::Test {
public:
    ParamShellUnitTest() {};
    virtual ~ParamShellUnitTest() {};
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
    void TestBody(void) {};
    void TestInitParamShell()
    {
        SystemSetParameter("aaa", "aaa");
        BShellHandle bshd = GetShellHandle();
        if (bshd == nullptr) {
            return;
        }
        const char *args[] = {"paramshell", "\n"};
        const ParamInfo *param = BShellEnvGetReservedParam(bshd, PARAM_REVERESD_NAME_CURR_PARAMETER);
        int ret = BShellEnvSetParam(bshd, PARAM_REVERESD_NAME_CURR_PARAMETER, "..a", PARAM_STRING, (void *)"..a");
        EXPECT_EQ(ret, 0);
        SetParamShellPrompt(bshd, args[1]);
        SetParamShellPrompt(bshd, "..");

        ret = BShellEnvSetParam(bshd, param->name, param->desc, param->type, (void *)"");
        SetParamShellPrompt(bshd, "..");

        SetParamShellPrompt(bshd, ".a");
        SetParamShellPrompt(bshd, ".");
        SetParamShellPrompt(bshd, args[1]);
        BShellParamCmdRegister(bshd, 1);
        BShellEnvStart(bshd);
        ret = BShellEnvOutputPrompt(bshd, "testprompt");
        ret = BShellEnvOutputPrompt(bshd, "testprompt1111111111111111111111111111111111111111111111111111111111");
        BShellEnvOutputByte(bshd, 'o');
        EXPECT_EQ(ret, 0);
    }
    void TestParamShellCmd()
    {
        BShellHandle bshd = GetShellHandle();
        BShellKey *key = BShellEnvGetDefaultKey('\n');
        EXPECT_NE(key, nullptr);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "cd const") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        int ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "cat aaa") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "testnotcmd") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        // test param start with "
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "\"ls") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        // test argc is 0
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), ",ls") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "$test$") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "exit") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
    }
    void TestParamShellCmd1()
    {
        BShellHandle bshd = GetShellHandle();
        BShellKey *key = BShellEnvGetDefaultKey('\n');
        EXPECT_NE(key, nullptr);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "pwd") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        int ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "help") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "dump") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "dump verbose") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\n');
        EXPECT_EQ(ret, 0);
    }
    void TestParamShellcmdEndkey()
    {
        BShellHandle bshd = GetShellHandle();
        bshd->input(nullptr, 0);
        BShellKey *key = BShellEnvGetDefaultKey('\b');
        EXPECT_NE(key, nullptr);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "testbbackspace") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        bshd->cursor = strlen("testb");
        int ret = key->keyHandle(bshd, '\b');
        EXPECT_EQ(ret, 0);
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "testbbackspace") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        bshd->cursor = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\b');
        EXPECT_EQ(ret, 0);

        key = BShellEnvGetDefaultKey('\t');
        if (strcpy_s(bshd->buffer, sizeof(bshd->buffer), "testtab") != EOK) {
            return;
        }
        bshd->length = strlen(bshd->buffer);
        ret = key->keyHandle(bshd, '\t');
        EXPECT_NE(key, nullptr);
        BShellEnvProcessInput(bshd, (char)3); // 3 is ctrl c
        BShellEnvProcessInput(bshd, '\e');
        BShellEnvProcessInput(bshd, '[');
        bshd->length = 1;
        bshd->cursor = 1;
        BShellEnvProcessInput(bshd, 'C');
        BShellEnvProcessInput(bshd, 'D');
    }
};

HWTEST_F(ParamShellUnitTest, TestInitParamShell, TestSize.Level1)
{
    ParamShellUnitTest test;
    test.TestInitParamShell();
    test.TestParamShellCmd();
    test.TestParamShellCmd1();
}
HWTEST_F(ParamShellUnitTest, TestParamShellInput, TestSize.Level1)
{
    BShellHandle bshd = GetShellHandle();
    BShellEnvProcessInput(bshd, '\n');

    BShellEnvProcessInput(bshd, 'l');
    bshd->length = BSH_COMMAND_MAX_LENGTH;

    BShellEnvProcessInput(bshd, 'l');
    bshd->length = sizeof('l');
    bshd->cursor = 0;
    BShellEnvProcessInput(bshd, 's');
    BShellEnvProcessInput(bshd, '\n');
 
    BShellEnvProcessInput(bshd, '\n'); // test bshd buff length is 0

    int ret = BShellEnvRegisterKeyHandle(bshd, 'z', (BShellkeyHandle)(void*)0x409600); // 0x409600 construct address
    EXPECT_EQ(ret, 0);
}
HWTEST_F(ParamShellUnitTest, TestParamShellcmd2, TestSize.Level1)
{
    ParamShellUnitTest test;
    test.TestParamShellcmdEndkey();
    GetSystemCommitId();
    BShellEnvLoop(nullptr);
    BShellEnvErrString(GetShellHandle(), 1);
    BShellEnvOutputResult(GetShellHandle(), 1);
    demoExit();
}
}  // namespace init_ut
