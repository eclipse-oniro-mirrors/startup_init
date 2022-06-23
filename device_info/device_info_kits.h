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

namespace OHOS {
namespace device_info {
class DeviceInfoKits final : public DelayedRefSingleton<DeviceInfoKits> {
    DECLARE_DELAYED_REF_SINGLETON(DeviceInfoKits);
public:
    DISALLOW_COPY_AND_MOVE(DeviceInfoKits);

    static DeviceInfoKits &GetInstance();
    int32_t GetUdid(std::string& result);
    int32_t GetSerialID(std::string& result);

    void FinishStartSASuccess(const sptr<IRemoteObject> &remoteObject);
    void FinishStartSAFailed();
private:
    static const int DEVICEINFO_LOAD_SA_TIMEOUT_MS = 60000;
    void LoadDeviceInfoSa();
    sptr<IDeviceInfo> GetService();
    std::mutex lock_;
    std::condition_variable deviceInfoLoadCon_;
    sptr<IDeviceInfo> deviceInfoService_ {};
};
} // namespace device_info
} // namespace OHOS
#endif // OHOS_SYSTEM_DEVICE_ID_KITS_S
