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
#include "param_stub.h"
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "param_message.h"
#include "init_param.h"
#include "param_utils.h"
#include "parcel.h"
#include "securec.h"
#include "system_ability_definition.h"
#include "string_ex.h"
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

    void OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value) override
    {
        printf("TestWatcher::OnParameterChange name %s %s \n", name.c_str(), value.c_str());
    }
};

using WatcherManagerPtr = WatcherManager *;
class WatcherProxyUnitTest : public ::testing::Test {
public:
    WatcherProxyUnitTest() {}
    virtual ~WatcherProxyUnitTest() {}

    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

    int TestAddRemoteWatcher(uint32_t agentId, uint32_t &watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;

        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        sptr<IWatcher> watcher = new TestWatcher();
        bool ret = data.WriteRemoteObject(watcher->AsObject());
        WATCHER_CHECK(ret, return 0, "Can not get remote");
        data.WriteUint32(agentId);
        watcherManager->OnRemoteRequest(IWatcherManager::ADD_REMOTE_AGENT, data, reply, option);
        watcherId = reply.ReadUint32();
        EXPECT_NE(watcherId, 0);

        auto remoteWatcher = watcherManager->GetRemoteWatcher(watcherId);
        if (remoteWatcher != nullptr) {
            EXPECT_EQ(remoteWatcher->GetAgentId(), agentId);
        } else {
            EXPECT_EQ(0, agentId);
        }
        return 0;
    }

    int TestDelRemoteWatcher(uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;

        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteUint32(watcherId);
        watcherManager->OnRemoteRequest(IWatcherManager::DEL_REMOTE_AGENT, data, reply, option);
        EXPECT_EQ(reply.ReadInt32(), 0);
        EXPECT_EQ(watcherManager->GetRemoteWatcher(watcherId) == nullptr, 1);
        return 0;
    }

    int TestAddWatcher(const std::string &keyPrefix, uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;

        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteString(keyPrefix);
        data.WriteUint32(watcherId);
        watcherManager->OnRemoteRequest(IWatcherManager::ADD_WATCHER, data, reply, option);
        int ret = reply.ReadInt32();
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(watcherManager->GetWatcherGroup(keyPrefix) != nullptr, 1);
        return 0;
    }

    int TestDelWatcher(const std::string &keyPrefix, uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteString(keyPrefix);
        data.WriteUint32(watcherId);
        watcherManager->OnRemoteRequest(IWatcherManager::DEL_WATCHER, data, reply, option);
        EXPECT_EQ(reply.ReadInt32(), 0);
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
        ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data());
        WATCHER_CHECK(msg != nullptr, return -1, "Invalid msg");
        msg->type = MSG_NOTIFY_PARAM;
        msg->msgSize = msgSize;
        msg->id.watcherId = watcherId;
        int ret = memcpy_s(msg->key, sizeof(msg->key), name.c_str(), name.size());
        WATCHER_CHECK(ret == 0, return -1, "Failed to fill value");
        uint32_t offset = 0;
        ret = FillParamMsgContent(msg, &offset, PARAM_VALUE, value.c_str(), value.size());
        WATCHER_CHECK(ret == 0, return -1, "Failed to fill value");
        watcherManager->ProcessWatcherMessage(msg);
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
            watcher->OnParameterChange(name, name, value);
            delete watcher;
        }
        return 0;
    }

    int TestWatchAgentDump(const std::string &keyPrefix)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        // dump watcher
        std::vector<std::u16string> args = {};
        watcherManager->Dump(STDOUT_FILENO, args);
        // dump parameter
        args.push_back(Str8ToStr16("-h"));
        watcherManager->Dump(STDOUT_FILENO, args);
        args.clear();
        args.push_back(Str8ToStr16("-k"));
        args.push_back(Str8ToStr16(keyPrefix.c_str()));
        watcherManager->Dump(STDOUT_FILENO, args);
        return 0;
    }

    int TestWatchAgentDied(uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get manager");
        auto remoteWatcher = watcherManager->GetRemoteWatcher(watcherId);
        WATCHER_CHECK(remoteWatcher != nullptr, return -1, "Failed to get remote watcher");
        if (watcherManager->GetDeathRecipient() != nullptr) {
            watcherManager->GetDeathRecipient()->OnRemoteDied(remoteWatcher->GetWatcher()->AsObject());
        }
        EXPECT_EQ(watcherManager->GetRemoteWatcher(watcherId) == nullptr, 1);
        return 0;
    }

    int TestInvalid(const std::string &keyPrefix)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteString(keyPrefix);
        watcherManager->OnRemoteRequest(IWatcherManager::REFRESH_WATCHER + 1, data, reply, option);

        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteString(keyPrefix);
        watcherManager->OnRemoteRequest(IWatcherManager::REFRESH_WATCHER + 1, data, reply, option);
        return 0;
    }

    int TestStop()
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get manager");
        watcherManager->OnStop();
        return 0;
    }

    WatcherManagerPtr GetWatcherManager()
    {
        static WatcherManagerPtr watcherManager = nullptr;
        if (watcherManager == nullptr) {
            watcherManager = new WatcherManager(0, true);
            if (watcherManager == nullptr) {
                return nullptr;
            }
            watcherManager->OnStart();
        }
        return watcherManager;
    }
};

