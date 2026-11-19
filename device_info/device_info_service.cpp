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

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <atomic>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

#ifdef INIT_SUPPORT_ACCESS_TOKEN
#include "accesstoken_kit.h"
#include "hap_token_info.h"
#endif
#include "device_info_kits.h"
#include "ipc_skeleton.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "param_comm.h"
#include "parameter.h"
#include "init_param.h"
#include "sysparam_errno.h"
#include "init_utils.h"

#ifdef INIT_SUPPORT_BOARDINFO_HDI
#include "v1_0/iboard_info.h"
using namespace OHOS::HDI::Boardinfo::V1_0;
#endif

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

#ifdef CUSTOM_SANDBOX
// env config is root-only: CallbackEnter rejects non-root callers. Root callers
// derive the per-user path from the calling HAP token (or honor an explicit
// --user override). userId == APPENV_USERID_FROM_TOKEN (-1) means token-derived.
static constexpr size_t ENV_KEY_MAX_LEN = 256;
static constexpr size_t ENV_VALUE_MAX_LEN = 4095;
static constexpr size_t ENV_CONFIG_MAX_ENTRIES = 256;
static constexpr int32_t APPENV_USERID_FROM_TOKEN = -1;
// Valid explicit --user range. Matches OpenHarmony app user IDs (system apps
// start at 100). 0 is root and must be rejected; 1-99 are reserved system uids.
static constexpr int32_t APPENV_USERID_MIN = 100;
static constexpr int32_t APPENV_USERID_MAX = 999;

static bool IsValidEnvKey(const std::string& key)
{
    if (key.empty() || key.size() > ENV_KEY_MAX_LEN) {
        return false;
    }
    if (!isalpha(static_cast<unsigned char>(key[0])) && key[0] != '_') {
        return false;
    }
    for (char c : key) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

static bool IsValidEnvValue(const std::string& value)
{
    if (value.size() > ENV_VALUE_MAX_LEN) {
        return false;
    }
    for (char c : value) {
        if (c == '\n' || c == '\r' || c == '\0') {
            return false;
        }
    }
    return true;
}
#endif // CUSTOM_SANDBOX

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
    DINFO_CHECK(ret == 0, return ret, "failed get dev udid");
    result = std::string(localDeviceInfo);
    return ret;
}

int32_t DeviceInfoService::GetSerialID(std::string& result)
{
    int ret = ERR_FAIL;
    const char *serialNumber = GetSerial_();
    DINFO_CHECK(serialNumber != nullptr, return ret, "failed get serialNumber");
    result = std::string(serialNumber);
    return 0;
}

int32_t DeviceInfoService::GetDiskSN(std::string& result)
{
    int ret = ERR_FAIL;
    char diskSN[DISK_SN_LEN] = {0};
    ret = GetDiskSN_(diskSN, DISK_SN_LEN);
    DINFO_CHECK(ret == 0, return ret, "failed get disk SN");
    result = std::string(diskSN);
    return ret;
}

#ifdef INIT_SUPPORT_BOARDINFO_HDI
static int32_t CallBoardInfoHdi(const char* name,
    int32_t (OHOS::HDI::Boardinfo::V1_0::IBoardInfo::*method)(std::string&),
    std::string& result,
    std::shared_ptr<std::string>& cache,
    std::mutex& mutex)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (cache != nullptr) {
            result = *cache;
            return 0;
        }
    }
    result.clear();
    int32_t hdiRet = -1;
    auto hdi = IBoardInfo::Get(true);
    if (hdi != nullptr) {
        hdiRet = (hdi->*method)(result);
    }
    DINFO_LOGI("boardinfo SA %s hdiRet=%d", name, hdiRet);
    if (hdiRet != 0) {
        return hdiRet;
    }
    std::lock_guard<std::mutex> lock(mutex);
    cache = std::make_shared<std::string>(result);
    return 0;
}
#endif

