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
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <unistd.h>

#include "service_watcher.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {

static void ParameterChangeTest(const char *key, const char *value, void *context)
{
    printf("ParameterChangeTest key:%s %s \n", key, value);
}

static void WatcherCallBackTest(const char *key, const ServiceInfo *status)
{
    printf("WatcherCallBackTest key:%s %d", key, status->status);
}

class ServiceWatcherUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
}

static void ServiceStatusChange(const char *key, const ServiceInfo *status)
{
    std::cout <<"service Name is: " << key;
    std::cout <<", ServiceStatus is: "<< status->status;
    std::cout <<", pid is: "<< status->pid << std::endl;
}

HWTEST_F(ServiceWatcherUnitTest, ServiceWatchForStatus_001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "ServiceWatchForStatus_001 start";
    string serviceName = "test.Service";
    int ret = ServiceWatchForStatus(serviceName.c_str(), ServiceStatusChange);
    EXPECT_EQ(ret, 0);
    auto status = GetServiceStatus(serviceName);
    EXPECT_TRUE(status == "idle");
    GTEST_LOG_(INFO) << "ServiceWatchForStatus_001 end";
}
}