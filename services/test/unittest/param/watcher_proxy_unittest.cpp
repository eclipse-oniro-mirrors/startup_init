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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

#include "init_unittest.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "param_utils.h"
#include "parcel.h"
#include "securec.h"
#include "watcher.h"
#include "watcher_manager.h"
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
        std::shared_ptr<TestWatcher> watcher = std::make_shared<TestWatcher>();
        bool ret = data.WriteRemoteObject(watcher->AsObject());
        WATCHER_CHECK(ret, return 0, "Can not get remote");
        watcherManager->OnRemoteRequest(IWatcherManager::ADD_WATCHER, data, reply, option);
        watcherId = reply.ReadUint32();
        EXPECT_NE(watcherId, 0);
        printf("TestAddWatcher %s watcherId %d %p \n", keyPrefix.c_str(), watcherId, watcherManager);
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
        watcherManager->StopLoop();
        printf("TestDelWatcher %s watcherId %d %p \n", keyPrefix.c_str(), watcherId, watcherManager);
        return 0;
    }

    WatcherManagerPtr GetWatcherManager()
    {
        if (g_watcherManager == nullptr) {
            g_watcherManager = new WatcherManager(0, true);
            if (g_watcherManager == nullptr) {
                return nullptr;
            }
        }
        return g_watcherManager;
    }
};

HWTEST_F(WatcherProxyTest, TestAddWatcher, TestSize.Level1)
{
    WatcherProxyTest test;
    uint32_t watcherId = 0;
    test.TestAddWatcher("test.permission.watcher.test1", watcherId);
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
    SystemSetParameter("test.permission.watcher.test4", "1101");
    SystemSetParameter("test.permission.watcher.test4.test", "1102");
    test.TestAddWatcher("test.permission.watcher.test*", watcherId);
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