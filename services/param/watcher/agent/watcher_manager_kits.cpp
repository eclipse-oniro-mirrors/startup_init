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
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "param_request.h"
#include "system_ability_definition.h"
#include "watcher_utils.h"

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
    std::lock_guard<std::mutex> lock(lock_);
    if (watcherManager_ != nullptr) {
        sptr<IRemoteObject> object = watcherManager_->AsObject();
        if ((object != nullptr) && (remote == object)) {
            object->RemoveDeathRecipient(deathRecipient_);
            watcherManager_ = nullptr;
        }
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

void WatcherManagerKits::SetParamWatcher(const std::string &keyPrefix, ParamWatcherKitPtr watcher)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (watchers_.find(keyPrefix) != watchers_.end()) {
        watchers_[keyPrefix] = watcher;
    }
}

int32_t WatcherManagerKits::AddWatcher(const std::string &keyPrefix, ParameterChangePtr callback, void *context)
{
    WATCHER_LOGI("AddWatcher keyPrefix %s", keyPrefix.c_str());
    ParamWatcherKitPtr watcher = GetParamWatcher(keyPrefix);
    if (watcher != nullptr) {
        WATCHER_LOGE("Has been watched by keyPrefix %s", keyPrefix.c_str());
        return 0;
    }
    watcher = new ParamWatcher(keyPrefix, callback, context);
    WATCHER_CHECK(watcher != nullptr, return -1, "Failed to create watcher for %s", keyPrefix.c_str());
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get watcher manager");
    uint32_t watcherId = watcherManager->AddWatcher(keyPrefix, watcher);
    WATCHER_CHECK(watcherId != 0, return -1, "Failed to add watcher for %s", keyPrefix.c_str());
    watcher->SetWatcherId(watcherId);
    SetParamWatcher(keyPrefix, watcher);
    return watcher->GetWatcherId();
}

int32_t WatcherManagerKits::DelWatcher(const std::string &keyPrefix)
{
    ParamWatcherKitPtr watcher = GetParamWatcher(keyPrefix);
    if (watcher == nullptr) {
        WATCHER_LOGE("Has been watched by keyPrefix %s", keyPrefix.c_str());
        return 0;
    }
    auto watcherManager = GetService();
    WATCHER_CHECK(watcherManager != nullptr, return -1, "Failed to get watcher manager");
    int ret = watcherManager->DelWatcher(keyPrefix, watcher->GetWatcherId());
    WATCHER_CHECK(ret == 0, return -1, "Failed to delete watcher for %s", keyPrefix.c_str());
    SetParamWatcher(keyPrefix, nullptr);
    return 0;
}

void WatcherManagerKits::ParamWatcher::OnParamerterChange(const std::string &name, const std::string &value)
{
    WATCHER_LOGD("OnParamerterChange name %s value %s", name.c_str(), value.c_str());
    if (callback_ != nullptr) {
        callback_(name.c_str(), value.c_str(), context_);
    }
}
} // namespace init_param
} // namespace OHOS

int SystemWatchParameter(const char *keyPrefix, ParameterChangePtr callback, void *context)
{
    if (keyPrefix == nullptr) {
        return PARAM_CODE_INVALID_PARAM;
    }
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
        ret = (instance.AddWatcher(keyPrefix, callback, context) > 0) ? 0 : -1;
    } else {
        ret = instance.DelWatcher(keyPrefix);
    }
    return ret;
}
