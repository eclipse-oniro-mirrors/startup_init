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
WatcherManagerKits &WatcherManagerKits::GetInstance(void)
{
    return DelayedRefSingleton<WatcherManagerKits>::GetInstance();
}

WatcherManagerKits::WatcherManagerKits(void) {}

WatcherManagerKits::~WatcherManagerKits(void) {}

void WatcherManagerKits::ResetService(const wptr<IRemoteObject> &remote)
{
    WATCHER_LOGI("Remote is dead, reset service instance");
    std::lock_guard<std::mutex> lock(lock_);
    if (watcherManager_ != nullptr) {
        sptr<IRemoteObject> object = watcherManager_->AsObject();
        if ((object != nullptr) && (remote == object)) {
            object->RemoveDeathRecipient(deathRecipient_);
            watcherManager_ = nullptr;
            remoteWatcherId_ = 0;
            remoteWatcher_ = nullptr;
            if (threadForReWatch_ != nullptr) {
                WATCHER_LOGI("Thead exist, delete thread");
                stop_ = true;
                threadForReWatch_->join();
                delete threadForReWatch_;
            }
            stop_ = false;
            threadForReWatch_ = new (std::nothrow)std::thread(&WatcherManagerKits::ReAddWatcher, this);
            WATCHER_CHECK(threadForReWatch_ != nullptr, return, "Failed to create thread");
        }
    }
}

sptr<IWatcherManager> WatcherManagerKits::GetService(void)
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

void WatcherManagerKits::ReAddWatcher(void)
{
    WATCHER_LOGV("ReAddWatcher");
    int count = 0;
    const int maxRetryCount = 100;
    const int sleepTime = 100;
    auto watcherManager = GetService();
    while (watcherManager == nullptr && count < maxRetryCount) {
        if (stop_) {
            return;
        }
        watcherManager = GetService();
        usleep(sleepTime);
        count++;
    }
    WATCHER_LOGV("ReAddWatcher count %d ", count);
    WATCHER_CHECK(watcherManager != nullptr, return, "Failed to get watcher manager");
    // add or get remote agent
    uint32_t remoteWatcherId = GetRemoteWatcher();
    WATCHER_CHECK(remoteWatcherId > 0, return, "Failed to get remote agent");
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto iter = watchers_.begin(); iter != watchers_.end(); iter++) {
        if (stop_) {
            return;
        }
        if (iter->second == nullptr) {
            continue;
        }
        WATCHER_LOGI("Add old watcher keyPrefix %s ", iter->first.c_str());
        int ret = watcherManager->AddWatcher(iter->first, remoteWatcherId);
        WATCHER_CHECK(ret == 0, continue, "Failed to add watcher for %s", iter->first.c_str());
    }
}

WatcherManagerKits::ParamWatcher *WatcherManagerKits::GetParamWatcher(const std::string &keyPrefix)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = watchers_.find(keyPrefix);
    if (iter != watchers_.end()) {
        return iter->second.get();
    }
    return nullptr;
}

uint32_t WatcherManagerKits::GetRemoteWatcher(void)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (remoteWatcher_ != nullptr) {
        return remoteWatcherId_;
    }
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return 0, "Failed to get watcher manager");
    remoteWatcher_  = new RemoteWatcher(this);
    WATCHER_CHECK(remoteWatcher_ != nullptr, return 0, "Failed to create watcher");
    remoteWatcherId_ = watcherManager->AddRemoteWatcher(getpid(), remoteWatcher_);
    WATCHER_CHECK(remoteWatcherId_ != 0, return 0, "Failed to add watcher");
    return remoteWatcherId_;
}

int32_t WatcherManagerKits::AddWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context)
{
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get watcher manager");

    // add or get remote agent
    uint32_t remoteWatcherId = GetRemoteWatcher();
    WATCHER_CHECK(remoteWatcherId > 0, return -1, "Failed to get remote agent");
    ParamWatcherKitPtr watcher = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // must check
        WATCHER_CHECK(remoteWatcherId > 0, return -1, "Failed to get remote agent");
        if (watchers_.find(keyPrefix) == watchers_.end()) {
            watcher = std::make_shared<ParamWatcher>(keyPrefix);
            WATCHER_CHECK(watcher != nullptr, return -1, "Failed to create watcher for %s", keyPrefix.c_str());
            int ret = watcher->AddParameterListener(callback, context);
            WATCHER_CHECK(ret == 0, return -1, "Failed to add callback for %s ", keyPrefix.c_str());
            ret = watcherManager->AddWatcher(keyPrefix, remoteWatcherId);
            WATCHER_CHECK(ret == 0, return -1, "Failed to add watcher for %s", keyPrefix.c_str());
            watchers_[keyPrefix] = watcher;
        } else {
            watcher = watchers_[keyPrefix];
            int ret = watcher->AddParameterListener(callback, context);
            WATCHER_CHECK(ret == 0, return -1, "Failed to add callback for %s ", keyPrefix.c_str());
            ret = watcherManager->RefreshWatcher(keyPrefix, remoteWatcherId);
            WATCHER_CHECK(ret == 0, return -1,
                "Failed to refresh watcher for %s %d", keyPrefix.c_str(), remoteWatcherId);
        }
    }
    WATCHER_LOGI("Add watcher keyPrefix %s remoteWatcherId %u success", keyPrefix.c_str(), remoteWatcherId);
    return 0;
}