int32_t DeviceInfoService::GetBoardSerial(std::string& result)
{
    DINFO_LOGI("GetBoardSerial");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetBoardSerial", &IBoardInfo::GetBoardSn, result, boardSerialCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetCpuId(std::string& result)
{
    DINFO_LOGI("GetCpuId");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetCpuId", &IBoardInfo::GetCpuId, result, cpuIdCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetCpuArchitecture(std::string& result)
{
    DINFO_LOGI("GetCpuArchitecture");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetCpuArchitecture", &IBoardInfo::GetCpuArch, result, cpuArchCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetCpuVendor(std::string& result)
{
    DINFO_LOGI("GetCpuVendor");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetCpuVendor", &IBoardInfo::GetCpuVendor, result, cpuVendorCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetBoardVendor(std::string& result)
{
    DINFO_LOGI("GetBoardVendor");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetBoardVendor", &IBoardInfo::GetBoardVendor, result, boardVendorCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetBoardProductName(std::string& result)
{
    DINFO_LOGI("GetBoardProductName");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetBoardProductName", &IBoardInfo::GetBoardName, result, boardProductNameCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetBiosVendor(std::string& result)
{
    DINFO_LOGI("GetBiosVendor");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetBiosVendor", &IBoardInfo::GetBiosVendor, result, biosVendorCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetBiosVersion(std::string& result)
{
    DINFO_LOGI("GetBiosVersion");
#ifdef INIT_SUPPORT_BOARDINFO_HDI
    return CallBoardInfoHdi("GetBiosVersion", &IBoardInfo::GetBiosVersion, result, biosVersionCache_,
        boardInfoMutex_);
#else
    result.clear();
    return 0;
#endif
}

int32_t DeviceInfoService::GetBiosReleaseDate(std::string& result)
{
    DINFO_LOGI("GetBiosReleaseDate");
    {
        std::lock_guard<std::mutex> lock(boardInfoMutex_);
        if (biosReleaseDateCache_ != nullptr) {
            result = *biosReleaseDateCache_;
            return 0;
        }
    }
    result.clear();
#ifdef INIT_SUPPORT_BOARDINFO_HDI
#define BOARD_INFO_PARAM_LEN 256
    char value[BOARD_INFO_PARAM_LEN] = {0};
    uint32_t len = sizeof(value);
    int ret = SystemReadParam("const.build.vendor.date", value, &len);
    if (ret == 0) {
        result = std::string(value);
    }
    DINFO_LOGI("boardinfo SA biosDate paramRet=%d", ret);
    if (ret != 0) {
        return ret;
    }
    std::lock_guard<std::mutex> lock(boardInfoMutex_);
    biosReleaseDateCache_ = std::make_shared<std::string>(result);
    return 0;
#endif
    return 0;
}

#ifdef CUSTOM_SANDBOX
std::string DeviceInfoService::GetEnvConfigPath(int32_t userId)
{
    return "/data/service/el1/" + std::to_string(userId) + "/appenv/appenv.conf";
}

// Resolve per-user userId. When userId == APPENV_USERID_FROM_TOKEN (-1), derive
// from the calling HAP token via GetHapTokenInfo. Returns the resolved userId
// (>=100 valid), 0 (derivation failed or userID==0 — caller rejects with
// SYSPARAM_INVALID_USERID), or SYSPARAM_USERID_OUT_OF_RANGE for explicit --user
// values outside [100,999] (including 0, 1-99, negative, >999).
int32_t DeviceInfoService::ResolveUserId(int32_t userId)
{
    if (userId != APPENV_USERID_FROM_TOKEN) {
        if (userId < APPENV_USERID_MIN || userId > APPENV_USERID_MAX) {
            return SYSPARAM_USERID_OUT_OF_RANGE;
        }
        return userId;
    }
#ifdef INIT_SUPPORT_ACCESS_TOKEN
    auto tokenID = IPCSkeleton::GetCallingFullTokenID();
    HapTokenInfo info;
    int32_t ret = AccessTokenKit::GetHapTokenInfo(tokenID, info);
    if (ret != RET_SUCCESS) {
        DINFO_LOGE("GetHapTokenInfo failed ret=%{public}d tokenID=%{public}u, treat userId as 0",
            ret, tokenID);
        return 0;
    }
    return static_cast<int32_t>(info.userID);
#else
    DINFO_LOGE("INIT_SUPPORT_ACCESS_TOKEN undefined, cannot resolve userId from token");
    return 0;
#endif
}

int32_t DeviceInfoService::LoadEnvConfig(std::map<std::string, std::string>& config, int32_t userId)
{
    std::string path = GetEnvConfigPath(userId);
    // Distinguish "file does not exist" (treat as empty config) from other open
    // failures (EACCES/EIO/SELinux deny etc.) via access(F_OK): ifstream is not
    // required to set errno, and treating a failed read as empty would let the
    // caller's SaveEnvConfig overwrite the file with partial data.
    if (access(path.c_str(), F_OK) != 0) {
        if (errno == ENOENT) {
            DINFO_LOGI("env config file not exist, treat as empty");
            return 0;
        }
        DINFO_LOGE("access %{public}s failed: %{public}s (uid=%{public}d)",
            path.c_str(), strerror(errno), static_cast<int>(getuid()));
        return ERR_FAIL;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        DINFO_LOGE("open %{public}s failed: %{public}s (uid=%{public}d)",
            path.c_str(), strerror(errno), static_cast<int>(getuid()));
        return ERR_FAIL;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        if (!IsValidEnvKey(key) || !IsValidEnvValue(value)) {
            DINFO_LOGW("skip invalid env config line");
            continue;
        }
        config[key] = value;
    }
    return 0;
}

int32_t DeviceInfoService::SaveEnvConfig(const std::map<std::string, std::string>& config, int32_t userId)
{
    // Directory is created by appspawn PRE_FORK root hook before any env set
    // call; SA only writes appenv.conf via tmp+fsync+rename, no mkdir fallback.
    // Same pattern as param_persistadp.c:BatchSavePersistParamEnd — fopen +
    // fputs + fflush + fsync(fileno) + fclose + rename, with explicit fail
    // checks at every IO step to prevent half-written files reaching the
    // rename (rename itself is atomic, but only the *result* is atomic; the
    // tmp file content must be fully written and synced first).
    std::string path = GetEnvConfigPath(userId);
    std::string tmpPath = path + ".tmp";
    FILE *fp = fopen(tmpPath.c_str(), "w");
    if (fp == nullptr) {
        DINFO_LOGE("open %{public}s for write failed: %{public}s (uid=%{public}d)",
            tmpPath.c_str(), strerror(errno), static_cast<int>(getuid()));
        return ERR_FAIL;
    }
    bool writeOk = true;
    for (const auto& [key, value] : config) {
        // key/value have passed IsValidEnvKey/IsValidEnvValue (no \n/\r/\0),
        // so concatenation into one line is safe. Use fputs rather than
        // fprintf("%s=...") to avoid % interpretation in key/value.
        std::string line = key + "=" + value + "\n";
        if (fputs(line.c_str(), fp) == EOF) {
            writeOk = false;
            break;
        }
    }
    if (!writeOk || fflush(fp) != 0) {
        DINFO_LOGE("write/flush %{public}s failed: %{public}s", tmpPath.c_str(), strerror(errno));
        (void)fclose(fp);
        (void)unlink(tmpPath.c_str());
        return ERR_FAIL;
    }
    // fsync before close: fflush only pushes stdio buffer to the OS page cache;
    // fsync forces the data to disk so that a crash after rename cannot leave
    // the new file empty or partially written (the directory entry rename is
    // atomic, but the *data* under it must already be durable).
    (void)fsync(fileno(fp));
    if (fclose(fp) != 0) {
        DINFO_LOGE("fclose %{public}s failed: %{public}s", tmpPath.c_str(), strerror(errno));
        (void)unlink(tmpPath.c_str());
        return ERR_FAIL;
    }
    if (rename(tmpPath.c_str(), path.c_str()) != 0) {
        DINFO_LOGE("rename to %{public}s failed: %{public}d", path.c_str(), errno);
        (void)unlink(tmpPath.c_str());
        return ERR_FAIL;
    }
    return 0;
}

int32_t DeviceInfoService::SetEnvConfig(const std::string& key, const std::string& value, int32_t userId)
{
    DINFO_LOGI("SetEnvConfig key %{public}s userId %{public}d from uid %{public}d pid %{public}d",
        key.c_str(), userId, static_cast<int>(IPCSkeleton::GetCallingUid()),
        static_cast<int>(IPCSkeleton::GetCallingPid()));
    if (!IsValidEnvKey(key) || !IsValidEnvValue(value)) {
        DINFO_LOGE("invalid env key or value format");
        return SYSPARAM_INVALID_INPUT;
    }
    // CallbackEnter already enforced GetCallingUid() == 0; derive the per-user
    // path from the calling token (or honor an explicit --user override).
    int32_t resolvedUserId = ResolveUserId(userId);
    if (resolvedUserId < 0) {
        return resolvedUserId;  // SYSPARAM_USERID_OUT_OF_RANGE
    }
    if (resolvedUserId == 0) {
        DINFO_LOGE("SetEnvConfig rejected: resolved userId is 0 (invalid)");
        return SYSPARAM_INVALID_USERID;
    }
    std::lock_guard<std::mutex> lock(envConfigLock_);
    std::map<std::string, std::string> config;
    int32_t ret = LoadEnvConfig(config, resolvedUserId);
    if (ret != 0) {
        return ret;
    }
    if (config.find(key) == config.end() && config.size() >= ENV_CONFIG_MAX_ENTRIES) {
        DINFO_LOGE("env config entries reach limit %{public}zu", ENV_CONFIG_MAX_ENTRIES);
        return SYSPARAM_ENV_CONFIG_FULL;
    }
    config[key] = value;
    ret = SaveEnvConfig(config, resolvedUserId);
    if (ret == 0) {
        DINFO_LOGI("SetEnvConfig key %{public}s success, total entries %{public}zu",
            key.c_str(), config.size());
    }
    return ret;
}

int32_t DeviceInfoService::GetEnvConfig(const std::string& key, std::string& value, int32_t userId)
{
    DINFO_LOGI("GetEnvConfig key %{public}s userId %{public}d from uid %{public}d pid %{public}d",
        key.c_str(), userId, static_cast<int>(IPCSkeleton::GetCallingUid()),
        static_cast<int>(IPCSkeleton::GetCallingPid()));
    if (key.empty()) {
        return SYSPARAM_INVALID_INPUT;
    }
    int32_t resolvedUserId = ResolveUserId(userId);
    if (resolvedUserId < 0) {
        return resolvedUserId;  // SYSPARAM_USERID_OUT_OF_RANGE
    }
    if (resolvedUserId == 0) {
        DINFO_LOGE("GetEnvConfig rejected: resolved userId is 0 (invalid)");
        return SYSPARAM_INVALID_USERID;
    }
    std::lock_guard<std::mutex> lock(envConfigLock_);
    std::map<std::string, std::string> config;
    int32_t ret = LoadEnvConfig(config, resolvedUserId);
    if (ret != 0) {
        return ret;
    }
    auto it = config.find(key);
    if (it == config.end()) {
        DINFO_LOGE("key %{public}s not found", key.c_str());
        return SYSPARAM_NOT_FOUND;
    }
    value = it->second;
    return 0;
}

int32_t DeviceInfoService::RemoveEnvConfig(const std::string& key, int32_t userId)
{
    DINFO_LOGI("RemoveEnvConfig key %{public}s userId %{public}d from uid %{public}d pid %{public}d",
        key.c_str(), userId, static_cast<int>(IPCSkeleton::GetCallingUid()),
        static_cast<int>(IPCSkeleton::GetCallingPid()));
    if (key.empty()) {
        return SYSPARAM_INVALID_INPUT;
    }
    int32_t resolvedUserId = ResolveUserId(userId);
    if (resolvedUserId < 0) {
        return resolvedUserId;  // SYSPARAM_USERID_OUT_OF_RANGE
    }
    if (resolvedUserId == 0) {
        DINFO_LOGE("RemoveEnvConfig rejected: resolved userId is 0 (invalid)");
        return SYSPARAM_INVALID_USERID;
    }
    std::lock_guard<std::mutex> lock(envConfigLock_);
    std::map<std::string, std::string> config;
    int32_t ret = LoadEnvConfig(config, resolvedUserId);
    if (ret != 0) {
        return ret;
    }
    auto it = config.find(key);
    if (it == config.end()) {
        DINFO_LOGI("key %{public}s not exist, return NOT_FOUND", key.c_str());
        return SYSPARAM_NOT_FOUND;
    }
    config.erase(it);
    ret = SaveEnvConfig(config, resolvedUserId);
    if (ret == 0) {
        DINFO_LOGI("RemoveEnvConfig key %{public}s success, total entries %{public}zu",
            key.c_str(), config.size());
    }
    return ret;
}
#else // !CUSTOM_SANDBOX
// Non-PC stubs: IDL-generated base class declares these as pure virtual, so
// overrides must exist to make DeviceInfoService instantiable. Never reached at
// runtime (appenv CLI is not built on non-PC).
int32_t DeviceInfoService::SetEnvConfig(const std::string& key, const std::string& value, int32_t userId)
{
    return SYSPARAM_PERMISSION_DENIED;
}
int32_t DeviceInfoService::GetEnvConfig(const std::string& key, std::string& value, int32_t userId)
{
    return SYSPARAM_PERMISSION_DENIED;
}
int32_t DeviceInfoService::RemoveEnvConfig(const std::string& key, int32_t userId)
{
    return SYSPARAM_PERMISSION_DENIED;
}
#endif // CUSTOM_SANDBOX

// Env config commands (SET/GET/REMOVE_ENV_CONFIG) require a root caller. On
// non-PC products CUSTOM_SANDBOX is undefined and the env code path is stubbed
// out, so reject up front. On PC products, enforce GetCallingUid() == 0.
// Returns 0 on pass, SYSPARAM_PERMISSION_DENIED on reject.
static int32_t CheckEnvRootPermission()
{
#ifdef CUSTOM_SANDBOX
    uid_t callingUid = IPCSkeleton::GetCallingUid();
    if (callingUid != 0) {
        DINFO_LOGE("env config rejected: calling uid %{public}d is not root", callingUid);
        return SYSPARAM_PERMISSION_DENIED;
    }
    return 0;
#else
    DINFO_LOGE("env config rejected: CUSTOM_SANDBOX not enabled on this product");
    return SYSPARAM_PERMISSION_DENIED;
#endif
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
            bool ret = CheckPermission("ohos.permission.sec.ACCESS_UDID");
            BEGET_CHECK_RETURN_VALUE(ret, SYSPARAM_PERMISSION_DENIED);
            break;
        }
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_DISK_S_N): {
            bool ret = CheckPermission("ohos.permission.ACCESS_DISK_PHY_INFO");
            BEGET_CHECK_RETURN_VALUE(ret, SYSPARAM_PERMISSION_DENIED);
            break;
        }
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_BOARD_SERIAL): {
            bool ret = CheckPermission("ohos.permission.ACCESS_BOARD_INFO");
            BEGET_CHECK_RETURN_VALUE(ret, SYSPARAM_PERMISSION_DENIED);
            break;
        }
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_CPU_ID):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_CPU_ARCHITECTURE):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_CPU_VENDOR):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_BOARD_VENDOR):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_BOARD_PRODUCT_NAME):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_BIOS_VENDOR):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_BIOS_VERSION):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_BIOS_RELEASE_DATE): {
            break;
        }
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_SET_ENV_CONFIG):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_REMOVE_ENV_CONFIG):
        case static_cast<uint32_t>(IDeviceInfoIpcCode::COMMAND_GET_ENV_CONFIG): {
            int32_t ret = CheckEnvRootPermission();
            BEGET_CHECK_RETURN_VALUE(ret == 0, ret);
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
    ClearBoardInfoCache();
    DINFO_LOGI("DeviceInfoService OnStop");
}

void DeviceInfoService::ClearBoardInfoCache()
{
    std::lock_guard<std::mutex> lock(boardInfoMutex_);
    boardSerialCache_.reset();
    cpuIdCache_.reset();
    cpuArchCache_.reset();
    cpuVendorCache_.reset();
    boardVendorCache_.reset();
    boardProductNameCache_.reset();
    biosVendorCache_.reset();
    biosVersionCache_.reset();
    biosReleaseDateCache_.reset();
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