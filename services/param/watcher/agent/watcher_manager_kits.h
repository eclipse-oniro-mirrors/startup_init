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

    static WatcherManagerKits &GetInstance();
    int32_t AddWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context);
    int32_t DelWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context);
    void ReAddWatcher();
#ifndef STARTUP_INIT_TEST
private:
#endif
    class ParameterChangeListener {
    public:
        ParameterChangeListener(ParameterChangePtr callback, void *context)
            : callback_(callback), context_(context) {}
        ~ParameterChangeListener() = default;

        bool IsEqual(ParameterChangePtr callback, void *context)
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

    class ParamWatcher final : public Watcher {
    public:
        explicit ParamWatcher(const std::string &key) : keyPrefix_(key) {}
        ~ParamWatcher() override
        {
            parameterChangeListeners.clear();
        };

        void OnParameterChange(const std::string &name, const std::string &value) override;

        void SetWatcherId(uint32_t watcherId)
        {
            watcherId_ = watcherId;
        }
        uint32_t GetWatcherId()
        {
            return watcherId_;
        }
        int AddParameterListener(ParameterChangePtr callback, void *context);
        int DelParameterListener(ParameterChangePtr callback, void *context);
    private:
        ParameterChangeListener *GetParameterListener(uint32_t *idx);
        void RemoveParameterListener(uint32_t idx);
        uint32_t watcherId_ { 0 };
        std::string keyPrefix_ {};

        std::mutex mutex_;
        uint32_t listenerId_ { 0 };
        std::map<uint32_t, std::shared_ptr<ParameterChangeListener>> parameterChangeListeners;
    };
    using ParamWatcherKitPtr = sptr<WatcherManagerKits::ParamWatcher>;
    ParamWatcherKitPtr GetParamWatcher(const std::string &keyPrefix);

    // For death event procession
    class DeathRecipient final : public IRemoteObject::DeathRecipient {
    public:
        DeathRecipient() = default;
        ~DeathRecipient() final = default;
        DISALLOW_COPY_AND_MOVE(DeathRecipient);
        void OnRemoteDied(const wptr<IRemoteObject> &remote) final;
    };
    sptr<IRemoteObject::DeathRecipient> GetDeathRecipient()
    {
        return deathRecipient_;
    }

private:
    void ResetService(const wptr<IRemoteObject> &remote);
    sptr<IWatcherManager> GetService();
    std::mutex lock_;
    sptr<IWatcherManager> watcherManager_ {};
    sptr<IRemoteObject::DeathRecipient> deathRecipient_ {};

    std::mutex mutex_;
    std::map<std::string, ParamWatcherKitPtr> watchers_;
};
} // namespace init_param
} // namespace OHOS
#endif // WATCHER_MANAGER_KITS_H
