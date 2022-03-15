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

#include "device_info_stub.h"

#include "beget_ext.h"
#include "idevice_info.h"
#include "ipc_skeleton.h"
#include "accesstoken_kit.h"
#include "parameter_hal.h"
#include "parcel.h"
#include "string_ex.h"
#include "system_ability_definition.h"

using std::u16string;
namespace {
static constexpr int UDID_LEN = 65;
} // namespace name

namespace OHOS {
using namespace Security;
using namespace Security::AccessToken;

namespace device_info {
REGISTER_SYSTEM_ABILITY_BY_ID(DeviceInfoService, SYSPARAM_DEVICE_SERVICE_ID, true)

int32_t DeviceInfoStub::OnRemoteRequest(uint32_t code,
    MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    std::u16string myDescripter = IDeviceInfo::GetDescriptor();
    std::u16string remoteDescripter = data.ReadInterfaceToken();
    DINFO_CHECK(myDescripter == remoteDescripter, return ERR_INVALD_DESC, "Invalid remoteDescripter");

    int ret = ERR_FAIL;
    switch (code) {
        case COMMAND_GET_UDID: {
            if (!CheckPermission(data, PERMISSION_UDID)) {
                return ERR_FAIL;
            }
            char localDeviceInfo[UDID_LEN] = {0};
            ret = HalGetDevUdid(localDeviceInfo, UDID_LEN);
            DINFO_CHECK(ret == 0, break, "Failed to get dev udid");
            reply.WriteString16(Str8ToStr16(localDeviceInfo));
            break;
        }
        case COMMAND_GET_SERIAL_ID: {
            if (!CheckPermission(data, PERMISSION_UDID)) {
                return ERR_FAIL;
            }
            const char *serialNumber = HalGetSerial();
            DINFO_CHECK(serialNumber != nullptr, break, "Failed to get serialNumber");
            reply.WriteString16(Str8ToStr16(serialNumber));
            break;
        }
        default: {
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
    return ret;
}

bool DeviceInfoStub::CheckPermission(MessageParcel &data, const std::string &permission)
{
    AccessTokenID callerToken = IPCSkeleton::GetCallingTokenID();
    int32_t result = TypePermissionState::PERMISSION_GRANTED;
    int32_t tokenType = AccessTokenKit::GetTokenTypeFlag(callerToken);
    if (tokenType == TOKEN_NATIVE) {
        result = AccessTokenKit::VerifyNativeToken(callerToken, permission);
    } else if (tokenType == TOKEN_HAP) {
        result = AccessTokenKit::VerifyAccessToken(callerToken, permission);
    } else {
        DINFO_LOGE("AccessToken type:%d, permission:%d denied!", tokenType, callerToken);
        return false;
    }
    if (result == TypePermissionState::PERMISSION_DENIED) {
        DINFO_LOGE("AccessTokenID:%d, permission:%s denied!", callerToken, permission.c_str());
        return false;
    }
    DINFO_LOGI("tokenType %d dAccessTokenID:%d, permission:%s matched!", tokenType, callerToken, permission.c_str());
    return true;
}

int32_t DeviceInfoService::GetUdid(std::string& result)
{
    return 0;
}
int32_t DeviceInfoService::GetSerialID(std::string& result)
{
    return 0;
}

void DeviceInfoService::OnStart(void)
{
    DINFO_LOGI("WatcherManager OnStart");
    bool res = Publish(this);
    if (!res) {
        DINFO_LOGE("WatcherManager Publish failed");
    }
    return;
}
void DeviceInfoService::OnStop()
{
}
} // namespace device_info
} // namespace OHOS
