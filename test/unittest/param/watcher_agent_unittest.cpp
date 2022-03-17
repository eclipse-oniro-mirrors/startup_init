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
#include <cstdio>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "param_utils.h"
#include "param_request.h"
#include "sys_param.h"
#include "system_ability_definition.h"
#include "watcher.h"
#include "watcher_manager_kits.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS;
using namespace OHOS::init_param;

void TestParameterChange(const char *key, const char *value, void *context)
{
    printf("TestParameterChange key:%s %s", key, value);
}

class WatcherAgentUnitTest : public ::testing::Test {
public:
    WatcherAgentUnitTest() {}
    virtual ~WatcherAgentUnitTest() {}

    void SetUp()
    {
        ParamWorkSpace *space = GetClientParamWorkSpace();
        if (space != nullptr && space->securityLabel != nullptr) {
            space->securityLabel->cred.uid = 1000; // 1000 test uid
            space->securityLabel->cred.gid = 1000; // 1000 test gid
        }
    }
    void TearDown() {}
    void TestBody() {}

    int TestAddWatcher()
    {
        int ret = SystemWatchParameter("test.permission.watcher.test1", TestParameterChange, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test1*", TestParameterChange, nullptr);
        EXPECT_EQ(ret, 0);
        // 非法
        ret = SystemWatchParameter("test.permission.watcher.tes^^^^t1*", TestParameterChange, nullptr);
        EXPECT_NE(ret, 0);
        // 被禁止
        ret = SystemWatchParameter("test.permission.read.test1*", TestParameterChange, nullptr);
        EXPECT_NE(ret, 0);
        return 0;
    }

    int TestDelWatcher()
    {
        int ret = SystemWatchParameter("test.permission.watcher.test1", nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test1*", nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test2", nullptr, nullptr);
        EXPECT_EQ(ret, 0);

        // 非法
        ret = SystemWatchParameter("test.permission.watcher.tes^^^^t1*", nullptr, nullptr);
        EXPECT_NE(ret, 0);
        // 被禁止
        ret = SystemWatchParameter("test.permission.read.test1*", nullptr, nullptr);
        EXPECT_NE(ret, 0);
        return 0;
    }

    int TestRecvMessage(const std::string &name)
    {
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteString(name);
        data.WriteString("watcherId");

        int ret = SystemWatchParameter(name.c_str(), TestParameterChange, nullptr);
        EXPECT_EQ(ret, 0);
        OHOS::init_param::WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
        OHOS::init_param::WatcherManagerKits::ParamWatcherKitPtr watcher = instance.GetParamWatcher(name);
        if (watcher != nullptr) {
            watcher->OnRemoteRequest(IWatcher::PARAM_CHANGE, data, reply, option);
            watcher->OnRemoteRequest(IWatcher::PARAM_CHANGE + 1, data, reply, option);
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