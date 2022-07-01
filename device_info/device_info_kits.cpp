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
#include "device_info_load.h"
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

void DeviceInfoKits::LoadDeviceInfoSa()
{
    DINFO_LOGV("deviceInfoService_ is %d", deviceInfoService_ == nullptr);
    std::unique_lock<std::mutex> lock(lock_);
    if (deviceInfoService_ != nullptr) {
        return;
    }
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    DINFO_CHECK(sam != nullptr, return, "GetSystemAbilityManager return null");

    sptr<DeviceInfoLoad> deviceInfoLoad = new (std::nothrow) DeviceInfoLoad();
    DINFO_CHECK(deviceInfoLoad != nullptr, return, "new deviceInfoLoad fail.");

    int32_t ret = sam->LoadSystemAbility(SYSPARAM_DEVICE_SERVICE_ID, deviceInfoLoad);
    DINFO_CHECK(ret == ERR_OK, return, "LoadSystemAbility deviceinfo sa failed");

    // wait_for release lock and block until time out(60s) or match the condition with notice
    auto waitStatus = deviceInfoLoadCon_.wait_for(lock, std::chrono::milliseconds(DEVICEINFO_LOAD_SA_TIMEOUT_MS),
        [this]() { return deviceInfoService_ != nullptr; });
    if (!waitStatus) {
        // time out or loadcallback fail
        DINFO_LOGE("tokensync load sa timeout");
        return;
    }
}

sptr<IDeviceInfo> DeviceInfoKits::GetService()
{
    LoadDeviceInfoSa();
    return deviceInfoService_;
}

void DeviceInfoKits::FinishStartSASuccess(const sptr<IRemoteObject> &remoteObject)
{
    DINFO_LOGI("get deviceinfo sa success.");
    // get lock which wait_for release and send a notice so that wait_for can out of block
    std::unique_lock<std::mutex> lock(lock_);
    deviceInfoService_ = iface_cast<IDeviceInfo>(remoteObject);
    deviceInfoLoadCon_.notify_one();
}

void DeviceInfoKits::FinishStartSAFailed()
{
    DINFO_LOGI("get deviceinfo sa failed.");
    // get lock which wait_for release and send a notice
    std::unique_lock<std::mutex> lock(lock_);
    deviceInfoService_ = nullptr;
    deviceInfoLoadCon_.notify_one();
}

int32_t DeviceInfoKits::GetUdid(std::string& result)
{
    auto deviceService = GetService();
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get deviceinfo manager");
    return deviceService->GetUdid(result);
}

int32_t DeviceInfoKits::GetSerialID(std::string& result)
{
    auto deviceService = GetService();
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get deviceinfo manager");
    return deviceService->GetSerialID(result);
}
} // namespace device_info
} // namespace OHOS
