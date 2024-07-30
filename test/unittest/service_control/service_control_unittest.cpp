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

#include "service_control.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class ServiceControlUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
}

HWTEST_F(ServiceControlUnitTest, ServiceControlWithExtra_001, TestSize.Level1)
{
    int ret = ServiceControlWithExtra("deviceinfoservice", RESTART, argv, 1);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceControlWithExtra_002, TestSize.Level1)
{
    int ret = ServiceControlWithExtra(nullptr, RESTART, argv, 1);
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(ServiceControlUnitTest, ServiceControlWithExtra_003, TestSize.Level1)
{
    int ret = ServiceControlWithExtra(nullptr, 3, argv, 1); // 3 is action
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(ServiceControlUnitTest, ServiceControlWithExtra_004, TestSize.Level1)
{
    int ret = ServiceControlWithExtra("notservie", RESTART, argv, 1);
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(ServiceControlUnitTest, ServiceControlWithExtra_005, TestSize.Level1)
{
    int ret = ServiceControlWithExtra("deviceinfoservice", 3, argv, 1); // 3 is action
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceControl_001, TestSize.Level1)
{
    int ret = ServiceControl("deviceinfoservice", START);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceControl_002, TestSize.Level1)
{
    int ret = ServiceControl("deviceinfoservice", STOP);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceControl_003, TestSize.Level1)
{
    int ret = ServiceControl("deviceinfoservice", RESTART);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceControl_004, TestSize.Level1)
{
    int ret = ServiceControl(nullptr, RESTART);
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(ServiceControlUnitTest, ServiceWaitForStatus_001, TestSize.Level1)
{
    int ret = ServiceWaitForStatus("deviceinfoservice", SERVICE_READY, 1);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceWaitForStatus_002, TestSize.Level1)
{
    int ret = ServiceWaitForStatus("deviceinfoservice", SERVICE_READY, -1);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceWaitForStatus_003, TestSize.Level1)
{
    int ret = ServiceWaitForStatus(nullptr, SERVICE_READY, 1);
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(ServiceControlUnitTest, ServiceSetReady_001, TestSize.Level1)
{
    int ret = ServiceSetReady("deviceinfoservice");
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, ServiceSetReady_002, TestSize.Level1)
{
    int ret = ServiceSetReady(nullptr);
    EXPECT_EQ(ret, EC_FAILURE);
}
    
HWTEST_F(ServiceControlUnitTest, StartServiceByTimer_001, TestSize.Level1)
{
    int ret = StartServiceByTimer("deviceinfoservice", 1);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, StartServiceByTimer_002, TestSize.Level1)
{
    int ret = StartServiceByTimer("deviceinfoservice", 0);
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, StartServiceByTimer_003, TestSize.Level1)
{
    int ret = StartServiceByTimer(nullptr, 0);
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(ServiceControlUnitTest, StopServiceTimer_001, TestSize.Level1)
{
    int ret = StopServiceTimer("deviceinfoservice");
    EXPECT_EQ(ret, EC_SUCCESS);
}

HWTEST_F(ServiceControlUnitTest, StopServiceTimer_002, TestSize.Level1)
{
    int ret = StopServiceTimer(nullptr);
    EXPECT_EQ(ret, EC_FAILURE);
}
}