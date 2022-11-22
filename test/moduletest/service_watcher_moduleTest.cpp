/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <thread>
#include <string>
#include <gtest/gtest.h>
#include "service_control.h"
#include "service_watcher.h"
#include "test_utils.h"

using namespace testing::ext;

namespace initModuleTest {
class ServiceWatcherModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

static void ServiceStatusChange(const char *key, ServiceStatus status)
{
    std::cout<<"service Name is: "<<key<<", ServiceStatus is: "<<status<<std::endl;
}

HWTEST_F(ServiceWatcherModuleTest, serviceWatcher_test_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "serviceWatcher_test_001 start";
    string serviceName = "test.Service";
    int ret = ServiceWatchForStatus(serviceName.c_str(), ServiceStatusChange);
    EXPECT_EQ(ret, 0); // No matter if service exist or not, ServiceWatchForStatus always success.
    auto status = GetServiceStatus(serviceName);
    EXPECT_TRUE(status == "idle");
    GTEST_LOG_(INFO) << "serviceWatcher_test_001 end";
}

HWTEST_F(ServiceWatcherModuleTest, serviceWatcher_test_002, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "serviceWatcher_test_002 start";
    string serviceName = "media_service";
    auto status = GetServiceStatus(serviceName);
    if (status == "running") {
        int ret = ServiceControl(serviceName.c_str(), STOP);
        ASSERT_EQ(ret, 0);
    } else if (status != "created" && status != "stopped") {
        std::cout << serviceName << " in invalid status " << status << std::endl;
        ASSERT_TRUE(0);
    }
    int ret = ServiceWatchForStatus(serviceName.c_str(), ServiceStatusChange);
    EXPECT_EQ(ret, 0);
    status = GetServiceStatus(serviceName);
    EXPECT_TRUE(status == "stopped");
    GTEST_LOG_(INFO) << "serviceWatcher_test_002 end";
}
}
