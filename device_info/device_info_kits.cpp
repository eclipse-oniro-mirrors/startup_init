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
#include "device_info_kits.h"

#include "beget_ext.h"
#include "device_info_proxy.h"
#include "idevice_info.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "securec.h"

namespace OHOS {
namespace device_info {
DeviceInfoKits::DeviceInfoKits() {}

DeviceInfoKits::~DeviceInfoKits() {}

DeviceInfoKits &DeviceInfoKits::GetInstance()
{
    return DelayedRefSingleton<DeviceInfoKits>::GetInstance();
}

void DeviceInfoKits::ResetService(const wptr<IRemoteObject> &remote)
{
    std::lock_guard<std::mutex> lock(lock_);
    if (deviceInfoService_ != nullptr) {
        sptr<IRemoteObject> object = deviceInfoService_->AsObject();
        if ((object != nullptr) && (remote == object)) {
            object->RemoveDeathRecipient(deathRecipient_);
            deviceInfoService_ = nullptr;
        }
    }
}

sptr<IDeviceInfo> DeviceInfoKits::GetService()
{
    std::lock_guard<std::mutex> lock(lock_);
    if (deviceInfoService_ != nullptr) {
        return deviceInfoService_;
    }

    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    DINFO_CHECK(samgr != nullptr, return nullptr, "Get samgr failed");
    sptr<IRemoteObject> object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    DINFO_CHECK(object != nullptr, return nullptr, "Get device service object from samgr failed");
    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new DeathRecipient();
    }

    if ((object->IsProxyObject()) && (!object->AddDeathRecipient(deathRecipient_))) {
        DINFO_LOGE("Failed to add death recipient");
    }
    deviceInfoService_ = iface_cast<IDeviceInfo>(object);
    if (deviceInfoService_ == nullptr) {
        DINFO_LOGE("device service iface_cast failed");
    }
    return deviceInfoService_;
}

void DeviceInfoKits::DeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    DelayedRefSingleton<DeviceInfoKits>::GetInstance().ResetService(remote);
}

int32_t DeviceInfoKits::GetUdid(std::string& result)
{
    auto deviceService = GetService();
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get watcher manager");
    return deviceService->GetUdid(result);
}

int32_t DeviceInfoKits::GetSerialID(std::string& result)
{
    auto deviceService = GetService();
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get watcher manager");
    return deviceService->GetSerialID(result);
}
} // namespace device_info
} // namespace OHOS
