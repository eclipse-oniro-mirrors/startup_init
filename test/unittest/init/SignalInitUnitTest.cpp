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

static void ProcessSignal(const struct signalfd_siginfo *siginfo)
{
    return;
}

namespace init_ut {
class SignalInitUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(SignalInitUnitTest, SignalInitTestRmSig, TestSize.Level1)
{
    LE_CreateSignalTask(LE_GetDefaultLoop(), &g_sigHandler, ProcessSignal);
    ASSERT_NE(g_sigHandler, nullptr);
    int ret = LE_AddSignal(LE_GetDefaultLoop(), g_sigHandler, SIGUSR1);
    ASSERT_EQ(ret, 0);
    LE_AddSignal(LE_GetDefaultLoop(), g_sigHandler, SIGUSR1);
    LE_AddSignal(LE_GetDefaultLoop(), g_sigHandler, SIGUSR2);
    ret = LE_RemoveSignal(LE_GetDefaultLoop(), g_sigHandler, SIGUSR1);
    ASSERT_EQ(ret, 0);
    LE_RemoveSignal(LE_GetDefaultLoop(), g_sigHandler, SIGUSR1);
    LE_RemoveSignal(LE_GetDefaultLoop(), g_sigHandler, SIGUSR2);
    ((BaseTask *)g_sigHandler)->handleEvent(LE_GetDefaultLoop(), (TaskHandle)&g_sigHandler, Event_Write);
    LE_CloseSignalTask(LE_GetDefaultLoop(), g_sigHandler);
    ASSERT_EQ(ret, 0);
}
}
