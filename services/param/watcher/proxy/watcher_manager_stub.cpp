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

#include "watcher_manager_stub.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
int32_t WatcherManagerStub::OnRemoteRequest(uint32_t code,
    MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    std::u16string myDescripter = IWatcherManager::GetDescriptor();
    std::u16string remoteDescripter = data.ReadInterfaceToken();
    WATCHER_CHECK(myDescripter == remoteDescripter, return -1, "Invalid remoteDescripter");

    switch (code) {
        case ADD_WATCHER: {
            std::string key = data.ReadString();
            auto remote = data.ReadRemoteObject();
            // 0 is invalid watcherId
            WATCHER_CHECK(remote != nullptr, reply.WriteUint32(0);
                return 0, "Failed to read remote watcher");
            uint32_t watcherId = AddWatcher(key, iface_cast<IWatcher>(remote));
            reply.WriteUint32(watcherId);
            break;
        }
        case DEL_WATCHER: {
            std::string key = data.ReadString();
            uint32_t watcherId = data.ReadUint32();
            int ret = DelWatcher(key, watcherId);
            reply.WriteInt32(ret);
            break;
        }
        default: {
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
    return 0;
}
}
}
