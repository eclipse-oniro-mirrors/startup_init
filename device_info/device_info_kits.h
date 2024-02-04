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

#ifndef OHOS_SYSTEM_DEVICE_ID_KITS_S
#define OHOS_SYSTEM_DEVICE_ID_KITS_S
#include <mutex>
#include "idevice_info.h"
#include "singleton.h"
#include "beget_ext.h"

namespace OHOS {
namespace device_info {
class INIT_LOCAL_API DeviceInfoKits final : public DelayedRefSingleton<DeviceInfoKits> {
    DECLARE_DELAYED_REF_SINGLETON(DeviceInfoKits);
public:
    DISALLOW_COPY_AND_MOVE(DeviceInfoKits);

    static DeviceInfoKits &GetInstance();
    int32_t GetUdid(std::string& result);
    int32_t GetSerialID(std::string& result);

    void FinishStartSASuccess(const sptr<IRemoteObject> &remoteObject);
    void FinishStartSAFailed();
    void ResetService(const wptr<IRemoteObject> &remote);
private:
    // For death event procession
    class DeathRecipient final : public IRemoteObject::DeathRecipient {
    public:
        DeathRecipient(void) = default;
        ~DeathRecipient(void) final = default;
        DISALLOW_COPY_AND_MOVE(DeathRecipient);
        void OnRemoteDied(const wptr<IRemoteObject> &remote) final;
    };
    sptr<IRemoteObject::DeathRecipient> GetDeathRecipient(void)
    {
        return deathRecipient_;
    }

    static const int DEVICEINFO_LOAD_SA_TIMEOUT_MS = 5000;
    void LoadDeviceInfoSa();
    sptr<IDeviceInfo> GetService();
    std::mutex lock_;
    std::condition_variable deviceInfoLoadCon_;
    sptr<IRemoteObject::DeathRecipient> deathRecipient_ {};
    sptr<IDeviceInfo> deviceInfoService_ {};
};
} // namespace device_info
} // namespace OHOS
#endif // OHOS_SYSTEM_DEVICE_ID_KITS_S
