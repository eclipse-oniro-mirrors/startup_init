/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <cstdint>
#include "loop_event.h"
extern "C" {
void LE_RunIdle(const LoopHandle loopHandle);
}

using namespace testing::ext;
using namespace std;

namespace init_ut {
class LoopIdleUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

static void Test_IdleOnce(const IdleHandle taskHandle, void *context)
{
    (void)taskHandle;
    int *count = reinterpret_cast<int *>(context);
    if (count != nullptr) {
        (*count)++;
    }
}

/* invalid parameters are rejected, null handles are tolerated and the loop stays usable */
HWTEST_F(LoopIdleUnitTest, LoopIdleTest_001, TestSize.Level1)
{
    LoopHandle loop = nullptr;
    ASSERT_EQ(LE_CreateLoop(&loop), 0);
    IdleHandle idle = nullptr;
    EXPECT_NE(LE_AddIdle(nullptr, &idle, Test_IdleOnce, nullptr, 0), 0);
    EXPECT_NE(LE_AddIdle(loop, &idle, nullptr, nullptr, 0), 0);
    EXPECT_NE(LE_DelayProc(nullptr, Test_IdleOnce, nullptr), 0);
    EXPECT_NE(LE_DelayProc(loop, nullptr, nullptr), 0);
    LE_RunIdle(nullptr);
    LE_DelIdle(nullptr);
    int count = 0;
    IdleHandle idle2 = nullptr;
    EXPECT_EQ(LE_AddIdle(loop, &idle2, Test_IdleOnce, &count, 0), 0);
    LE_RunIdle(loop);
    EXPECT_EQ(count, 1);
    LE_CloseLoop(loop);
}

/* once idle (repeat = 0): executed on first run, then removed automatically */
HWTEST_F(LoopIdleUnitTest, LoopIdleTest_002, TestSize.Level1)
{
    LoopHandle loop = nullptr;
    ASSERT_EQ(LE_CreateLoop(&loop), 0);
    int count = 0;
    IdleHandle idle = nullptr;
    EXPECT_EQ(LE_AddIdle(loop, &idle, Test_IdleOnce, &count, 0), 0);
    ASSERT_NE(idle, nullptr);
    LE_RunIdle(loop);
    EXPECT_EQ(count, 1);
   
    LE_RunIdle(loop);
    EXPECT_EQ(count, 1);
    LE_CloseLoop(loop);
}

/* repeated idle (repeat != 0): stays in list until deleted */
HWTEST_F(LoopIdleUnitTest, LoopIdleTest_003, TestSize.Level1)
{
    LoopHandle loop = nullptr;
    ASSERT_EQ(LE_CreateLoop(&loop), 0);
    int count = 0;
    IdleHandle idle = nullptr;
    EXPECT_EQ(LE_AddIdle(loop, &idle, Test_IdleOnce, &count, 1), 0);
    LE_RunIdle(loop);
    LE_RunIdle(loop);
    EXPECT_EQ(count, 2);
   
    ASSERT_NE(idle, nullptr);
    LE_DelIdle(idle);
    LE_RunIdle(loop);
    EXPECT_EQ(count, 2);
    LE_CloseLoop(loop);
}

/* LE_DelayProc runs the function once on the next idle loop */
HWTEST_F(LoopIdleUnitTest, LoopIdleTest_004, TestSize.Level1)
{
    LoopHandle loop = nullptr;
    ASSERT_EQ(LE_CreateLoop(&loop), 0);
    int count = 0;
    EXPECT_EQ(LE_DelayProc(loop, Test_IdleOnce, &count), 0);
    LE_RunIdle(loop);
    EXPECT_EQ(count, 1);
   
    LE_RunIdle(loop);
    EXPECT_EQ(count, 1);
    LE_CloseLoop(loop);
}
} // namespace init_ut
