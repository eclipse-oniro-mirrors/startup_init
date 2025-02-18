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
#include "sysparam_errno.h"

namespace OHOS {
namespace device_info {

static const int DEVINFO_SAID = 3902;
static const int DEVICEINFO_LOAD_SA_TIMEOUT_MS = 5000;

DeviceInfoKits::DeviceInfoKits() {}

DeviceInfoKits::~DeviceInfoKits() {}

DeviceInfoKits &DeviceInfoKits::GetInstance()
{
    return DelayedRefSingleton<DeviceInfoKits>::GetInstance();
}

void DeviceInfoKits::LoadDeviceInfoSa(std::unique_lock<std::mutex> &lock)
{
    DINFO_LOGV("deviceInfoService_ is %d", deviceInfoService_ == nullptr);
    if (deviceInfoService_ != nullptr) {
        return;
    }
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    DINFO_CHECK(sam != nullptr, return, "GetSystemAbilityManager return null");

    auto remoteObject = sam->CheckSystemAbility(DEVINFO_SAID);
    if (remoteObject) {
        DINFO_LOGV("deviceinfo sa is already loaded");
        deviceInfoService_ = iface_cast<IDeviceInfo>(remoteObject);
        return;
    }
    sptr<DeviceInfoLoad> deviceInfoLoad = new (std::nothrow) DeviceInfoLoad();
    DINFO_CHECK(deviceInfoLoad != nullptr, return, "new deviceInfoLoad fail.");

    int32_t ret = sam->LoadSystemAbility(SYSPARAM_DEVICE_SERVICE_ID, deviceInfoLoad);
    DINFO_CHECK(ret == ERR_OK, return, "LoadSystemAbility deviceinfo sa failed");

    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new DeathRecipient();
    }

    // wait_for release lock and block until time out(60s) or match the condition with notice
    auto waitStatus = deviceInfoLoadCon_.wait_for(lock, std::chrono::milliseconds(DEVICEINFO_LOAD_SA_TIMEOUT_MS),
        [this]() { return deviceInfoService_ != nullptr; });
    if (!waitStatus || deviceInfoService_ == nullptr) {
        // time out or loadcallback fail
        DINFO_LOGE("tokensync load sa timeout");
        return;
    }

    // for dead
    auto object = deviceInfoService_->AsObject();
    if ((object->IsProxyObject()) && (!object->AddDeathRecipient(deathRecipient_))) {
        DINFO_LOGE("Failed to add death recipient");
    }
}

sptr<IDeviceInfo> DeviceInfoKits::GetService(std::unique_lock<std::mutex> &lock)
{
    LoadDeviceInfoSa(lock);
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
    DINFO_LOGI("Get deviceinfo sa failed.");
    // get lock which wait_for release and send a notice
    std::unique_lock<std::mutex> lock(lock_);
    deviceInfoService_ = nullptr;
    deviceInfoLoadCon_.notify_one();
}

int32_t DeviceInfoKits::GetUdid(std::string& result)
{
    std::unique_lock<std::mutex> lock(lock_);
    static std::optional<std::pair<int32_t, std::string>> resultPair;
    if (resultPair.has_value()) {
        int32_t ret = resultPair->first;
        result = resultPair->second;
        DINFO_LOGV("GetUdid from resultPair ret = %d", ret);
        return ret;
    }
    auto deviceService = GetService(lock);
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get deviceinfo manager");
    int ret = deviceService->GetUdid(result);
    DINFO_LOGV("GetSerialID from remote ret = %d", ret);
    if (ret == 0 || ret == SYSPARAM_PERMISSION_DENIED) {
        resultPair = std::make_optional(std::make_pair(ret, result));
    }
    return ret;
}

int32_t DeviceInfoKits::GetSerialID(std::string& result)
{
    std::unique_lock<std::mutex> lock(lock_);
    static std::optional<std::pair<int32_t, std::string>> resultPair;
    if (resultPair.has_value()) {
        int32_t ret = resultPair->first;
        result = resultPair->second;
        DINFO_LOGV("GetSerialID from resultPair ret = %d", ret);
        return ret;
    }
    auto deviceService = GetService(lock);
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get deviceinfo manager");
    int ret = deviceService->GetSerialID(result);
    DINFO_LOGV("GetSerialID from remote ret = %d", ret);
    if (ret == 0 || ret == SYSPARAM_PERMISSION_DENIED) {
        resultPair = std::make_optional(std::make_pair(ret, result));
    }
    return ret;
}

int32_t DeviceInfoKits::GetDiskSN(std::string& result)
{
    std::unique_lock<std::mutex> lock(lock_);
    static std::optional<std::pair<int32_t, std::string>> resultPair;
    if (resultPair.has_value()) {
        int32_t ret = resultPair->first;
        result = resultPair->second;
        DINFO_LOGV("GetDiskSN from resultPair ret = %d", ret);
        return ret;
    }
    auto deviceService = GetService(lock);
    DINFO_CHECK(deviceService != nullptr, return -1, "Failed to get deviceinfo manager");
    int ret = deviceService->GetDiskSN(result);
    DINFO_LOGV("GetDiskSN from remote ret = %d", ret);
    if (ret == 0 || ret == SYSPARAM_PERMISSION_DENIED) {
        resultPair = std::make_optional(std::make_pair(ret, result));
    }
    return ret;
}

void DeviceInfoKits::DeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    DelayedRefSingleton<DeviceInfoKits>::GetInstance().ResetService(remote);
}

void DeviceInfoKits::ResetService(const wptr<IRemoteObject> &remote)
{
    DINFO_LOGI("Remote is dead, reset service instance");
    std::lock_guard<std::mutex> lock(lock_);
    if (deviceInfoService_ != nullptr) {
        sptr<IRemoteObject> object = deviceInfoService_->AsObject();
        if ((object != nullptr) && (remote == object)) {
            object->RemoveDeathRecipient(deathRecipient_);
            deviceInfoService_ = nullptr;
        }
    }
}
} // namespace device_info
} // namespace OHOS
