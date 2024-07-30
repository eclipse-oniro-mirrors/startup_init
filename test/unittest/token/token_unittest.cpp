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

using namespace std;
using namespace testing::ext;

namespace init_ut {
class TokenUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(TokenUnitTest, Init_HalReadToken_001, TestSize.Level0)
{
    char *token;
    unsigned int len;
    int ret = HalReadToken(token, len);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TokenUnitTest, Init_HalWriteToken_001, TestSize.Level0)
{
    const char *token;
    unsigned int len;
    int ret = HalWriteToken(token, len);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TokenUnitTest, Init_HalGetAcKey_001, TestSize.Level0)
{
    char *acKey;
    unsigned int len;
    int ret = HalGetAcKey(acKey, len);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TokenUnitTest, Init_HalGetProdId_001, TestSize.Level0)
{
    char *productId;
    unsigned int len;
    int ret = HalGetProdId(productId, len);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TokenUnitTest, Init_FsHvbGetValueFromCmdLine_001, TestSize.Level0)
{
    char *productKey;
    unsigned int len;
    int ret = HalGetProdKey(productKey, len);
    EXPECT_EQ(ret, 0);
}
}
