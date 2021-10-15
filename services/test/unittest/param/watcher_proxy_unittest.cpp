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
#include "init_unittest.h"
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "param_message.h"
#include "param_request.h"
#include "param_utils.h"
#include "parcel.h"
#include "securec.h"
#include "system_ability_definition.h"
#include "watcher.h"
#include "watcher_manager.h"
#include "watcher_proxy.h"
#include "watcher_utils.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS;
using namespace OHOS::init_param;

class TestWatcher final : public Watcher {
public:
    TestWatcher() {}
    ~TestWatcher() = default;

    void OnParamerterChange(const std::string &name, const std::string &value) override
    {
        printf("TestWatcher::OnParamerterChange name %s %s \n", name.c_str(), value.c_str());
    }
};

using WatcherManagerPtr = WatcherManager *;
WatcherManagerPtr g_watcherManager{ nullptr };

class WatcherProxyTest : public ::testing::Test {
public:
    WatcherProxyTest() {}
    virtual ~WatcherProxyTest() {}

    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

    int TestAddWatcher(const std::string &keyPrefix, uint32_t &watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;

        data.WriteString(keyPrefix);
        sptr<IWatcher> watcher = new TestWatcher();
        // std::shared_ptr<TestWatcher> watcher = std::make_shared<TestWatcher>();
        bool ret = data.WriteRemoteObject(watcher->AsObject());
        WATCHER_CHECK(ret, return 0, "Can not get remote");
        watcherManager->OnRemoteRequest(IWatcherManager::ADD_WATCHER, data, reply, option);
        watcherId = reply.ReadUint32();
        EXPECT_NE(watcherId, 0);
        printf("TestAddWatcher %s watcherId %d %p \n", keyPrefix.c_str(), watcherId, watcherManager);

        EXPECT_EQ(watcherManager->GetWatcherGroup(1000) != NULL, 0); // test group id
        EXPECT_EQ(watcherManager->GetWatcherGroup("TestAddWatcher") != NULL, 0); // test key not exist
        return 0;
    }

    int TestDelWatcher(const std::string &keyPrefix, uint32_t &watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteString(keyPrefix);
        data.WriteUint32(watcherId);
        watcherManager->OnRemoteRequest(IWatcherManager::DEL_WATCHER, data, reply, option);
        EXPECT_EQ(reply.ReadInt32(), 0);
        watcherManager->OnStop();
        printf("TestDelWatcher %s watcherId %d %p \n", keyPrefix.c_str(), watcherId, watcherManager);
        return 0;
    }

    int TestProcessWatcherMessage(const std::string &name, uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        const std::string value("test.value");
        uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + value.size();
        msgSize = PARAM_ALIGN(msgSize); // align
        std::vector<char> buffer(msgSize, 0);
        ParamMessage *msg = (ParamMessage *)buffer.data();
        WATCHER_CHECK(msg != NULL, return -1, "Invalid msg");
        msg->type = MSG_NOTIFY_PARAM;
        msg->msgSize = msgSize;
        msg->id.watcherId = watcherId;
        int ret = memcpy_s(msg->key, sizeof(msg->key), name.c_str(), name.size());
        WATCHER_CHECK(ret == 0, return -1, "Failed to fill value");
        uint32_t offset = 0;
        ret = FillParamMsgContent(msg, &offset, PARAM_VALUE, value.c_str(), value.size());
        WATCHER_CHECK(ret == 0, return -1, "Failed to fill value");
        watcherManager->ProcessWatcherMessage(buffer, msgSize);
        return 0;
    }

    int TestWatchProxy(const std::string &name, const std::string &value)
    {
        sptr<ISystemAbilityManager> systemMgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        EXPECT_NE(systemMgr, nullptr);
        sptr<IRemoteObject> remoteObj = systemMgr->GetSystemAbility(PARAM_WATCHER_DISTRIBUTED_SERVICE_ID);
        EXPECT_NE(remoteObj, nullptr);
        WatcherProxy *watcher = new WatcherProxy(remoteObj);
        if (watcher != nullptr) {
            watcher->OnParamerterChange(name, value);
            delete watcher;
        }
        return 0;
    }

    WatcherManagerPtr GetWatcherManager()
    {
        if (g_watcherManager == nullptr) {
            g_watcherManager = new WatcherManager(0, true);
            if (g_watcherManager == nullptr) {
                return nullptr;
            }
            g_watcherManager->OnStart();
        }
        return g_watcherManager;
    }
};

HWTEST_F(WatcherProxyTest, TestAddWatcher, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    test.TestAddWatcher("test.permission.watcher.test1", watcherId);
    test.TestProcessWatcherMessage("test.permission.watcher.test1", watcherId);
}

HWTEST_F(WatcherProxyTest, TestAddWatcher2, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    test.TestAddWatcher("test.permission.watcher.test2", watcherId);
    test.TestAddWatcher("test.permission.watcher.test2", watcherId);
    test.TestAddWatcher("test.permission.watcher.test2", watcherId);
}

HWTEST_F(WatcherProxyTest, TestAddWatcher3, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    test.TestAddWatcher("test.permission.watcher.test3", watcherId);
}

HWTEST_F(WatcherProxyTest, TestAddWatcher4, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    SystemSetParameter("test.watcher.test4", "1101");
    SystemSetParameter("test.watcher.test4.test", "1102");
    test.TestAddWatcher("test.watcher.test4*", watcherId);
}

HWTEST_F(WatcherProxyTest, TestAddWatcher5, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    test.TestAddWatcher("test.permission.watcher.test5", watcherId);
    SystemSetParameter("test.permission.watcher.test5", "1101");
}

HWTEST_F(WatcherProxyTest, TestDelWatcher, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    test.TestAddWatcher("test.permission.watcher.testDel", watcherId);
    test.TestDelWatcher("test.permission.watcher.testDel", watcherId);
}

HWTEST_F(WatcherProxyTest, TestWatchProxy, TestSize.Level1)
{
    WatcherProxyTest test;
    test.TestWatchProxy("test.permission.watcher.test1", "watcherId");
}