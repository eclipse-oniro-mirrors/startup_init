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

#include "watcher_proxy.h"

#include <string_ex.h>
#include "parcel.h"
#include "message_parcel.h"
#include "watcher_utils.h"
#include "securec.h"

namespace OHOS {
namespace init_param {
void WatcherProxy::OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value)
{
    WATCHER_LOGV("WatcherProxy::OnParameterChange %s %s %s", prefix.c_str(), name.c_str(), value.c_str());
    MessageParcel data;
    MessageParcel reply;
    MessageOption option { MessageOption::TF_ASYNC };
    auto remote = Remote();
    WATCHER_CHECK(remote != nullptr, return, "Can not get remote");

    data.WriteInterfaceToken(WatcherProxy::GetDescriptor());
    data.WriteString(prefix);
    data.WriteString(name);
    data.WriteString(value);
    int ret = remote->SendRequest(PARAM_CHANGE, data, reply, option);
    WATCHER_CHECK(ret == ERR_OK, return, "Can not SendRequest for name %s err:%d", name.c_str(), ret);
    return;
}
}
} // namespace OHOS
