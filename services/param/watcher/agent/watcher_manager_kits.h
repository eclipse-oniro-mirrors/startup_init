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

#ifndef WATCHER_MANAGER_KITS_H
#define WATCHER_MANAGER_KITS_H
#include <functional>
#include <map>
#include <mutex>
#include <thread>

#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "singleton.h"
#include "init_param.h"
#include "watcher.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
class WatcherManagerKits final : public DelayedRefSingleton<WatcherManagerKits> {
    DECLARE_DELAYED_REF_SINGLETON(WatcherManagerKits);
public:
    DISALLOW_COPY_AND_MOVE(WatcherManagerKits);

    static WatcherManagerKits &GetInstance(void);
    int32_t AddWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context);
    int32_t DelWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context);
    void ReAddWatcher(void);

#ifndef STARTUP_INIT_TEST
private:
#endif
    class ParameterChangeListener {
    public:
        ParameterChangeListener(ParameterChangePtr callback, void *context)
            : callback_(callback), context_(context) {}
        ~ParameterChangeListener(void) = default;

        bool IsEqual(ParameterChangePtr callback, const void *context) const
        {
            return (callback == callback_ && context == context_);
        }
        void OnParameterChange(const std::string &name, const std::string &value);
        bool CheckValueChange(const std::string &value)
        {
            bool ret = (value_ == value);
            value_ = value;
            return ret;
        }
    private:
        std::string value_ {};
        ParameterChangePtr callback_ { nullptr };
        void *context_ { nullptr };
    };

    class ParamWatcher {
    public:
        explicit ParamWatcher(const std::string &key) : keyPrefix_(key) {}
        ~ParamWatcher(void)
        {
            parameterChangeListeners.clear();
        };
        void OnParameterChange(const std::string &name, const std::string &value);
        int AddParameterListener(ParameterChangePtr callback, void *context);
        int DelParameterListener(ParameterChangePtr callback, void *context);
    private:
        ParameterChangeListener *GetParameterListener(uint32_t *idx);
        void RemoveParameterListener(uint32_t idx);
        std::string keyPrefix_ {};
        std::mutex mutex_;
        uint32_t listenerId_ { 0 };
        std::map<uint32_t, std::shared_ptr<ParameterChangeListener>> parameterChangeListeners;
    };

    class RemoteWatcher final : public Watcher {
    public:
        explicit RemoteWatcher(WatcherManagerKits *watcherManager) : watcherManager_(watcherManager) {}
        ~RemoteWatcher(void) override {}

        void OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value) final;
    private:
        WatcherManagerKits *watcherManager_ = { nullptr };
    };

    using ParamWatcherKitPtr = std::shared_ptr<WatcherManagerKits::ParamWatcher>;
    // For death event procession
    class DeathRecipient final : public IRemoteObject::DeathRecipient {
    public:
        DeathRecipient(void) = default;
        ~DeathRecipient(void) final = default;
        DISALLOW_COPY_AND_MOVE(DeathRecipient);
        void OnRemoteDied(const wptr<IRemoteObject> &remote) final;
    };
    sptr<IRemoteObject::DeathRecipient> GetDeathRecipient(void)
    {
        return deathRecipient_;
    }

    uint32_t GetRemoteWatcher(void);
    ParamWatcher *GetParamWatcher(const std::string &keyPrefix);
    void ResetService(const wptr<IRemoteObject> &remote);
    sptr<IWatcherManager> GetService(void);
    std::mutex lock_;
    sptr<IWatcherManager> watcherManager_ {};
    sptr<IRemoteObject::DeathRecipient> deathRecipient_ {};

    std::mutex mutex_;
    uint32_t remoteWatcherId_ = { 0 };
    sptr<Watcher>  remoteWatcher_ = { nullptr };
    std::map<std::string, ParamWatcherKitPtr> watchers_;
    std::atomic<bool> stop_ { false };
    std::thread *threadForReWatch_ { nullptr };
};
} // namespace init_param
} // namespace OHOS
#endif // WATCHER_MANAGER_KITS_H