HWTEST_F(WatcherProxyUnitTest, TestAddWatcher, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1000, watcherId); // 1000 test agent
    test.TestAddWatcher("test.permission.watcher.test1", watcherId);
    test.TestProcessWatcherMessage("test.permission.watcher.test1", watcherId);
    test.TestWatchAgentDump("test.permission.watcher.test1");
}

HWTEST_F(WatcherProxyUnitTest, TestAddWatcher2, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1001, watcherId); // 1001 test agent
    test.TestAddWatcher("test.permission.watcher.test2", watcherId);
    test.TestAddWatcher("test.permission.watcher.test2", watcherId);
    test.TestAddWatcher("test.permission.watcher.test2", watcherId);
    test.TestWatchAgentDump("test.permission.watcher.test2");
}

HWTEST_F(WatcherProxyUnitTest, TestAddWatcher3, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1003, watcherId); // 1003 test agent
    test.TestAddWatcher("test.permission.watcher.test3", watcherId);
    test.TestWatchAgentDump("test.permission.watcher.test3");
}

HWTEST_F(WatcherProxyUnitTest, TestAddWatcher4, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1004, watcherId); // 1004 test agent
    SystemSetParameter("test.watcher.test4", "1101");
    SystemSetParameter("test.watcher.test4.test", "1102");
    test.TestAddWatcher("test.watcher.test4*", watcherId);
    test.TestWatchAgentDump("test.watcher.test4*");
}

HWTEST_F(WatcherProxyUnitTest, TestAddWatcher5, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1005, watcherId); // 1005 test agent
    test.TestAddWatcher("test.permission.watcher.test5", watcherId);
    SystemSetParameter("test.permission.watcher.test5", "1101");
    test.TestWatchAgentDump("test.permission.watcher.test5");
}

HWTEST_F(WatcherProxyUnitTest, TestDelWatcher, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1006, watcherId); // 1005 test agent
    test.TestAddWatcher("test.permission.watcher.testDel", watcherId);
    test.TestDelWatcher("test.permission.watcher.testDel", watcherId);
    test.TestDelRemoteWatcher(watcherId);
    test.TestWatchAgentDump("test.permission.watcher.testDel");
}

HWTEST_F(WatcherProxyUnitTest, TestDiedWatcher, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    uint32_t watcherId = 0;
    test.TestAddRemoteWatcher(1006, watcherId); // 1005 test agent
    test.TestAddWatcher("test.permission.watcher.testdied", watcherId);
    test.TestDelWatcher("test.permission.watcher.testdied", watcherId);
    test.TestWatchAgentDied(watcherId);
    test.TestWatchAgentDump("test.permission.watcher.testdied");
}

HWTEST_F(WatcherProxyUnitTest, TestWatchProxy, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    test.TestWatchProxy("test.permission.watcher.test1", "watcherId");
}

HWTEST_F(WatcherProxyUnitTest, TestInvalid, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    test.TestInvalid("test.permission.watcher.test1");
}

HWTEST_F(WatcherProxyUnitTest, TestStop, TestSize.Level0)
{
    WatcherProxyUnitTest test;
    test.TestStop();
}