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

#ifndef ISTARTUP_WATCHER_H
#define ISTARTUP_WATCHER_H

#include <iostream>
#include "iremote_broker.h"
#include "iremote_proxy.h"

namespace OHOS {
namespace init_param {
class IWatcher : public OHOS::IRemoteBroker {
public:
    virtual ~IWatcher() = default;
    enum {
        PARAM_CHANGE = 1,
    };

    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.Startup.IWatcher");
public:
    virtual void OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value) = 0;
};
} // namespace init_param
} // namespace OHOS
#endif // ISTARTUP_WATCHER_H