int32_t WatcherManagerKits::DelWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context)
{
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get watcher manager");

    WatcherManagerKits::ParamWatcher *watcher = GetParamWatcher(keyPrefix);
    WATCHER_CHECK(watcher != nullptr, return -1, "Failed to get watcher");

    int count = watcher->DelParameterListener(callback, context);
    WATCHER_LOGI("DelWatcher keyPrefix_ %s count %d", keyPrefix.c_str(), count);
    if (count != 0) {
        return 0;
    }
    // delete watcher
    int ret = watcherManager->DelWatcher(keyPrefix, remoteWatcherId_);
    WATCHER_CHECK(ret == 0, return -1, "Failed to delete watcher for %s", keyPrefix.c_str());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = watchers_.find(keyPrefix); // delete watcher
        if (it != watchers_.end()) {
            watchers_.erase(it);
        }
        if (watchers_.empty()) { // no watcher, so delete remote agent
            watcherManager->DelRemoteWatcher(remoteWatcherId_);
            remoteWatcherId_ = 0;
            remoteWatcher_ = nullptr;
        }
    }
    return 0;
}

WatcherManagerKits::ParameterChangeListener *WatcherManagerKits::ParamWatcher::GetParameterListener(uint32_t *idx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t index = *idx;
    if (parameterChangeListeners.empty()) {
        return nullptr;
    }
    while (index < listenerId_) {
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
    auto it = parameterChangeListeners.find(idx);
    if (it != parameterChangeListeners.end()) {
        parameterChangeListeners.erase(it);
    }
}

int WatcherManagerKits::ParamWatcher::AddParameterListener(ParameterChangePtr callback, void *context)
{
    WATCHER_CHECK(callback != nullptr, return -1, "Invalid callback ");
    WATCHER_LOGV("AddParameterListener %s listenerId_ %d", keyPrefix_.c_str(), listenerId_);
    for (auto it = parameterChangeListeners.begin(); it != parameterChangeListeners.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }
        if (it->second->IsEqual(callback, context)) {
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
    if (callback == nullptr) {
        parameterChangeListeners.clear();
        return 0;
    }
    uint32_t index = 0;
    ParameterChangeListener *listener = GetParameterListener(&index);
    while (listener != nullptr) {
        if (listener->IsEqual(callback, context)) {
            WATCHER_LOGV("DelParameterListener listenerId_ %d", index);
            RemoveParameterListener(index);
            break;
        }
        index++;
        listener = GetParameterListener(&index);
    }
    return static_cast<int>(parameterChangeListeners.size());
}

void WatcherManagerKits::RemoteWatcher::OnParameterChange(
    const std::string &prefix, const std::string &name, const std::string &value)
{
    // get param watcher
    WatcherManagerKits::ParamWatcher *watcher = watcherManager_->GetParamWatcher(prefix);
    WATCHER_CHECK(watcher != nullptr, return, "Failed to get watcher '%s'", prefix.c_str());
    if (watcher != nullptr) {
        watcher->OnParameterChange(name, value);
    }
}

void WatcherManagerKits::ParamWatcher::OnParameterChange(const std::string &name, const std::string &value)
{
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

void WatcherManagerKits::ParameterChangeListener::OnParameterChange(const std::string &name, const std::string &value)
{
    if (callback_ != nullptr) {
        callback_(name.c_str(), value.c_str(), context_);
    }
}
} // namespace init_param
} // namespace OHOS

static int PreHandleWatchParam(std::string &prefix)
{
    // clear space in head or tail
    prefix.erase(0, prefix.find_first_not_of(" "));
    prefix.erase(prefix.find_last_not_of(" ") + 1);
    WATCHER_CHECK(!prefix.empty(), return PARAM_CODE_INVALID_PARAM, "Invalid prefix");
    int ret = 0;
    if (prefix.rfind(".*") == prefix.length() - 2) { // 2 last index
        ret = WatchParamCheck(prefix.substr(0, prefix.length() - 2).c_str()); // 2 last index
    } else if (prefix.rfind("*") == prefix.length() - 1) {
        ret = WatchParamCheck(prefix.substr(0, prefix.length() - 1).c_str());
    } else if (prefix.rfind(".") == prefix.length() - 1) {
        ret = WatchParamCheck(prefix.substr(0, prefix.length() - 1).c_str());
    } else {
        ret = WatchParamCheck(prefix.c_str());
    }
    return ret;
}

int SystemWatchParameter(const char *keyPrefix, ParameterChangePtr callback, void *context)
{
    WATCHER_CHECK(keyPrefix != nullptr, return PARAM_CODE_INVALID_PARAM, "Invalid prefix");
    std::string key(keyPrefix);
    int ret = PreHandleWatchParam(key);
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
    std::string key(keyPrefix);
    int ret = PreHandleWatchParam(key);
    if (ret != 0) {
        return ret;
    }
    OHOS::init_param::WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
    return instance.DelWatcher(keyPrefix, (ParameterChangePtr)callback, context);
}