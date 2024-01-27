/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "device_info_load.h"

#include "device_info_kits.h"
#include "system_ability_definition.h"

namespace OHOS {
namespace device_info {
DeviceInfoLoad::DeviceInfoLoad()
{
}

void DeviceInfoLoad::OnLoadSystemAbilitySuccess(int32_t systemAbilityId,
    const sptr<IRemoteObject>& remoteObject)
{
    DINFO_CHECK(systemAbilityId == SYSPARAM_DEVICE_SERVICE_ID, return,
        "start systemabilityId is not deviceinfo! %d", systemAbilityId);
    if (remoteObject == nullptr) {
        DINFO_LOGI("OnLoadSystemAbilitySuccess but remote is null %d", systemAbilityId);
        DeviceInfoKits::GetInstance().FinishStartSAFailed();
        return;
    }

    DINFO_LOGI("OnLoadSystemAbilitySuccess start systemAbilityId: %d success!", systemAbilityId);
    DeviceInfoKits::GetInstance().FinishStartSASuccess(remoteObject);
}

void DeviceInfoLoad::OnLoadSystemAbilityFail(int32_t systemAbilityId)
{
    DINFO_CHECK(systemAbilityId == SYSPARAM_DEVICE_SERVICE_ID, return,
        "start systemabilityId is not deviceinfo! %d", systemAbilityId);

    DINFO_LOGI("OnLoadSystemAbilityFail systemAbilityId: %d failed.", systemAbilityId);

    DeviceInfoKits::GetInstance().FinishStartSAFailed();
}
} // namespace device_info
} // namespace OHOS
