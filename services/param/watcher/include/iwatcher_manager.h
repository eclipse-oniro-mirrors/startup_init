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

#ifndef ISTARTUP_WATCHER_MANAGER_H
#define ISTARTUP_WATCHER_MANAGER_H

#include <iostream>
#include "iremote_broker.h"
#include "iremote_proxy.h"
#include "iwatcher.h"

namespace OHOS {
namespace init_param {
class IWatcherManager : public OHOS::IRemoteBroker {
public:
    enum {
        ADD_WATCHER,
        DEL_WATCHER,
        ADD_REMOTE_AGENT,
        DEL_REMOTE_AGENT,
        REFRESH_WATCHER
    };

    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.Startup.IWatcherManager");
public:
    virtual uint32_t AddRemoteWatcher(uint32_t id, const sptr<IWatcher> &watcher) = 0;
    virtual int32_t DelRemoteWatcher(uint32_t remoteWatcherId) = 0;
    virtual int32_t AddWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) = 0;
    virtual int32_t DelWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) = 0;
    virtual int32_t RefreshWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) = 0;
};
} // namespace update_engine
} // namespace OHOS
#endif // ISTARTUP_WATCHER_MANAGER_H
