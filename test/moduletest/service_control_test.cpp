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
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <thread>
#include <gtest/gtest.h>
#include <csignal>

#include "service_control.h"
#include "securec.h"
#include "syspara/parameter.h"
#include "sys_param.h"
#include "init_param.h"
#include "beget_ext.h"
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
    uint64_t margin = 60; // 60 ms margin in case of timer not that precisely
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
    BEGET_ERROR_CHECK(ret == -1, return, "Get media_service status failed.");

    serviceName = "non-exist-service";
    std::cout << "Wait for service " << serviceName << " status change to stop\n";
    ret = ServiceWaitForStatus(serviceName.c_str(), SERVICE_STOPPED, WAIT_SERVICE_STATUS_TIMEOUT);
    EXPECT_EQ(ret, -1);
}

namespace {
constexpr int PROC_STAT_STATE_OFFSET = 2;
constexpr int POLL_INTERVAL_MS = 100;

bool IsLiveServiceProcess(uint32_t pid)
{
    if (pid <= 1 || pid > static_cast<uint32_t>(std::numeric_limits<pid_t>::max())) {
        return false;
    }
    std::ifstream statFile("/proc/" + std::to_string(pid) + "/stat");
    std::string statLine;
    if (!std::getline(statFile, statLine)) {
        return false;
    }
    // comm may contain spaces or ')': the state follows its final closing bracket.
    size_t end = statLine.rfind(')');
    if (end == std::string::npos || end + PROC_STAT_STATE_OFFSET >= statLine.size()) {
        return false;
    }
    char state = statLine[end + PROC_STAT_STATE_OFFSET];
    return state != 'Z' && state != 'X' && state != 'x' && kill(static_cast<pid_t>(pid), 0) == 0;
}

uint32_t WaitForNewServiceProcess(const std::string &name, uint32_t oldPid)
{
    const std::string pidKey = std::string(STARTUP_SERVICE_CTL) + "." + name + ".pid";
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(WAIT_SERVICE_STATUS_TIMEOUT);
    uint32_t previous = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        uint32_t pid = GetUintParameter(pidKey.c_str(), 0);
        if (pid != oldPid && GetServiceStatus(name) == "running" && IsLiveServiceProcess(pid)) {
            if (pid == previous) {
                return pid; // Two consecutive samples of a new, non-zombie instance.
            }
            previous = pid;
        } else {
            previous = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
    return 0;
}

class OnDemandRecoveryCleanup {
public:
    explicit OnDemandRecoveryCleanup(const std::string &name) : name_(name) {}
    ~OnDemandRecoveryCleanup()
    {
        (void)ServiceControl(name_.c_str(), STOP);
    }

private:
    std::string name_;
};
} // namespace

// Opt-in only: use an isolated, non-critical ondemand test service. This is an
// interface recovery loop, not a high-load or samgr/SA availability test.
HWTEST_F(ServiceControlTest, DISABLED_OnDemandRecovery, TestSize.Level1)
{
    const char *configuredName = std::getenv("INIT_ONDEMAND_TEST_SERVICE");
    ASSERT_NE(configuredName, nullptr) << "Set an isolated ondemand test service explicitly";
    const std::string serviceName(configuredName);
    ASSERT_EQ(serviceName.find("init_ondemand_test_"), 0U);
    ASSERT_EQ(serviceName.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"), std::string::npos);
    OnDemandRecoveryCleanup cleanup(serviceName);
    constexpr int totalCycles = 20;
    constexpr int minSuccess = 19;
    int successCount = 0;

    for (int i = 0; i < totalCycles; ++i) {
        SCOPED_TRACE(i);
        // Ordinary START avoids pre-arming a recovery before the simulated crash.
        ASSERT_EQ(ServiceControl(serviceName.c_str(), START), 0);
        uint32_t oldPid = WaitForNewServiceProcess(serviceName, 0);
        ASSERT_GT(oldPid, 1U) << "No live test instance before crash";
        ASSERT_EQ(kill(static_cast<pid_t>(oldPid), SIGKILL), 0) << "kill failed, errno=" << errno;
        int ret = ServiceControl(serviceName.c_str(), START_ONDEMAND);
        if (ret == 0 && WaitForNewServiceProcess(serviceName, oldPid) != 0) {
            ++successCount;
        } else {
            std::cout << "Cycle " << i << ": no new live instance, ret=" << ret << std::endl;
        }
    }
    std::cout << "Interface recovery: " << successCount << "/" << totalCycles << std::endl;
    EXPECT_GE(successCount, minSuccess);
}
} // initModuleTest
