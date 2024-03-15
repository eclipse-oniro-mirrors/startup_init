/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <cerrno>
#include <cstdio>
#include "init_unittest.h"
#include "init.h"
#include "loop_event.h"
#include "le_epoll.h"
#include "le_signal.h"
#include "init_hashmap.h"

using namespace testing::ext;
using namespace std;

static SignalHandle g_sigHandler = nullptr;

static void TestProcessSignal(const struct signalfd_siginfo *siginfo)
{
    return;
}

namespace init_ut {
class LoopSignalUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(LoopSignalUnitTest, SignalInitTestRmSig, TestSize.Level1)
{
    static LoopHandle loopClient = nullptr;
    LE_STATUS status = LE_CreateLoop(&loopClient);
    EXPECT_EQ(status, 0);
    LE_CreateSignalTask(loopClient, &g_sigHandler, TestProcessSignal);
    ASSERT_NE(g_sigHandler, nullptr);
    int ret = LE_AddSignal(loopClient, g_sigHandler, SIGUSR1);
    ASSERT_EQ(ret, 0);
    LE_AddSignal(loopClient, g_sigHandler, SIGUSR1);
    LE_AddSignal(loopClient, g_sigHandler, SIGUSR2);
    ret = LE_RemoveSignal(loopClient, g_sigHandler, SIGUSR1);
    ASSERT_EQ(ret, 0);
    LE_RemoveSignal(loopClient, g_sigHandler, SIGUSR1);
    LE_RemoveSignal(loopClient, g_sigHandler, SIGUSR2);
    ((BaseTask *)g_sigHandler)->handleEvent(loopClient, (TaskHandle)&g_sigHandler, EVENT_WRITE);
    LE_CloseSignalTask(loopClient, g_sigHandler);
    ASSERT_EQ(ret, 0);
    LE_StopLoop(loopClient);
    LE_CloseLoop(loopClient);
    loopClient = nullptr;
}
}
