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
#include "watcher_manager_proxy.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
uint32_t WatcherManagerProxy::AddRemoteWatcher(uint32_t id, const sptr<IWatcher> &watcher)
{
    WATCHER_CHECK(watcher != nullptr, return ERR_INVALID_VALUE, "Invalid param");
    auto remote = Remote();
    WATCHER_CHECK(remote != nullptr, return 0, "Can not get remote");

    MessageParcel data;
    data.WriteInterfaceToken(WatcherManagerProxy::GetDescriptor());
    bool ret = data.WriteRemoteObject(watcher->AsObject());
    WATCHER_CHECK(ret, return 0, "Can not get remote");
    data.WriteUint32(id);

    MessageParcel reply;
    MessageOption option { MessageOption::TF_SYNC };
    int32_t res = remote->SendRequest(ADD_REMOTE_AGENT, data, reply, option);
    WATCHER_CHECK(res == ERR_OK, return 0, "Transact error %d", res);
    return reply.ReadUint32();
}

int32_t WatcherManagerProxy::DelRemoteWatcher(uint32_t remoteWatcherId)
{
    auto remote = Remote();
    WATCHER_CHECK(remote != nullptr, return ERR_FLATTEN_OBJECT, "Can not get remote");

    MessageParcel data;
    data.WriteInterfaceToken(WatcherManagerProxy::GetDescriptor());
    data.WriteUint32(remoteWatcherId);
    MessageParcel reply;
    MessageOption option { MessageOption::TF_SYNC };
    int32_t res = remote->SendRequest(DEL_REMOTE_AGENT, data, reply, option);
    WATCHER_CHECK(res == ERR_OK, return ERR_FLATTEN_OBJECT, "Transact error");
    return reply.ReadInt32();
}

int32_t WatcherManagerProxy::SendMsg(int op, const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    auto remote = Remote();
    WATCHER_CHECK(remote != nullptr, return 0, "Can not get remote");

    MessageParcel data;
    data.WriteInterfaceToken(WatcherManagerProxy::GetDescriptor());
    data.WriteString(keyPrefix);
    data.WriteUint32(remoteWatcherId);
    MessageParcel reply;
    MessageOption option { MessageOption::TF_SYNC };
    int32_t res = remote->SendRequest(op, data, reply, option);
    WATCHER_CHECK(res == ERR_OK, return 0, "Transact error");
    return reply.ReadInt32();
}

int32_t WatcherManagerProxy::AddWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    return SendMsg(ADD_WATCHER, keyPrefix, remoteWatcherId);
}

int32_t WatcherManagerProxy::DelWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    return SendMsg(DEL_WATCHER, keyPrefix, remoteWatcherId);
}

int32_t WatcherManagerProxy::RefreshWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    return SendMsg(REFRESH_WATCHER, keyPrefix, remoteWatcherId);
}
}
} // namespace OHOS
