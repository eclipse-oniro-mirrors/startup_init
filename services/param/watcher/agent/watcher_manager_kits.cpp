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
#include "watcher_manager_kits.h"

#include "if_system_ability_manager.h"
#include "init_param.h"
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "system_ability_definition.h"
#include "watcher_utils.h"
#include "parameter.h"

namespace OHOS {
namespace init_param {
WatcherManagerKits &WatcherManagerKits::GetInstance()
{
    return DelayedRefSingleton<WatcherManagerKits>::GetInstance();
}

WatcherManagerKits::WatcherManagerKits() {}

WatcherManagerKits::~WatcherManagerKits() {}

void WatcherManagerKits::ResetService(const wptr<IRemoteObject> &remote)
{
    WATCHER_LOGI("Remote is dead, reset service instance");
    bool resetService = false;
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (watcherManager_ != nullptr) {
            sptr<IRemoteObject> object = watcherManager_->AsObject();
            if ((object != nullptr) && (remote == object)) {
                object->RemoveDeathRecipient(deathRecipient_);
                watcherManager_ = nullptr;
                resetService = true;
            }
        }
    }
    if (resetService) {
        ReAddWatcher();
    }
}

sptr<IWatcherManager> WatcherManagerKits::GetService()
{
    std::lock_guard<std::mutex> lock(lock_);
    if (watcherManager_ != nullptr) {
        return watcherManager_;
    }

    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    WATCHER_CHECK(samgr != nullptr, return nullptr, "Get samgr failed");
    sptr<IRemoteObject> object = samgr->GetSystemAbility(PARAM_WATCHER_DISTRIBUTED_SERVICE_ID);
    WATCHER_CHECK(object != nullptr, return nullptr, "Get watcher manager object from samgr failed");
    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new DeathRecipient();
    }

    if ((object->IsProxyObject()) && (!object->AddDeathRecipient(deathRecipient_))) {
        WATCHER_LOGE("Failed to add death recipient");
    }
    watcherManager_ = iface_cast<IWatcherManager>(object);
    if (watcherManager_ == nullptr) {
        WATCHER_LOGE("watcher manager iface_cast failed");
    }
    return watcherManager_;
}

void WatcherManagerKits::DeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    DelayedRefSingleton<WatcherManagerKits>::GetInstance().ResetService(remote);
}

WatcherManagerKits::ParamWatcherKitPtr WatcherManagerKits::GetParamWatcher(const std::string &keyPrefix)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (watchers_.find(keyPrefix) == watchers_.end()) {
        return nullptr;
    }
    return watchers_[keyPrefix];
}

void WatcherManagerKits::ReAddWatcher()
{
    WATCHER_LOGV("ReAddWatcher ");
    int count = 0;
    const int maxRetryCount = 100;
    const int sleepTime = 100;
    auto watcherManager = GetService();
    while (watcherManager == nullptr && count < maxRetryCount) {
        watcherManager = GetService();
        usleep(sleepTime);
        count++;
    }
    WATCHER_CHECK(watcherManager != nullptr, return, "Failed to get watcher manager");
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto iter = watchers_.begin(); iter != watchers_.end(); iter++) {
        if (iter->second == nullptr) {
            continue;
        }
        WATCHER_LOGV("ReAddWatcher keyPrefix %s ", iter->first.c_str());
        uint32_t watcherId = watcherManager->AddWatcher(iter->first, iter->second);
        WATCHER_CHECK(watcherId != 0, continue, "Failed to add watcher for %s", iter->first.c_str());
        iter->second->SetWatcherId(watcherId);
    }
}

int32_t WatcherManagerKits::AddWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context)
{
    WATCHER_LOGV("AddWatcher keyPrefix %s", keyPrefix.c_str());
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get watcher manager");

    bool newWatcher = false;
    ParamWatcherKitPtr watcher = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (watchers_.find(keyPrefix) == watchers_.end()) {
            watcher = new ParamWatcher(keyPrefix);
            WATCHER_CHECK(watcher != nullptr, return -1, "Failed to create watcher for %s", keyPrefix.c_str());
            watchers_[keyPrefix] = watcher;
            newWatcher = true;
            int ret = watcher->AddParameterListener(callback, context);
            WATCHER_CHECK(ret == 0, return -1, "Failed to add callback for %s ", keyPrefix.c_str());
            uint32_t watcherId = watcherManager->AddWatcher(keyPrefix, watcher);
            WATCHER_CHECK(watcherId != 0, return -1, "Failed to add watcher for %s", keyPrefix.c_str());
            watcher->SetWatcherId(watcherId);
        } else {
            watcher = watchers_[keyPrefix];
        }
    }
    // save callback
    if (!newWatcher) { // refresh
        int ret = watcher->AddParameterListener(callback, context);
        WATCHER_CHECK(ret == 0, return -1, "Failed to add callback for %s ", keyPrefix.c_str());
        ret = watcherManager->RefreshWatcher(keyPrefix, watcher->GetWatcherId());
        WATCHER_CHECK(ret == 0, return -1, "Failed to refresh watcher for %s", keyPrefix.c_str());
    }
    WATCHER_LOGI("AddWatcher keyPrefix %s watcherId %u success", keyPrefix.c_str(), watcher->GetWatcherId());
    return 0;
}

