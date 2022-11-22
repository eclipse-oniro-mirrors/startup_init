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
#include "sysparam_errno.h"

namespace OHOS {
namespace init_param {
void WatcherProxy::OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option { MessageOption::TF_ASYNC };

    data.WriteInterfaceToken(WatcherProxy::GetDescriptor());
    data.WriteString(prefix);
    data.WriteString(name);
    data.WriteString(value);
    auto remote = Remote();
#ifdef STARTUP_INIT_TEST
    int ret = 0;
#else
    int ret = SYSPARAM_SYSTEM_ERROR;
#endif
    if (remote != nullptr) {
        ret = remote->SendRequest(PARAM_CHANGE, data, reply, option);
    }
    WATCHER_CHECK(ret == ERR_OK, return, "Can not SendRequest for name %s err:%d", name.c_str(), ret);
    return;
}
}
} // namespace OHOS
