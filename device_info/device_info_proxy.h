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

#ifndef OHOS_SYSTEM_DEVICEIDPROXY_H
#define OHOS_SYSTEM_DEVICEIDPROXY_H

#include "iremote_proxy.h"
#include "idevice_info.h"
#include "beget_ext.h"

namespace OHOS {
namespace device_info {
class INIT_LOCAL_API DeviceInfoProxy : public IRemoteProxy<IDeviceInfo> {
public:
    explicit DeviceInfoProxy(const sptr<IRemoteObject> &impl) : IRemoteProxy<IDeviceInfo>(impl) {}
    virtual ~DeviceInfoProxy() {}

    int32_t GetUdid(std::string& result) override;
    int32_t GetSerialID(std::string& result) override;
private:
    int32_t DeviceInfoSendRequest(uint32_t code,
        MessageParcel &data, MessageParcel &reply, MessageOption &option);
    static inline BrokerDelegator<DeviceInfoProxy> delegator_;
};
} // namespace system
} // namespace OHOS
#endif // OHOS_SYSTEM_DEVICEIDPROXY_H