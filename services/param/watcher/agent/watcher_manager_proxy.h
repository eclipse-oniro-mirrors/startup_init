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

#ifndef STARTUP_WATCH_MANAGER_PROXY_H
#define STARTUP_WATCH_MANAGER_PROXY_H

#include "iremote_proxy.h"
#include "iwatcher_manager.h"

namespace OHOS {
namespace init_param {
class WatcherManagerProxy : public IRemoteProxy<IWatcherManager> {
public:
    explicit WatcherManagerProxy(const sptr<IRemoteObject> &impl) : IRemoteProxy<IWatcherManager>(impl) {}

    uint32_t AddRemoteWatcher(uint32_t id, const sptr<IWatcher> &watcher) override;
    int32_t DelRemoteWatcher(uint32_t remoteWatcherId) override;
    int32_t AddWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) override;
    int32_t DelWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) override;
    int32_t RefreshWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) override;
private:
    int32_t SendMsg(int op, const std::string &keyPrefix, uint32_t remoteWatcherId);
    static inline BrokerDelegator<WatcherManagerProxy> delegator_;
};
} // namespace init_param
} // namespace OHOS
#endif // STARTUP_WATCH_MANAGER_PROXY_H