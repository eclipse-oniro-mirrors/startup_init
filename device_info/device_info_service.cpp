/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "device_info_service.h"

#include <thread>

#ifdef INIT_SUPPORT_ACCESS_TOKEN
#include "accesstoken_kit.h"
#endif
#include "device_info_kits.h"
#include "ipc_skeleton.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "param_comm.h"
#include "parameter.h"
#include "sysparam_errno.h"
#include "init_utils.h"

namespace OHOS {
#ifdef INIT_SUPPORT_ACCESS_TOKEN
using namespace Security;
using namespace Security::AccessToken;
#endif

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

int32_t DeviceInfoService::GetUdid(std::string& result)
{
    int ret = ERR_FAIL;
    char localDeviceInfo[UDID_LEN] = {0};
    ret = GetDevUdid_(localDeviceInfo, UDID_LEN);
    DINFO_CHECK(ret == 0, return ret, "Failed to get dev udid");
    result = std::string(localDeviceInfo);
    return ret;
}

int32_t DeviceInfoService::GetSerialID(std::string& result)
{
    int ret = ERR_FAIL;
    const char *serialNumber = GetSerial_();
    DINFO_CHECK(serialNumber != nullptr, return ret, "Failed to get serialNumber");
    result = std::string(serialNumber);
    return 0;
}

int32_t DeviceInfoService::GetDiskSN(std::string& result)
{
    int ret = ERR_FAIL;
    char diskSN[DISK_SN_LEN] = {0};
    ret = GetDiskSN_(diskSN, DISK_SN_LEN);
    DINFO_CHECK(ret == 0, return ret, "Failed to get disk SN");
    result = std::string(diskSN);
    return ret;
}

int32_t DeviceInfoService::CallbackEnter(uint32_t code)
{
    {
        std::unique_lock<std::mutex> lock(g_lock);
        (void)clock_gettime(CLOCK_MONOTONIC, &g_lastTime);
    }
    switch (code) {
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_UDID):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_SERIAL_I_D): {
            if (!CheckPermission("ohos.permission.sec.ACCESS_UDID")) {
                return SYSPARAM_PERMISSION_DENIED;
            }
            break;
        }
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_DISK_S_N): {
            if (!CheckPermission("ohos.permission.ACCESS_DISK_PHY_INFO")) {
                return SYSPARAM_PERMISSION_DENIED;
            }
            break;
        }
        default: {
            return SYSPARAM_PERMISSION_DENIED;
        }
    }
    return 0;
}

int32_t DeviceInfoService::CallbackExit(uint32_t code, int32_t result)
{
    return 0;
}

bool DeviceInfoService::CheckPermission(const std::string &permission)
{
#ifdef INIT_SUPPORT_ACCESS_TOKEN
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
        DINFO_LOGE("permission:%s denied!", permission.c_str());
        return false;
    }
    return true;
#else
    DINFO_LOGE("CheckPermission is not supported");
    return false;
#endif
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
    std::thread([this] {this->ThreadForUnloadSa();}).detach();
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
    while (threadStarted_) {
        std::this_thread::sleep_for(std::chrono::seconds(DEVICE_INFO_EXIT_TIMEOUT_S / DEVICE_INFO_EXIT_WAITTIMES));
        if (UnloadDeviceInfoSa() == 1) {
            break;
        }
    }
}
} // namespace device_info
} // namespace OHOS