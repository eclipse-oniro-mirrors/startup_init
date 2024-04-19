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

#include <gtest/gtest.h>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <unistd.h>
#include "le_loop.h"
#include "loop_event.h"
#include "le_timer.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class LoopTimerUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

static LoopHandle g_loop = NULL;
int32_t g_maxCount = 0;
static void Test_ProcessTimer(const TimerHandle taskHandle, void *context)
{
    g_maxCount--;
    if (g_maxCount <= 0) {
         LE_StopLoop(g_loop);
    }
    printf("WaitTimeout count %d\n", g_maxCount);
}

static void TimeoutCancel(const TimerHandle taskHandle, void *context)
{
    printf("TimeoutCancel count %d", g_maxCount);
    LE_StopTimer(LE_GetDefaultLoop(), taskHandle);
}

HWTEST_F(LoopTimerUnitTest, Init_Timer_001, TestSize.Level0)
{
    EXPECT_EQ(LE_CreateLoop(&g_loop), 0);

    TimerHandle timer = NULL;
    int ret = LE_CreateTimer(NULL, &timer, Test_ProcessTimer, NULL);
    EXPECT_NE(ret, 0);

    ret = LE_CreateTimer(g_loop, NULL, Test_ProcessTimer, NULL);
    EXPECT_NE(ret, 0);

    ret = LE_CreateTimer(g_loop,  &timer, NULL, NULL);
    EXPECT_NE(ret, 0);

    CancelTimer(timer);

    uint64_t time = GetCurrentTimespec(0);
    EXPECT_GT(time, 0);

    time = GetMinTimeoutPeriod(NULL);
    EXPECT_EQ(time, 0);

    EventLoop *loop = reinterpret_cast<EventLoop *>(g_loop);
    DestroyTimerList(loop);
    printf("Init_Timer_001 %d end", g_maxCount);
}

HWTEST_F(LoopTimerUnitTest, Init_Timer_002, TestSize.Level0)
{
    EXPECT_EQ(LE_CreateLoop(&g_loop), 0);
    EventLoop *loop = reinterpret_cast<EventLoop *>(g_loop);

    TimerHandle timer = NULL;
    g_maxCount = 1;
    int ret = LE_CreateTimer(g_loop, &timer, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer, 200, 1);
    EXPECT_EQ(ret, 0);
    usleep(200000);
    CheckTimeoutOfTimer(loop, GetCurrentTimespec(0));
    EXPECT_EQ(g_maxCount, 0);
    LE_CloseLoop(g_loop);

    printf("Init_Timer_002 %d end", g_maxCount);
}

HWTEST_F(LoopTimerUnitTest, Init_Timer_003, TestSize.Level0)
{
    EXPECT_EQ(LE_CreateLoop(&g_loop), 0);

    TimerHandle timer = NULL;
    int ret = LE_CreateTimer(g_loop, &timer, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer, 200, 2);
    EXPECT_EQ(ret, 0);
    g_maxCount = 2;
    LE_RunLoop(g_loop);
    EXPECT_EQ(g_maxCount, 0);
    LE_CloseLoop(g_loop);

    printf("Init_Timer_003 %d end", g_maxCount);
}

HWTEST_F(LoopTimerUnitTest, Init_Timer_004, TestSize.Level0)
{
    EXPECT_EQ(LE_CreateLoop(&g_loop), 0);

    g_maxCount = 3;
    TimerHandle timer = NULL;
    int ret = LE_CreateTimer(g_loop, &timer, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer, 100, 1);
    EXPECT_EQ(ret, 0);

    TimerHandle timer1 = NULL;
    ret = LE_CreateTimer(g_loop, &timer1, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer1, 150, 1);
    EXPECT_EQ(ret, 0);

    TimerHandle timer2 = NULL;
    ret = LE_CreateTimer(g_loop, &timer2, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer2, 300, 1);
    EXPECT_EQ(ret, 0);

    usleep(150);
    LE_RunLoop(g_loop);
    EXPECT_EQ(g_maxCount, 0);
    LE_CloseLoop(g_loop);

    printf("Init_Timer_004 %d end", g_maxCount);
}

HWTEST_F(LoopTimerUnitTest, Init_Timer_005, TestSize.Level0)
{
    EXPECT_EQ(LE_CreateLoop(&g_loop), 0);

    g_maxCount = 3;
    TimerHandle timer = NULL;
    int ret = LE_CreateTimer(g_loop, &timer, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer, 100, 2);
    EXPECT_EQ(ret, 0);

    TimerHandle timer1 = NULL;
    ret = LE_CreateTimer(g_loop, &timer1, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer1, 150, 2);
    EXPECT_EQ(ret, 0);

    TimerHandle timer2 = NULL;
    ret = LE_CreateTimer(g_loop, &timer2, Test_ProcessTimer, NULL);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer2, 300, 1);
    EXPECT_EQ(ret, 0);

    CancelTimer(timer);
    LE_RunLoop(g_loop);
    EXPECT_EQ(g_maxCount, 0);
    LE_CloseLoop(g_loop);

    printf("Init_Timer_005 %d end", g_maxCount);
}
}