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

#ifndef OHOS_SYSTEM_DEVICEIDSTUB_H
#define OHOS_SYSTEM_DEVICEIDSTUB_H

#include "iremote_stub.h"
#include "idevice_info.h"
#include "system_ability.h"

namespace OHOS {
namespace device_info {
class DeviceInfoStub : public IRemoteStub<IDeviceInfo> {
public:
    int32_t OnRemoteRequest(uint32_t code, MessageParcel& data, MessageParcel& reply,
        MessageOption &option) override;
private:
    bool CheckPermission(MessageParcel &data, const std::string &permission);
};

class DeviceInfoService : public SystemAbility, public DeviceInfoStub {
public:
    DECLARE_SYSTEM_ABILITY(DeviceInfoService);
    DISALLOW_COPY_AND_MOVE(DeviceInfoService);
    explicit DeviceInfoService(int32_t systemAbilityId, bool runOnCreate = true)
        : SystemAbility(systemAbilityId, runOnCreate)
    {
    }
    ~DeviceInfoService() override {}
    int32_t GetUdid(std::string& result) override;
    int32_t GetSerialID(std::string& result) override;
#ifndef STARTUP_INIT_TEST
protected:
#endif
    void OnStart(void) override;
    void OnStop(void) override;
    int Dump(int fd, const std::vector<std::u16string>& args) override;
};
} // namespace device_info
} // namespace OHOS
#endif // OHOS_SYSTEM_DEVICEIDSTUB_H