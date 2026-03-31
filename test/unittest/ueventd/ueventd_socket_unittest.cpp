/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <fcntl.h>

#include "param_stub.h"
#include "ueventd.h"
#include "ueventd_socket.h"

using namespace testing::ext;

namespace UeventdUt {

class UeventdSocketUnitTest : public testing::Test {
public:
static void SetUpTestCase(void) {}
static void TearDownTestCase(void) {}
void TearDown() {}
};

HWTEST_F(UeventdSocketUnitTest, Init_UeventdSocketTest_UeventdSocketInit001, TestSize.Level1)
{
    int sockFd = UeventdSocketInit();
    EXPECT_GE(sockFd, 0);
    if (sockFd >= 0) {
        close(sockFd);
    }
}

HWTEST_F(UeventdSocketUnitTest, Init_UeventdSocketTest_ReadUeventMessage001, TestSize.Level1)
{
    int sockFd = UeventdSocketInit();
    ASSERT_GE(sockFd, 0);
    
    char buffer[2048] = {0};
    ssize_t ret = ReadUeventMessage(sockFd, buffer, sizeof(buffer));
    EXPECT_GE(ret, -1);
    
    close(sockFd);
}

HWTEST_F(UeventdSocketUnitTest, Init_UeventdSocketTest_ReadUeventMessage002, TestSize.Level1)
{
    char buffer[2048] = {0};
    ssize_t ret = ReadUeventMessage(-1, buffer, sizeof(buffer));
    EXPECT_EQ(ret, -1);
}

HWTEST_F(UeventdSocketUnitTest, Init_UeventdSocketTest_ReadUeventMessage003, TestSize.Level1)
{
    int sockFd = UeventdSocketInit();
    ASSERT_GE(sockFd, 0);
    
    ssize_t ret = ReadUeventMessage(sockFd, nullptr, 2048);
    EXPECT_EQ(ret, -1);
    
    close(sockFd);
}

} // namespace UeventdUt