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

#include <gtest/gtest.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <asm/unistd.h>
#include <syscall.h>
#include <climits>
#include <sched.h>

#include "systemcapability.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class SyscapUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
}

HWTEST_F(SyscapUnitTest, HasSystemCapability_001, TestSize.Level1)
{
    bool ret = HasSystemCapability("test.cap");
    EXPECT_EQ(ret, false);
    ret = HasSystemCapability(nullptr);
    EXPECT_EQ(ret, false);
    ret = HasSystemCapability("ArkUI.ArkUI.Napi");
    EXPECT_EQ(ret, true);
    ret = HasSystemCapability("SystemCapability.ArkUI.ArkUI.Napi");
    EXPECT_EQ(ret, true);
    char *wrongName = reinterpret_cast<char *>(malloc(SYSCAP_MAX_SIZE));
    ASSERT_NE(wrongName, nullptr);
    EXPECT_EQ(memset_s(wrongName, SYSCAP_MAX_SIZE, 1, SYSCAP_MAX_SIZE), 0);
    HasSystemCapability(wrongName);
    free(wrongName);
}
}