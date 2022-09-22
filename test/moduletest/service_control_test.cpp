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
#include <chrono>
#include <thread>
#include <gtest/gtest.h>

#include "service_control.h"
#include "test_utils.h"

using namespace testing::ext;
namespace initModuleTest {
namespace {
// Default wait for service status change time is 10 seconds
constexpr int WAIT_SERVICE_STATUS_TIMEOUT = 10;
}

class ServiceControlTest : public testing::Test {
public:
    static void SetUpTestCase() {};
    static void TearDownTestCase() {};
    void SetUp() {};
    void TearDown() {};
};

// Test service start
HWTEST_F(ServiceControlTest, ServiceStartTest, TestSize.Level1)
{
    // Pick an unusual service for testing
    // Try to start media_service.

    // 1) Check if media_service exist
    std::string serviceName = "media_service";
    auto status = GetServiceStatus(serviceName);
    if (status == "running") {
        int ret = ServiceControl(serviceName.c_str(), STOP);
        ASSERT_EQ(ret, 0);
        ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
        ASSERT_EQ(ret, 0);
    } else if (status != "created" && status != "stopped") {
        std::cout << serviceName << " in invalid status " << status << std::endl;
        std::cout << "Debug " << serviceName << " in unexpected status " << status << std::endl;
        ASSERT_TRUE(0);
    }

    // 2) Now try to start service
    int ret = ServiceControl(serviceName.c_str(), START);
    EXPECT_EQ(ret, 0);
    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STARTED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, 0);
    status = GetServiceStatus(serviceName);
    std::cout << "Debug " << serviceName << " in status " << status << std::endl;
    EXPECT_TRUE(status == "running");
}

HWTEST_F(ServiceControlTest, NonExistServiceStartTest, TestSize.Level1)
{
    std::string serviceName = "non_exist_service";
    int ret = ServiceControl(serviceName.c_str(), START);
    EXPECT_EQ(ret, 0); // No matter if service exist or not, ServiceControl always success.

    auto status = GetServiceStatus(serviceName);
    EXPECT_TRUE(status == "idle");
}

HWTEST_F(ServiceControlTest, ServiceStopTest, TestSize.Level1)
{
    std::string serviceName = "media_service";
    auto status = GetServiceStatus(serviceName);
    if (status == "stopped" || status == "created") {
        int ret = ServiceControl(serviceName.c_str(), START);
        ASSERT_EQ(ret, 0); // start must be success

    } else if (status != "running") {
        std::cout << serviceName << " in invalid status " << status << std::endl;
        ASSERT_TRUE(0);
    }

    int ret = ServiceControl(serviceName.c_str(), STOP);
    EXPECT_EQ(ret, 0);
    // Sleep for a while, let init handle service starting.
    const int64_t ms = 500;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    status = GetServiceStatus(serviceName);
    bool isStopped = status == "stopped";
    EXPECT_TRUE(isStopped);
}

HWTEST_F(ServiceControlTest, NonExistServiceStopTest, TestSize.Level1)
{
    std::string serviceName = "non_exist_service";
    int ret = ServiceControl(serviceName.c_str(), STOP);
    EXPECT_EQ(ret, 0); // No matter if service exist or not, ServiceControl always success.

    auto status = GetServiceStatus(serviceName);
    EXPECT_TRUE(status == "idle");
}

HWTEST_F(ServiceControlTest, ServiceTimerStartTest, TestSize.Level1)
{
    uint64_t timeout = 1000; // Start service in 1 second
    std::string serviceName = "media_service";
    // stop this service first
    int ret = ServiceControl(serviceName.c_str(), STOP);
    auto oldStatus = GetServiceStatus(serviceName);
    bool isRunning = oldStatus == "running";
    EXPECT_FALSE(isRunning);

    ret = StartServiceByTimer(serviceName.c_str(), timeout);
    EXPECT_EQ(ret, 0);

    // Service will be started in @timeout seconds
    // Now we try to sleep about @timeout / 2 seconds, then check service status.
    int64_t ms = 600;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    // Get service status.
    auto newStatus = GetServiceStatus(serviceName);
    bool notChange = oldStatus == newStatus;
    EXPECT_TRUE(notChange);

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    newStatus = GetServiceStatus(serviceName);

    isRunning = newStatus == "running";
    EXPECT_TRUE(isRunning);
}

