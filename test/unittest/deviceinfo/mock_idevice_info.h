/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef DEVICE_INFO_MOCK_IDEVICE_INFO_H
#define DEVICE_INFO_MOCK_IDEVICE_INFO_H

#include <gtest/gtest.h>
#include <string>

#include "idevice_info.h"
#include "refbase.h"

namespace init_ut {

// Minimal mock of IDeviceInfo (IDL-generated IRemoteBroker subclass). Used by
// device_info_kits_env_unittest.cpp to drive the NOT_FOUND_SERVICE_ERROR_NUMBER
// branches in DeviceInfoKits::SetEnvConfig/GetEnvConfig/RemoveEnvConfig
// (device_info_kits.cpp L206/L221/L236).
//
// The mock is injected via DeviceInfoKits::deviceInfoService_ (a private member
// made accessible by the target's -Dprivate=public cflag). SetEnvConfig /
// GetEnvConfig / RemoveEnvConfig return a configurable code (default
// NOT_FOUND_SERVICE_ERROR_NUMBER = 29189) so the `if (ret == NOT_FOUND...)`
// branch is taken; AsObject returns nullptr so RetryGetService's
// `deviceService->AsObject()` yields nullptr, which makes the deathRecipient_
// block a no-op (deathRecipient_ is nullptr by default) and lets GetService
// return the mock again (LoadDeviceInfoSa's fast path: deviceInfoService_ !=
// nullptr → return immediately). The retry call then returns the same code,
// exercising L207-L212 fully.
//
// The other 12 IDL methods are stubbed to return 0 (they are not exercised by
// the env tests but must be implemented because IDeviceInfo is abstract).
class MockIDeviceInfo : public OHOS::device_info::IDeviceInfo {
public:
    MockIDeviceInfo() = default;
    ~MockIDeviceInfo() override = default;

    // Settable return code for the three env methods. Defaults to
    // NOT_FOUND_SERVICE_ERROR_NUMBER (29189) to trigger the retry branch.
    int envRet = 29189;  // NOT_FOUND_SERVICE_ERROR_NUMBER

    OHOS::ErrCode SetEnvConfig(const std::string& key, const std::string& value, int32_t userId) override
    {
        (void)key;
        (void)value;
        (void)userId;
        return static_cast<OHOS::ErrCode>(envRet);
    }

    OHOS::ErrCode GetEnvConfig(const std::string& key, std::string& value, int32_t userId) override
    {
        (void)key;
        (void)userId;
        value = "";
        return static_cast<OHOS::ErrCode>(envRet);
    }

    OHOS::ErrCode RemoveEnvConfig(const std::string& key, int32_t userId) override
    {
        (void)key;
        (void)userId;
        return static_cast<OHOS::ErrCode>(envRet);
    }

    // AsObject: returns nullptr so RetryGetService's AsObject() call yields
    // nullptr, making the deathRecipient_ block a no-op and avoiding any real
    // IRemoteObject interaction.
    OHOS::sptr<OHOS::IRemoteObject> AsObject() override
    {
        return nullptr;
    }

    // Stubs for the other 12 IDL methods (not exercised here, but IDeviceInfo
    // is abstract and requires all pure-virtuals implemented).
    OHOS::ErrCode GetUdid(std::string& udid) override
    {
        (void)udid;
        return 0;
    }
    OHOS::ErrCode GetSerialID(std::string& serialid) override
    {
        (void)serialid;
        return 0;
    }
    OHOS::ErrCode GetDiskSN(std::string& disksn) override
    {
        (void)disksn;
        return 0;
    }
    OHOS::ErrCode GetCpuId(std::string& cpuid) override
    {
        (void)cpuid;
        return 0;
    }
    OHOS::ErrCode GetCpuArchitecture(std::string& cpuarchitecture) override
    {
        (void)cpuarchitecture;
        return 0;
    }
    OHOS::ErrCode GetCpuVendor(std::string& cpuvendor) override
    {
        (void)cpuvendor;
        return 0;
    }
    OHOS::ErrCode GetBoardSerial(std::string& boardserial) override
    {
        (void)boardserial;
        return 0;
    }
    OHOS::ErrCode GetBoardVendor(std::string& boardvendor) override
    {
        (void)boardvendor;
        return 0;
    }
    OHOS::ErrCode GetBoardProductName(std::string& boardproductname) override
    {
        (void)boardproductname;
        return 0;
    }
    OHOS::ErrCode GetBiosVendor(std::string& biosvendor) override
    {
        (void)biosvendor;
        return 0;
    }
    OHOS::ErrCode GetBiosVersion(std::string& biosversion) override
    {
        (void)biosversion;
        return 0;
    }
    OHOS::ErrCode GetBiosReleaseDate(std::string& biosreleasedate) override
    {
        (void)biosreleasedate;
        return 0;
    }
};

}  // namespace init_ut

#endif  // DEVICE_INFO_MOCK_IDEVICE_INFO_H
