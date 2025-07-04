/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "addwatcher_fuzzer.h"
#include "if_system_ability_manager.h"
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
#include "watcher_proxy.h"
#include "watcher_utils.h"
#define private public
#define protected public
#include "watcher_manager.h"
#undef protected
#undef private

using namespace OHOS::init_param;
using WatcherManagerPtr = WatcherManager *;
class TestWatcher final : public Watcher {
public:
    TestWatcher() {}
    ~TestWatcher() = default;

    int32_t OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value) override
    {
        printf("TestWatcher::OnParameterChange name %s %s \n", name.c_str(), value.c_str());
        return 0;
    }
};
namespace OHOS {
    WatcherManagerPtr GetWatcherManager()
    {
        static WatcherManagerPtr watcherManager_ = nullptr;
        if (watcherManager_ == nullptr) {
            watcherManager_ = new WatcherManager(0, true);
            if (watcherManager_ == nullptr) {
                return nullptr;
            }
            watcherManager_->OnStart();
        }
        return watcherManager_;
    }

    int FuzzTestAddRemoteWatcher(uint32_t agentId, uint32_t &watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;

        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteUint32(agentId);
        sptr<IWatcher> watcher = new TestWatcher();
        bool ret = data.WriteRemoteObject(watcher->AsObject());
        
        WATCHER_CHECK(ret, return 0, "Can not get remote");
        watcherManager->OnRemoteRequest(
            static_cast<uint32_t> (IWatcherManagerIpcCode::COMMAND_ADD_REMOTE_WATCHER), data, reply, option);
        reply.ReadInt32();
        watcherId = reply.ReadUint32();

        auto remoteWatcher = watcherManager->GetRemoteWatcher(watcherId);
        if (remoteWatcher == nullptr) {
            return -1;
        }
        return 0;
    }

    int FuzzTestAddWatcher(const std::string &keyPrefix, uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;

        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteString16(Str8ToStr16(keyPrefix));
        data.WriteUint32(watcherId);
        watcherManager->OnRemoteRequest(
            static_cast<uint32_t> (IWatcherManagerIpcCode::COMMAND_ADD_WATCHER), data, reply, option);
        return 0;
    }

    int FuzzTestWatchAgentDump(const std::string &keyPrefix)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
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

    int FuzzTestDelWatcher(const std::string &keyPrefix, uint32_t watcherId)
    {
        WatcherManagerPtr watcherManager = GetWatcherManager();
        WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to create manager");
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteString16(Str8ToStr16(keyPrefix));
        data.WriteUint32(watcherId);
        watcherManager->OnRemoteRequest(
            static_cast<uint32_t> (IWatcherManagerIpcCode::COMMAND_DEL_WATCHER), data, reply, option);
        return 0;
    }

    bool FuzzAddWatcher(const uint8_t* data, size_t size)
    {
        if (size < sizeof(uint32_t)) {
            return false;
        }
        std::unique_ptr<WatcherManager> watcherManager = std::make_unique<WatcherManager>(0, true);
        std::string str(reinterpret_cast<const char*>(data), size);
        uint32_t watcherId = 0;

        FuzzTestAddRemoteWatcher(getpid(), watcherId);
        FuzzTestAddWatcher("test.permission.watcher.test1", 0);
        FuzzTestAddWatcher("test.permission.watcher.test1", 0);
        FuzzTestDelWatcher("test.permission.watcher.test1", 0);
        watcherId = 1;
        FuzzTestAddRemoteWatcher(getpid(), watcherId);
        FuzzTestAddWatcher("test.permission.watcher.test1", 1);
        FuzzTestWatchAgentDump("test.permission.watcher.test1");
        FuzzTestDelWatcher("test.permission.watcher.test1", 1);

        uint32_t randomValue = 0;
        errno_t ret = memcpy_s(&randomValue, sizeof(randomValue), data, sizeof(uint32_t));
        if (ret != 0) {
            return false;
        }
        if (!FuzzTestAddWatcher(str, randomValue)) {
            FuzzTestWatchAgentDump(str);
            FuzzTestDelWatcher(str, randomValue);
            return true;
        };
        return false;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzAddWatcher(data, size);
    return 0;
}