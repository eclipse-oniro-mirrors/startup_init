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
#ifndef WATCHER_MANAGER_H_
#define WATCHER_MANAGER_H_
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "iremote_stub.h"
#include "iwatcher.h"
#include "list.h"
#include "message_parcel.h"
#include "param_utils.h"
#include "parcel.h"
#include "system_ability.h"
#include "watcher_manager_stub.h"

namespace OHOS {
namespace init_param {
class WatcherManager : public SystemAbility, public WatcherManagerStub {
public:
    DECLARE_SYSTEM_ABILITY(WatcherManager);
    DISALLOW_COPY_AND_MOVE(WatcherManager);
    explicit WatcherManager(int32_t systemAbilityId, bool runOnCreate = true)
        : SystemAbility(systemAbilityId, runOnCreate)
    {
    }
    ~WatcherManager() override;

    class ParamWatcher;
    class WatcherGroup;
    using ParamWatcherPtr = std::shared_ptr<WatcherManager::ParamWatcher>;
    using WatcherGroupPtr = std::shared_ptr<WatcherManager::WatcherGroup>;

    // For death event procession
    class DeathRecipient final : public IRemoteObject::DeathRecipient {
    public:
        explicit DeathRecipient(WatcherManager *manager) : manager_(manager) {}
        ~DeathRecipient() final = default;
        DISALLOW_COPY_AND_MOVE(DeathRecipient);
        void OnRemoteDied(const wptr<IRemoteObject> &remote) final;
    private:
        WatcherManager *manager_;
    };
    sptr<IRemoteObject::DeathRecipient> GetDeathRecipient()
    {
        return deathRecipient_;
    }

    class ParamWatcher {
    public:
        ParamWatcher(uint32_t watcherId, const sptr<IWatcher> &watcher, const WatcherGroupPtr &group)
            : watcherId_(watcherId), watcher_(watcher), group_(group)
        {
            ListInit(&groupNode_);
        }
        ~ParamWatcher() = default;

        uint32_t GetWatcherId()
        {
            return watcherId_;
        }
        WatcherGroupPtr GetWatcherGroup()
        {
            return group_;
        }
        ListNode *GetGroupNode()
        {
            return &groupNode_;
        }
        sptr<IWatcher> GetRemoteWatcher()
        {
            return watcher_;
        }
        void ProcessParameterChange(const std::string &name, const std::string &value)
        {
#ifndef STARTUP_INIT_TEST
            watcher_->OnParamerterChange(name, value);
#endif
        }
        ListNode groupNode_ { };
    private:
        uint32_t watcherId_ = { 0 };
        sptr<IWatcher> watcher_;
        WatcherGroupPtr group_ { nullptr };
    };

    class WatcherGroup {
    public:
        WatcherGroup(uint32_t groupId, const std::string &key) : groupId_(groupId), keyPrefix_(key)
        {
            ListInit(&watchers_);
        }
        ~WatcherGroup() = default;
        void AddWatcher(const ParamWatcherPtr &watcher);
        void ProcessParameterChange(const std::string &name, const std::string &value);

        const std::string GetKeyPrefix()
        {
            return keyPrefix_;
        }
        bool Emptry()
        {
            return ListEmpty(watchers_);
        }
        uint32_t GetGroupId()
        {
            return groupId_;
        }
        ListNode *GetWatchers()
        {
            return &watchers_;
        }
    private:
        uint32_t groupId_;
        std::string keyPrefix_ { };
        ListNode watchers_;
    };

    uint32_t AddWatcher(const std::string &keyPrefix, const sptr<IWatcher> &watcher) override;
    int32_t DelWatcher(const std::string &keyPrefix, uint32_t watcherId) override;
    int32_t DelWatcher(WatcherGroupPtr group, ParamWatcherPtr watcher);
    ParamWatcherPtr GetWatcher(uint32_t watcherId);
    ParamWatcherPtr GetWatcher(const wptr<IRemoteObject> &remote);

#ifndef STARTUP_INIT_TEST
protected:
#endif
    void OnStart() override;
    void OnStop() override;
#ifndef STARTUP_INIT_TEST
private:
#endif
    void ProcessWatcherMessage(const std::vector<char> &buffer, uint32_t dataSize);
    WatcherGroupPtr GetWatcherGroup(uint32_t groupId);
    WatcherGroupPtr GetWatcherGroup(const std::string &keyPrefix);
    void DelWatcherGroup(WatcherGroupPtr group);
    void AddParamWatcher(const std::string &keyPrefix, WatcherGroupPtr group, ParamWatcherPtr watcher);
    void DelParamWatcher(ParamWatcherPtr watcher);
    void RunLoop();
    void StartLoop();
    void StopLoop();
    void SendLocalChange(const std::string &keyPrefix, ParamWatcherPtr watcher);
    int SendMessage(WatcherGroupPtr group, int type);
    int GetServerFd(bool retry);
    int GetWatcherId(uint32_t &watcherId);
    int GetGroupId(uint32_t &groupId);
private:
    std::atomic<uint32_t> watcherId_ { 0 };
    std::atomic<uint32_t> groupId_ { 0 };
    std::mutex mutex_;
    std::mutex watcherMutex_;
    int serverFd_ { -1 };
    std::thread *pRecvThread_ { nullptr };
    std::atomic<bool> stop_ { false };
    std::map<std::string, uint32_t> groupMap_ {};
    std::map<uint32_t, WatcherGroupPtr> watcherGroups_ {};
    std::map<uint32_t, ParamWatcherPtr> watchers_ {};
    sptr<IRemoteObject::DeathRecipient> deathRecipient_ {};
};
} // namespace init_param
} // namespace OHOS
#endif // WATCHER_MANAGER_H_