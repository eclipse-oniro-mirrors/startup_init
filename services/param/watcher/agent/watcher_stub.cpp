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

#include "watcher_stub.h"

#include "securec.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
int32_t WatcherStub::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    std::u16string myDescriptor = IWatcher::GetDescriptor();
    std::u16string remoteDescriptor = data.ReadInterfaceToken();
    WATCHER_CHECK(myDescriptor == remoteDescriptor, return -1, "Invalid remoteDescriptor");

    switch (code) {
        case PARAM_CHANGE: {
            std::string key = data.ReadString();
            std::string name = data.ReadString();
            std::string value = data.ReadString();
            OnParameterChange(key, name, value);
            break;
        }
        default: {
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
    return 0;
}
} // namespace init_param
} // namespace OHOS
