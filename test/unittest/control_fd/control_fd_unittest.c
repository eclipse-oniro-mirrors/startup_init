/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "control_fd.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class ControlfdUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ControlfdUnitTest, Init_CmdServiceInit_001, TestSize.Level0)
{
    char *socketPath = "dev/test";
    CallbackControlFdProcess callback;
    LoopHandle loopHandle;
    CmdServiceInit(socketPath, callback, loopHandle);
}

HWTEST_F(ControlfdUnitTest, Init_CmdClientInit_001, TestSize.Level1)
{
    char *socketPath = "dev/test";
    CallbackControlFdProcess callback;
    uint16_t type = 0;
    const char *cmd;
    CmdClientInit(socketPath, type, cmd, callback);
}

HWTEST_F(ControlfdUnitTest, Init_CmdServiceProcessDelClient_001, TestSize.Level1)
{
    pid_t pid = 0;
    CmdServiceProcessDelClient(pid);
}

HWTEST_F(ControlfdUnitTest, Init_CmdServiceProcessDestroyClient_001, TestSize.Level1)
{
    CmdServiceProcessDestroyClient();
}

HWTEST_F(ControlfdUnitTest, Init_InitPtyInterface_001, TestSize.Level1)
{
    CmdAgent *agent;
    uint16_t type;
    const char *cmd;
    CallbackSendMsgProcess callback;
    int ret = InitPtyInterface(agent, type, cmd, callback);
    EXPECT_EQ(ret, 0);
}
}  // namespace init_ut