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
#include "watcher_proxy.h"
#include "watcher_utils.h"
#ifdef HICOLLIE_ENABLE
#include "xcollie/xcollie.h"
#include "xcollie/xcollie_define.h"
#endif

namespace OHOS {
namespace init_param {
int32_t WatcherManagerStub::OnRemoteRequest(uint32_t code,
    MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    std::u16string myDescriptor = IWatcherManager::GetDescriptor();
    std::u16string remoteDescriptor = data.ReadInterfaceToken();
    WATCHER_CHECK(myDescriptor == remoteDescriptor, return -1, "Invalid remoteDescriptor %u", code);

    WATCHER_LOGV("OnRemoteRequest code %u", code);
    switch (code) {
        case static_cast<uint32_t>(ParamWatcherInterfaceCode::ADD_WATCHER): {
            std::string key = data.ReadString();
#ifdef HICOLLIE_ENABLE
            const int dfxDelayS = 20;
            int id = HiviewDFX::XCollie::GetInstance().SetTimer("ParamWatcher_AddWatcher",
                dfxDelayS, nullptr, nullptr, HiviewDFX::XCOLLIE_FLAG_LOG);
#endif
            int ret = AddWatcher(key, data.ReadUint32());
#ifdef HICOLLIE_ENABLE
            HiviewDFX::XCollie::GetInstance().CancelTimer(id);
#endif
            reply.WriteInt32(ret);
            break;
        }
        case static_cast<uint32_t>(ParamWatcherInterfaceCode::DEL_WATCHER): {
            std::string key = data.ReadString();
            int ret = DelWatcher(key, data.ReadUint32());
            reply.WriteInt32(ret);
            break;
        }
        case static_cast<uint32_t>(ParamWatcherInterfaceCode::ADD_REMOTE_AGENT): {
            auto remote = data.ReadRemoteObject();
            // 0 is invalid watcherId
            uint32_t id = data.ReadUint32();
            sptr<IWatcher> watcher = new WatcherProxy(remote);
            uint32_t remoteWatcherId = AddRemoteWatcher(id, watcher);
            reply.WriteUint32(remoteWatcherId);
            break;
        }
        case static_cast<uint32_t>(ParamWatcherInterfaceCode::DEL_REMOTE_AGENT): {
            int ret = DelRemoteWatcher(data.ReadUint32());
            reply.WriteInt32(ret);
            break;
        }
        case static_cast<uint32_t>(ParamWatcherInterfaceCode::REFRESH_WATCHER): {
            std::string key = data.ReadString();
            int ret = RefreshWatcher(key, data.ReadUint32());
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
