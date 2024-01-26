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

#include <chrono>
#include <thread>

#include "beget_ext.h"
#include "idevice_info.h"
#include "ipc_skeleton.h"
#include "accesstoken_kit.h"
#include "parcel.h"
#include "string_ex.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "param_comm.h"
#include "parameter.h"
#include "sysparam_errno.h"
#include "init_utils.h"
#include "deviceinfoservice_ipc_interface_code.h"

namespace OHOS {
using namespace Security;
using namespace Security::AccessToken;

namespace device_info {
REGISTER_SYSTEM_ABILITY_BY_ID(DeviceInfoService, SYSPARAM_DEVICE_SERVICE_ID, true)

static std::mutex g_lock;
static struct timespec g_lastTime;
#ifndef STARTUP_INIT_TEST
static const int DEVICE_INFO_EXIT_TIMEOUT_S = 60;
#else
static const int DEVICE_INFO_EXIT_TIMEOUT_S = 3;
#endif
static const int DEVICE_INFO_EXIT_WAITTIMES = 12;


static int UnloadDeviceInfoSa(void)
{
    {
        std::unique_lock<std::mutex> lock(g_lock);
        struct timespec currTimer = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &currTimer);
        if (IntervalTime(&g_lastTime, &currTimer) < DEVICE_INFO_EXIT_TIMEOUT_S) {
            return 0;
        }
    }
    DINFO_LOGI("DeviceInfoService::UnloadDeviceInfoSa");
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    DINFO_CHECK(sam != nullptr, return 0, "GetSystemAbilityManager return null");

    int32_t ret = sam->UnloadSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    DINFO_CHECK(ret == ERR_OK, return 0, "UnLoadSystemAbility deviceinfo sa failed");
    return 1;
}

int32_t DeviceInfoStub::OnRemoteRequest(uint32_t code,
    MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    std::u16string myDescriptor = IDeviceInfo::GetDescriptor();
    std::u16string remoteDescriptor = data.ReadInterfaceToken();
    DINFO_CHECK(myDescriptor == remoteDescriptor, return ERR_FAIL, "Invalid remoteDescriptor");

    {
        std::unique_lock<std::mutex> lock(g_lock);
        (void)clock_gettime(CLOCK_MONOTONIC, &g_lastTime);
    }

    int ret = ERR_FAIL;
    switch (code) {
        case static_cast<uint32_t> (DeviceInfoInterfaceCode::COMMAND_GET_UDID): {
            if (!CheckPermission(data, "ohos.permission.sec.ACCESS_UDID")) {
                return SYSPARAM_PERMISSION_DENIED;
            }
            char localDeviceInfo[UDID_LEN] = {0};
            ret = GetDevUdid_(localDeviceInfo, UDID_LEN);
            DINFO_CHECK(ret == 0, break, "Failed to get dev udid");
            reply.WriteString16(Str8ToStr16(localDeviceInfo));
            break;
        }
        case static_cast<uint32_t> (DeviceInfoInterfaceCode::COMMAND_GET_SERIAL_ID): {
            if (!CheckPermission(data, "ohos.permission.sec.ACCESS_UDID")) {
                return SYSPARAM_PERMISSION_DENIED;
            }
            const char *serialNumber = GetSerial_();
            DINFO_CHECK(serialNumber != nullptr, break, "Failed to get serialNumber");
            reply.WriteString16(Str8ToStr16(serialNumber));
            ret = ERR_NONE;
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
    if (tokenType == TOKEN_INVALID) {
        DINFO_LOGE("AccessToken type:%d, permission:%d denied!", tokenType, callerToken);
        return false;
    } else {
        result = AccessTokenKit::VerifyAccessToken(callerToken, permission);
    }
    if (result == TypePermissionState::PERMISSION_DENIED) {
        DINFO_LOGE("AccessTokenID:%d, permission:%s denied!", callerToken, permission.c_str());
        return false;
    }
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
    int level = GetIntParameter(INIT_DEBUG_LEVEL, (int)INIT_INFO);
    SetInitLogLevel((InitLogLevel)level);
    DINFO_LOGI("DeviceInfoService OnStart");
    bool res = Publish(this);
    if (!res) {
        DINFO_LOGE("DeviceInfoService Publish failed");
    }
    threadStarted_ = true;
    std::thread(&DeviceInfoService::ThreadForUnloadSa, this).detach();
    return;
}

void DeviceInfoService::OnStop(void)
{
    threadStarted_ = false;
    DINFO_LOGI("DeviceInfoService OnStop");
}

int DeviceInfoService::Dump(int fd, const std::vector<std::u16string>& args)
{
    (void)args;
    DINFO_LOGI("DeviceInfoService Dump");
    DINFO_CHECK(fd >= 0, return -1, "Invalid fd for dump %d", fd);
    return dprintf(fd, "%s\n", "No information to dump for this service");
}

void DeviceInfoService::ThreadForUnloadSa(void)
{
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(DEVICE_INFO_EXIT_TIMEOUT_S / DEVICE_INFO_EXIT_WAITTIMES));
        if (!threadStarted_) {
            break;
        }
        if (UnloadDeviceInfoSa() == 1) {
            break;
        }
    }
}
} // namespace device_info
} // namespace OHOS
