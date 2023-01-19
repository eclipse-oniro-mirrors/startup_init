/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "if_system_ability_manager.h"
#include "init_param.h"
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "parameter.h"
#include "param_manager.h"
#include "param_stub.h"
#include "param_utils.h"
#include "system_ability_definition.h"
#include "watcher.h"
#include "watcher_manager_kits.h"
#include "service_watcher.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS;
using namespace OHOS::init_param;

static void TestParameterChange(const char *key, const char *value, void *context)
{
    printf("TestParameterChange key:%s %s", key, value);
}

static void TestWatcherCallBack(const char *key, ServiceStatus status)
{
    printf("TestWatcherCallBack key:%s %d", key, status);
}

class WatcherAgentUnitTest : public ::testing::Test {
public:
    WatcherAgentUnitTest() {}
    virtual ~WatcherAgentUnitTest() {}

    void SetUp()
    {
        if (GetParamSecurityLabel() != nullptr) {
            GetParamSecurityLabel()->cred.uid = 1000;  // 1000 test uid
            GetParamSecurityLabel()->cred.gid = 1000;  // 1000 test gid
        }
        SetTestPermissionResult(0);
    }
    void TearDown() {}
    void TestBody() {}

    int TestAddWatcher()
    {
        size_t index = 1;
        int ret = SystemWatchParameter("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test1*",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        // repeat add, return fail
        ret = SystemWatchParameter("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_NE(ret, 0);
        index++;
        ret = SystemWatchParameter("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        index++;
        ret = SystemWatchParameter("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);

        // delete
        ret = RemoveParameterWatcher("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        index--;
        ret = RemoveParameterWatcher("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        index--;
        ret = RemoveParameterWatcher("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        // has beed deleted
        ret = RemoveParameterWatcher("test.permission.watcher.test1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_NE(ret, 0);

        // 非法
        ret = SystemWatchParameter("test.permission.watcher.tes^^^^t1*", TestParameterChange, nullptr);
        EXPECT_NE(ret, 0);
        ret = SystemWatchParameter("test.permission.read.test1*", TestParameterChange, nullptr);
        EXPECT_EQ(ret, DAC_RESULT_FORBIDED);
        return 0;
    }

    int TestDelWatcher()
    {
        size_t index = 1;
        int ret = SystemWatchParameter("test.permission.watcher.test3.1",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test3.1*",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test3.2",
            TestParameterChange, reinterpret_cast<void *>(index));
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test3.1", nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test3.1*", nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test3.2", nullptr, nullptr);
        EXPECT_EQ(ret, 0);

        // 非法
        ret = SystemWatchParameter("test.permission.watcher.tes^^^^t1*", nullptr, nullptr);
        EXPECT_NE(ret, 0);
        ret = SystemWatchParameter("test.permission.read.test1*", nullptr, nullptr);
        EXPECT_EQ(ret, DAC_RESULT_FORBIDED);
        return 0;
    }

    int TestRecvMessage(const std::string &name)
    {
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteInterfaceToken(IWatcher::GetDescriptor());
        data.WriteString(name);
        data.WriteString(name);
        data.WriteString("watcherId");
        int ret = SystemWatchParameter(name.c_str(), TestParameterChange, nullptr);
        EXPECT_EQ(ret, 0);
        WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
        if (instance.remoteWatcher_ != nullptr) {
            instance.remoteWatcher_->OnRemoteRequest(IWatcher::PARAM_CHANGE, data, reply, option);
            instance.remoteWatcher_->OnRemoteRequest(IWatcher::PARAM_CHANGE + 1, data, reply, option);
            instance.remoteWatcher_->OnParameterChange(name.c_str(), "testname", "testvalue");
        }
        return 0;
    }

    int TestResetService()
    {
        sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        WATCHER_CHECK(samgr != nullptr, return -1, "Get samgr failed");
        sptr<IRemoteObject> object = samgr->GetSystemAbility(PARAM_WATCHER_DISTRIBUTED_SERVICE_ID);
        WATCHER_CHECK(object != nullptr, return -1, "Get watcher manager object from samgr failed");
        OHOS::init_param::WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
        if (instance.GetDeathRecipient() != nullptr) {
            instance.GetDeathRecipient()->OnRemoteDied(object);
        }
        return 0;
    }
};

HWTEST_F(WatcherAgentUnitTest, TestAddWatcher, TestSize.Level0)
{
    WatcherAgentUnitTest test;
    test.TestAddWatcher();
}

HWTEST_F(WatcherAgentUnitTest, TestRecvMessage, TestSize.Level0)
{
    WatcherAgentUnitTest test;
    test.TestRecvMessage("test.permission.watcher.agent.test1");
}

HWTEST_F(WatcherAgentUnitTest, TestDelWatcher, TestSize.Level0)
{
    WatcherAgentUnitTest test;
    test.TestDelWatcher();
}

HWTEST_F(WatcherAgentUnitTest, TestResetService, TestSize.Level0)
{
    WatcherAgentUnitTest test;
    test.TestResetService();
}

HWTEST_F(WatcherAgentUnitTest, TestWatcherService, TestSize.Level0)
{
    const char *errstr = "111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    ServiceWatchForStatus("param_watcher", TestWatcherCallBack);
    ServiceWaitForStatus("param_watcher", SERVICE_STARTED, 1);
    EXPECT_EQ(ServiceWatchForStatus(errstr, TestWatcherCallBack), -1);
    EXPECT_EQ(ServiceWatchForStatus(NULL, TestWatcherCallBack), -1);
    WatchParameter("testParam", nullptr, nullptr);
}