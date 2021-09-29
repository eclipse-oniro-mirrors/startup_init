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
#include "message_parcel.h"
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
        : SystemAbility(systemAbilityId, runOnCreate) {}
    ~WatcherManager() override
    {
        watcherGroups_.clear();
        groupMap_.clear();
    };

    class ParamWatcher {
    public:
        ParamWatcher(uint32_t watcherId, const sptr<IWatcher> &watcher) : watcherId_(watcherId), watcher_(watcher) {}
        ~ParamWatcher() = default;
        uint32_t GetWatcherId()
        {
            return watcherId_;
        }
        void ProcessParameterChange(const std::string &name, const std::string &value)
        {
            watcher_->OnParamerterChange(name, value);
        }
    private:
        uint32_t watcherId_ = { 0 };
        sptr<IWatcher> watcher_;
    };
    using ParamWatcherPtr = std::shared_ptr<WatcherManager::ParamWatcher>;

    class WatcherGroup {
    public:
        WatcherGroup(uint32_t groupId, const std::string &key) : groupId_(groupId), keyPrefix_(key) {}
        ~WatcherGroup() = default;
        void AddWatcher(const ParamWatcherPtr &watcher);
        void DelWatcher(uint32_t watcherId);
        void ProcessParameterChange(const std::string &name, const std::string &value);
        ParamWatcherPtr GetWatcher(uint32_t watcherId);

        const std::string GetKeyPrefix()
        {
            return keyPrefix_;
        }
        bool Emptry()
        {
            return watchers_.size() == 0;
        }
        uint32_t GetGroupId()
        {
            return groupId_;
        }
    private:
        uint32_t groupId_;
        std::string keyPrefix_ { };
        std::map<uint32_t, ParamWatcherPtr> watchers_;
    };
    using WatcherGroupPtr = std::shared_ptr<WatcherManager::WatcherGroup>;

    uint32_t AddWatcher(const std::string &keyPrefix, const sptr<IWatcher> &watcher) override;
    int32_t DelWatcher(const std::string &keyPrefix, uint32_t watcherId) override;
protected:
    void OnStart() override;
    void OnStop() override;
private:
    WatcherGroupPtr GetWatcherGroup(uint32_t groupId);
    WatcherGroupPtr GetWatcherGroup(const std::string &keyPrefix);
    void AddWatcherGroup(const std::string &keyPrefix, WatcherGroupPtr group);
    void DelWatcherGroup(WatcherGroupPtr group);
    void RunLoop();
    void StartLoop();
    void SendLocalChange(const std::string &keyPrefix, ParamWatcherPtr watcher);
    void ProcessWatcherMessage(const std::vector<char> &buffer, uint32_t dataSize);
    int SendMessage(WatcherGroupPtr group, int type);
    int GetServerFd(bool retry);
private:
    std::atomic<uint32_t> watcherId_ { 0 };
    std::atomic<uint32_t> groupId_ { 0 };
    std::mutex mutex_;
    std::mutex watcherMutex_;
    int serverFd_ { -1 };
    std::thread *pRecvThread_ { nullptr };
    std::atomic<bool> stop { false };
    std::map<std::string, uint32_t> groupMap_ {};
    std::map<uint32_t, WatcherGroupPtr> watcherGroups_ {};
};
} // namespace init_param
} // namespace OHOS
#endif // WATCHER_MANAGER_H_