HWTEST_F(ServiceControlTest, ServiceTimerStartContinuouslyTest, TestSize.Level1)
{
    uint64_t oldTimeout = 500;
    uint64_t newTimeout = 1000;
    std::string serviceName = "media_service";
    int ret = ServiceControl(serviceName.c_str(), STOP);
    EXPECT_EQ(ret, 0);
    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, 0);
    auto oldStatus = GetServiceStatus(serviceName);
    bool isRunning = oldStatus == "running";
    EXPECT_FALSE(isRunning);

    ret = StartServiceByTimer(serviceName.c_str(), oldTimeout); // Set timer as 500 ms
    EXPECT_EQ(ret, 0);
    ret = StartServiceByTimer(serviceName.c_str(), newTimeout); // Set timer as 1 second
    EXPECT_EQ(ret, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(oldTimeout)));
    auto newStatus = GetServiceStatus(serviceName);
    bool notChange = oldStatus == newStatus;
    EXPECT_TRUE(notChange);
    uint64_t margin = 20; // 20 ms margin in case of timer not that precisely
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(oldTimeout + margin)));
    newStatus = GetServiceStatus(serviceName);
    isRunning = newStatus == "running";
    EXPECT_TRUE(isRunning);
}

HWTEST_F(ServiceControlTest, ServiceTimerStopTest, TestSize.Level1)
{
    uint64_t timeout = 1000; // set timer as 1 second
    std::string serviceName = "media_service";
    int ret = ServiceControl(serviceName.c_str(), STOP);
    EXPECT_EQ(ret, 0);
    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, 0);
    auto oldStatus = GetServiceStatus(serviceName);
    bool isRunning = oldStatus == "running";
    EXPECT_FALSE(isRunning);

    ret = StartServiceByTimer(serviceName.c_str(), timeout);
    EXPECT_EQ(ret, 0);

    // Now sleep for a while
    int64_t ms = 300;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    auto newStatus = GetServiceStatus(serviceName);

    bool notChange = oldStatus == newStatus;
    EXPECT_TRUE(notChange);

    ret = StopServiceTimer(serviceName.c_str());
    EXPECT_EQ(ret, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));

    newStatus = GetServiceStatus(serviceName);
    notChange = oldStatus == newStatus;
    EXPECT_TRUE(notChange);
}

HWTEST_F(ServiceControlTest, ServiceTimerStopLateTest, TestSize.Level1)
{
    uint64_t timeout = 500; // set timer as 5 micro seconds
    std::string serviceName = "media_service";
    int ret = ServiceControl(serviceName.c_str(), STOP);
    auto oldStatus = GetServiceStatus(serviceName);
    bool isRunning = oldStatus == "running";
    EXPECT_FALSE(isRunning);

    ret = StartServiceByTimer(serviceName.c_str(), timeout);
    EXPECT_EQ(ret, 0);

    int64_t ms = 550;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    ret = StopServiceTimer(serviceName.c_str());
    EXPECT_EQ(ret, 0);

    auto newStatus = GetServiceStatus(serviceName);
    isRunning = newStatus == "running";
    EXPECT_TRUE(isRunning);
}

HWTEST_F(ServiceControlTest, RestartServiceTest, TestSize.Level1)
{
    std::string serviceName = "media_service";
    auto status = GetServiceStatus(serviceName);
    EXPECT_FALSE(status.empty());

    int ret = ServiceControl(serviceName.c_str(), RESTART);
    EXPECT_EQ(ret, 0);

    ret = ServiceControl(serviceName.c_str(), STOP);
    EXPECT_EQ(ret, 0);

    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, 0);

    ret = ServiceControl(serviceName.c_str(), RESTART);
    EXPECT_EQ(ret, 0);

    status = GetServiceStatus(serviceName);

    bool isRunning = status == "running";
    EXPECT_TRUE(isRunning);
}

HWTEST_F(ServiceControlTest, WaitForServiceStatusTest, TestSize.Level1)
{
    std::string serviceName = "media_service";
    int ret = ServiceControl(serviceName.c_str(), STOP);
    EXPECT_EQ(ret, 0);

    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, 0);

    auto status = GetServiceStatus(serviceName);
    bool isStopped = status == "stopped";
    EXPECT_TRUE(isStopped);

    // service is stopped now. try to wait a status which will not be set
    std::cout << "Wait for service " << serviceName << " status change to start\n";
    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STARTED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, -1);

    serviceName = "non-exist-service";
    std::cout << "Wait for service " << serviceName << " status change to stop\n";
    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, -1);
}
} // initModuleTest