int32_t WatcherManagerKits::DelWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context)
{
    ParamWatcherKitPtr watcher = GetParamWatcher(keyPrefix);
    WATCHER_CHECK(watcher != nullptr, return 0, "Can not find watcher for keyPrefix %s", keyPrefix.c_str());
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get watcher manager");
    int count = watcher->DelParameterListener(callback, context);
    WATCHER_LOGI("DelWatcher keyPrefix_ %s count %d", keyPrefix.c_str(), count);
    if (count == 0) {
        int ret = watcherManager->DelWatcher(keyPrefix, watcher->GetWatcherId());
        WATCHER_CHECK(ret == 0, return -1, "Failed to delete watcher for %s", keyPrefix.c_str());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto iter = watchers_.find(keyPrefix);
            if (iter != watchers_.end()) {
                watchers_.erase(iter);
            }
        }
    }
    return 0;
}

WatcherManagerKits::ParameterChangeListener *WatcherManagerKits::ParamWatcher::GetParameterListener(uint32_t *idx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t index = *idx;
    while (index < static_cast<uint32_t>(parameterChangeListeners.size())) {
        auto it = parameterChangeListeners.find(index);
        if (it != parameterChangeListeners.end()) {
            *idx = index;
            return parameterChangeListeners[index].get();
        }
        index++;
    }
    return nullptr;
}

void WatcherManagerKits::ParamWatcher::RemoveParameterListener(uint32_t idx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx >= static_cast<uint32_t>(parameterChangeListeners.size())) {
        return;
    }
    parameterChangeListeners.erase(idx);
}

void WatcherManagerKits::ParamWatcher::OnParameterChange(const std::string &name, const std::string &value)
{
    Watcher::OnParameterChange(name, value);
    WATCHER_LOGI("OnParameterChange name %s value %s", name.c_str(), value.c_str());
    uint32_t index = 0;
    ParameterChangeListener *listener = GetParameterListener(&index);
    while (listener != nullptr) {
        if (!listener->CheckValueChange(value)) {
            listener->OnParameterChange(name, value);
        }
        index++;
        listener = GetParameterListener(&index);
    }
}

int WatcherManagerKits::ParamWatcher::AddParameterListener(ParameterChangePtr callback, void *context)
{
    WATCHER_CHECK(callback != nullptr, return -1, "Invalid callback ");
    WATCHER_LOGV("AddParameterListener listenerId_ %d callback %p context %p", listenerId_, callback, context);
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = parameterChangeListeners.begin(); it != parameterChangeListeners.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }
        if (it->second->IsEqual(callback, context)) {
            WATCHER_LOGI("AddParameterListener callback %p context %p exist", callback, context);
            return -1;
        }
    }
    std::shared_ptr<ParameterChangeListener> changeNode =
        std::make_shared<ParameterChangeListener>(callback, context);
    WATCHER_CHECK(changeNode != nullptr, return -1, "Failed to create listener");
    parameterChangeListeners[listenerId_] = changeNode;
    listenerId_++;
    return 0;
}

int WatcherManagerKits::ParamWatcher::DelParameterListener(ParameterChangePtr callback, void *context)
{
    uint32_t index = 0;
    ParameterChangeListener *listener = GetParameterListener(&index);
    while (listener != nullptr) {
        if ((callback == nullptr && context == nullptr)) {
            RemoveParameterListener(index);
        } else if (listener->IsEqual(callback, context)) {
            WATCHER_LOGI("DelParameterListener callback %p context %p", callback, context);
            RemoveParameterListener(index);
            break;
        }
        index++;
        listener = GetParameterListener(&index);
    }
    return static_cast<int>(parameterChangeListeners.size());
}

void WatcherManagerKits::ParameterChangeListener::OnParameterChange(const std::string &name, const std::string &value)
{
    if (callback_ != nullptr) {
        callback_(name.c_str(), value.c_str(), context_);
    }
}
} // namespace init_param
} // namespace OHOS

int SystemWatchParameter(const char *keyPrefix, ParameterChangePtr callback, void *context)
{
    WATCHER_CHECK(keyPrefix != nullptr, return PARAM_CODE_INVALID_PARAM, "Invalid prefix");
    int ret = 0;
    std::string key(keyPrefix);
    if (key.rfind("*") == key.length() - 1) {
        ret = WatchParamCheck(key.substr(0, key.length() - 1).c_str());
    } else {
        ret = WatchParamCheck(keyPrefix);
    }
    if (ret != 0) {
        return ret;
    }
    OHOS::init_param::WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
    if (callback != nullptr) {
        ret = instance.AddWatcher(keyPrefix, callback, context);
    } else {
        ret = instance.DelWatcher(keyPrefix, nullptr, nullptr);
    }
    return ret;
}

int RemoveParameterWatcher(const char *keyPrefix, ParameterChgPtr callback, void *context)
{
    WATCHER_CHECK(keyPrefix != nullptr, return PARAM_CODE_INVALID_PARAM, "Invalid prefix");
    int ret = 0;
    std::string key(keyPrefix);
    if (key.rfind("*") == key.length() - 1) {
        ret = WatchParamCheck(key.substr(0, key.length() - 1).c_str());
    } else {
        ret = WatchParamCheck(keyPrefix);
    }
    if (ret != 0) {
        return ret;
    }
    OHOS::init_param::WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
    return instance.DelWatcher(keyPrefix, (ParameterChangePtr)callback, context);
}