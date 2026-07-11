/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <string>

#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "napi/native_common.h"
#include "parameter.h"
#include "systemcapability.h"
#include "sysversion.h"
#ifdef DEPENDENT_APPEXECFWK_BASE
#include "bundlemgr/bundle_mgr_proxy.h"
#endif
#include "iservice_registry.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
#include "beget_ext.h"
#include "init_error.h"
#include "securec.h"

#ifndef DEVICEINFO_JS_DOMAIN
#define DEVICEINFO_JS_DOMAIN (BASE_DOMAIN + 8)
#endif

#ifndef DINFO_TAG
#define DINFO_TAG "DEVICEINFO_JS"
#endif

#define DEVINFO_LOGV(fmt, ...) STARTUP_LOGV(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)
#define DEVINFO_LOGI(fmt, ...) STARTUP_LOGI(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)
#define DEVINFO_LOGW(fmt, ...) STARTUP_LOGW(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)
#define DEVINFO_LOGE(fmt, ...) STARTUP_LOGE(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)

const int UDID_LEN = 65;
const int ODID_LEN = 37;
constexpr int DISK_SN_LEN = 65;

typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;

typedef enum {
    CLASS_LEVEL_HIGH,
    CLASS_LEVEL_MEDIUM,
    CLASS_LEVEL_LOW
} PerformanceClassLevel;

static napi_value GetDeviceType(napi_env env, napi_callback_info info)
{
    napi_value deviceType = nullptr;
    const char *value = GetDeviceType();
    if (value == nullptr) {
        value = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, value, strlen(value), &deviceType));
    return deviceType;
}

static napi_value GetManufacture(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *manfactureName = GetManufacture();
    if (manfactureName == nullptr) {
        manfactureName = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, manfactureName, strlen(manfactureName), &napiValue));
    return napiValue;
}

static napi_value GetBrand(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *productBrand = GetBrand();
    if (productBrand == nullptr) {
        productBrand = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, productBrand, strlen(productBrand), &napiValue));
    return napiValue;
}

static napi_value GetMarketName(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *marketName = GetMarketName();
    if (marketName == nullptr) {
        marketName = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, marketName, strlen(marketName), &napiValue));
    return napiValue;
}

static napi_value GetProductSeries(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *productSeries = GetProductSeries();
    if (productSeries == nullptr) {
        productSeries = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, productSeries, strlen(productSeries), &napiValue));
    return napiValue;
}

static napi_value GetProductModel(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *productModel = GetProductModel();
    if (productModel == nullptr) {
        productModel = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, productModel, strlen(productModel), &napiValue));
    return napiValue;
}

static napi_value GetProductModelAlias(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *productModel = GetProductModelAlias();
    if (productModel == nullptr) {
        productModel = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, productModel, strlen(productModel), &napiValue));
    return napiValue;
}

static napi_value GetSoftwareModel(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *softwareModel = GetSoftwareModel();
    if (softwareModel == nullptr) {
        softwareModel = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, softwareModel, strlen(softwareModel), &napiValue));
    return napiValue;
}

static napi_value GetHardwareModel(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *hardwareModel = GetHardwareModel();
    if (hardwareModel == nullptr) {
        hardwareModel = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, hardwareModel, strlen(hardwareModel), &napiValue));
    return napiValue;
}

static napi_value GetHardwareProfile(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *hardwareProfile = GetHardwareProfile();
    if (hardwareProfile == nullptr) {
        hardwareProfile = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, hardwareProfile, strlen(hardwareProfile), &napiValue));
    return napiValue;
}

static napi_value GetSerial(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *serialNumber = AclGetSerial();
    if (serialNumber == nullptr) {
        serialNumber = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, serialNumber, strlen(serialNumber), &napiValue));
    return napiValue;
}

static napi_value GetBootloaderVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *bootloaderVersion = GetBootloaderVersion();
    if (bootloaderVersion == nullptr) {
        bootloaderVersion = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, bootloaderVersion, strlen(bootloaderVersion), &napiValue));
    return napiValue;
}

static napi_value GetAbiList(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *abiList = GetAbiList();
    if (abiList == nullptr) {
        abiList = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, abiList, strlen(abiList), &napiValue));
    return napiValue;
}

static napi_value GetSecurityPatchTag(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *securityPatchTag = GetSecurityPatchTag();
    if (securityPatchTag == nullptr) {
        securityPatchTag = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, securityPatchTag, strlen(securityPatchTag), &napiValue));
    return napiValue;
}

static napi_value GetDisplayVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *productVersion = GetDisplayVersion();
    if (productVersion == nullptr) {
        productVersion = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, productVersion, strlen(productVersion), &napiValue));
    return napiValue;
}

static napi_value GetIncrementalVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *incrementalVersion = GetIncrementalVersion();
    if (incrementalVersion == nullptr) {
        incrementalVersion = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, incrementalVersion, strlen(incrementalVersion), &napiValue));
    return napiValue;
}

static napi_value GetOsReleaseType(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *osReleaseType = GetOsReleaseType();
    if (osReleaseType == nullptr) {
        osReleaseType = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, osReleaseType, strlen(osReleaseType), &napiValue));
    return napiValue;
}

static napi_value GetOSFullName(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *osVersion = GetOSFullName();
    if (osVersion == nullptr) {
        osVersion = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, osVersion, strlen(osVersion), &napiValue));
    return napiValue;
}

static napi_value GetMajorVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int majorVersion = GetMajorVersion();

    NAPI_CALL(env, napi_create_int32(env, majorVersion, &napiValue));
    return napiValue;
}

static napi_value GetSeniorVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int seniorVersion = GetSeniorVersion();

    NAPI_CALL(env, napi_create_int32(env, seniorVersion, &napiValue));
    return napiValue;
}

static napi_value GetFeatureVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int featureVersion = GetFeatureVersion();

    NAPI_CALL(env, napi_create_int32(env, featureVersion, &napiValue));
    return napiValue;
}

static napi_value GetBuildVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int buildVersion = GetBuildVersion();

    NAPI_CALL(env, napi_create_int32(env, buildVersion, &napiValue));
    return napiValue;
}

static napi_value GetSdkApiVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int sdkApiVersion = GetSdkApiVersion();

    NAPI_CALL(env, napi_create_int32(env, sdkApiVersion, &napiValue));
    return napiValue;
}
static napi_value GetSdkMinorApiVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int sdkMinorApiVersion = GetSdkMinorApiVersion();

    NAPI_CALL(env, napi_create_int32(env, sdkMinorApiVersion, &napiValue));
    return napiValue;
}
static napi_value GetSdkPatchApiVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int sdkPatchApiVersion = GetSdkPatchApiVersion();

    NAPI_CALL(env, napi_create_int32(env, sdkPatchApiVersion, &napiValue));
    return napiValue;
}

#define DISTRIBUTION_OS_API_VER_MAX 999999
#define DISTRIBUTION_OS_API_VER_MIN 10000
#define DISTRIBUTION_PERCENT 100
#define API_VERSION_MAX 99
#define TARGET_OS_NAME_PART1 "Harmony"
#define TARGET_OS_NAME_PART2 "OS"
#define TARGET_OS_NAME TARGET_OS_NAME_PART1 TARGET_OS_NAME_PART2
#define VERSION_STR_MAX_LEN 64
#define DECIMAL_BASE 10

// 校验版本三段是否都在合法范围：major∈[1, API_VERSION_MAX]，minor/patch∈[0, API_VERSION_MAX]
static bool IsVersionInRange(int32_t major, int32_t minor, int32_t patch)
{
    return major >= 1 && major <= API_VERSION_MAX &&
           minor >= 0 && minor <= API_VERSION_MAX &&
           patch >= 0 && patch <= API_VERSION_MAX;
}

// 解析 DistributionOS 分发版本（打包编码 = major*10000 + minor*100 + patch）。
// 仅依据打包值与各字段范围校验；OS 名称的判定由调用方完成。
// 打包值、各字段均合法时返回 true 并写出三段版本。
static bool TryResolveDistributionOSVersion(int32_t* major, int32_t* minor, int32_t* patch)
{
    int32_t distributionOSApiVersion = GetDistributionOSApiVersion();
    // 校验范围：必须在 [10000, 999999] 之间（对应 1.0.0 ~ 99.99.99）
    if (distributionOSApiVersion < DISTRIBUTION_OS_API_VER_MIN ||
        distributionOSApiVersion > DISTRIBUTION_OS_API_VER_MAX) {
        return false;
    }

    int32_t majorDistribution = distributionOSApiVersion / DISTRIBUTION_OS_API_VER_MIN;
    int32_t minorDistribution = (distributionOSApiVersion / DISTRIBUTION_PERCENT) % DISTRIBUTION_PERCENT;
    int32_t patchDistribution = distributionOSApiVersion % DISTRIBUTION_PERCENT;
    // 再次校验各字段范围（防御性编程）
    if (!IsVersionInRange(majorDistribution, minorDistribution, patchDistribution)) {
        return false;
    }

    *major = majorDistribution;
    *minor = minorDistribution;
    *patch = patchDistribution;
    BEGET_LOGI("Using DistributionOS API version: %d.%d.%d (from %d)",
        *major, *minor, *patch, distributionOSApiVersion);
    return true;
}

// 读取一段无符号整数（至少 1 位数字）。成功返回 true，并通过 endp 回传结束位置、value 回传数值。
// 严格要求以数字开头，杜绝 sscanf 的 %d 跳过前导空白导致的 "1. 2. 3"、" 1.2.3" 误判。
static bool ReadVersionSegment(const char* p, const char** endp, int* value)
{
    if (*p < '0' || *p > '9') {
        *endp = p;
        return false;
    }
    int v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * DECIMAL_BASE + (*p - '0');
        ++p;
        // 版本号每段上限为 API_VERSION_MAX；超出即非法（同时避免 int 溢出）
        if (v > API_VERSION_MAX) {
            *endp = p;
            return false;
        }
    }
    *value = v;
    *endp = p;
    return true;
}

// 单趟严格解析：格式为 "N.N.N" 或 "N.N.N(M)"，N/M 为无符号整数（无空格、符号）。
// 逐字符读取，任何多余或非法字符（含空格）即判非法。
static bool ParseVersionFromString(const char* str, int32_t* majorVersion, int32_t* minorVersion,
    int32_t* patchVersion)
{
    if (str == nullptr) {
        return false;
    }

    const char* p = str;
    int major = 0;
    int minor = 0;
    int patch = 0;

    // 严格按 "数字.数字.数字" 解析，点号必须紧跟在数字之后
    if (!ReadVersionSegment(p, &p, &major) || *p != '.') {
        return false;
    }
    ++p;
    if (!ReadVersionSegment(p, &p, &minor) || *p != '.') {
        return false;
    }
    ++p;
    if (!ReadVersionSegment(p, &p, &patch)) {
        return false;
    }

    // 可选后缀 "(M)"：括号内的第 4 段（OpenHarmony 版本号）仅校验合法性、不参与比较
    // （取值范围 [0, API_VERSION_MAX] 由 ReadVersionSegment 保证）
    if (*p == '(') {
        ++p;
        int ohVersion = 0;
        if (!ReadVersionSegment(p, &p, &ohVersion) || *p != ')') {
            return false;
        }
        ++p;
    }

    if (*p != '\0') {
        return false;
    }

    if (major > 0 && minor >= 0 && patch >= 0) {
        *majorVersion = major;
        *minorVersion = minor;
        *patchVersion = patch;
        return true;
    }
    // 其他格式（如 "0.0.0"）视为非法
    return false;
}

static bool ParseVersionFromArg(napi_env env, napi_value arg,
    int32_t* majorVersion, int32_t* minorVersion, int32_t* patchVersion)
{
    napi_valuetype type;
    if (napi_typeof(env, arg, &type) != napi_ok) {
        return false;
    }

    if (type == napi_number) {
        double value;
        if (napi_get_value_double(env, arg, &value) != napi_ok) {
            return false;
        }
        *majorVersion = static_cast<int32_t>(value);
        *minorVersion = 0;
        *patchVersion = 0;
        return (*majorVersion > 0); // major 必须 >=1
    }

    if (type == napi_string) {
        char str[VERSION_STR_MAX_LEN] = {0};
        size_t len = 0;
        if (napi_get_value_string_utf8(env, arg, str, sizeof(str), &len) != napi_ok) {
            return false;
        }
        return ParseVersionFromString(str, majorVersion, minorVersion, patchVersion);
    }

    return false;
}

// 通过判断系统是 HO 还是 OH 进行版本号对比
static bool CheckApiVersionGreaterOrEqualByOS(int majorVersion, int minorVersion, int patchVersion)
{
    if (!IsVersionInRange(majorVersion, minorVersion, patchVersion)) {
        return false;
    }

    int32_t osMajor = 0;
    int32_t osMinor = 0;
    int32_t osPatch = 0;
    // name 为 DistributionOS 时走 DistributionOS 分发版本；否则（或分发版本无法解码时）回退到 OH SDK 版本
    const char* distributionOSName = GetDistributionOSName();
    bool isDistributionOS = (distributionOSName != nullptr &&
        strcmp(distributionOSName, TARGET_OS_NAME) == 0);
    if (!isDistributionOS || !TryResolveDistributionOSVersion(&osMajor, &osMinor, &osPatch)) {
        osMajor = GetSdkApiVersion();
        osMinor = GetSdkMinorApiVersion();
        osPatch = GetSdkPatchApiVersion();
    }

    // 统一比较：系统版本 >= 请求版本（major 不同比 major，其次 minor，最后 patch>=）
    if (majorVersion != osMajor) {
        return osMajor > majorVersion;
    }
    if (minorVersion != osMinor) {
        return osMinor > minorVersion;
    }
    return osPatch >= patchVersion;
}

static napi_value ApiAvailable(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_status status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok || argc < 1) {
        napi_value result;
        NAPI_CALL(env, napi_get_boolean(env, false, &result));
        return result;
    }

    int32_t majorVersion = 0;
    int32_t minorVersion = 0;
    int32_t patchVersion = 0;
    // 解析版本号
    bool parsed = ParseVersionFromArg(env, argv[0], &majorVersion, &minorVersion, &patchVersion);
    bool ret = false;
    if (parsed) {
        ret = CheckApiVersionGreaterOrEqualByOS(majorVersion, minorVersion, patchVersion);
    }

    napi_value result;
    NAPI_CALL(env, napi_get_boolean(env, ret, &result));
    return result;
}

static napi_value GetFirstApiVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int firstApiVersion = GetFirstApiVersion();

    NAPI_CALL(env, napi_create_int32(env, firstApiVersion, &napiValue));
    return napiValue;
}

static napi_value GetVersionId(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *versionId = GetVersionId();
    if (versionId == nullptr) {
        versionId = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, versionId, strlen(versionId), &napiValue));
    return napiValue;
}

static napi_value GetBuildType(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *buildType = GetBuildType();
    if (buildType == nullptr) {
        buildType = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, buildType, strlen(buildType), &napiValue));
    return napiValue;
}

static napi_value GetBuildUser(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *buildUser = GetBuildUser();
    if (buildUser == nullptr) {
        buildUser = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, buildUser, strlen(buildUser), &napiValue));
    return napiValue;
}

static napi_value GetBuildHost(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *buildHost = GetBuildHost();
    if (buildHost == nullptr) {
        buildHost = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, buildHost, strlen(buildHost), &napiValue));
    return napiValue;
}

static napi_value GetBuildTime(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *buildTime = GetBuildTime();
    if (buildTime == nullptr) {
        buildTime = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, buildTime, strlen(buildTime), &napiValue));
    return napiValue;
}

static napi_value GetBuildRootHash(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *buildRootHash = GetBuildRootHash();
    if (buildRootHash == nullptr) {
        buildRootHash = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, buildRootHash, strlen(buildRootHash), &napiValue));
    return napiValue;
}

static napi_value GetBootCount(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int bootCount = GetBootCount();
    NAPI_CALL(env, napi_create_int32(env, bootCount, &napiValue));
    return napiValue;
}

static napi_value GetChipType(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *chipType = GetChipType();
    if (chipType == nullptr) {
        chipType = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, chipType, strlen(chipType), &napiValue));
    return napiValue;
}

static napi_value GetDeviceColor(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *deviceColor = GetDeviceColor();
    if (deviceColor == nullptr) {
        deviceColor = "";
    }

    NAPI_CALL(env, napi_create_string_utf8(env, deviceColor, strlen(deviceColor), &napiValue));
    return napiValue;
}

static napi_value GetDevUdid(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    char localDeviceId[UDID_LEN] = {0};
    AclGetDevUdid(localDeviceId, UDID_LEN);

    NAPI_CALL(env, napi_create_string_utf8(env, localDeviceId, strlen(localDeviceId), &napiValue));
    return napiValue;
}

static napi_value NAPI_GetDistributionOSName(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *val = GetDistributionOSName();

    NAPI_CALL(env, napi_create_string_utf8(env, val, strlen(val), &napiValue));
    return napiValue;
}

static napi_value NAPI_GetDistributionOSVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *val = GetDistributionOSVersion();

    NAPI_CALL(env, napi_create_string_utf8(env, val, strlen(val), &napiValue));
    return napiValue;
}

static napi_value NAPI_GetDistributionOSApiVersion(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int sdkApiVersion = GetDistributionOSApiVersion();

    NAPI_CALL(env, napi_create_int32(env, sdkApiVersion, &napiValue));
    return napiValue;
}

static napi_value NAPI_GetDistributionOSApiName(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *val = GetDistributionOSApiName();

    NAPI_CALL(env, napi_create_string_utf8(env, val, strlen(val), &napiValue));
    return napiValue;
}

static napi_value NAPI_GetDistributionOSReleaseType(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *val = GetDistributionOSReleaseType();

    NAPI_CALL(env, napi_create_string_utf8(env, val, strlen(val), &napiValue));
    return napiValue;
}

static DevInfoError AclGetDevOdid(char *odid, int size)
{
    DevInfoError ret = DEV_INFO_OK;
    if (odid[0] != '\0') {
        return DEV_INFO_OK;
    }
    auto systemAbilityManager = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemAbilityManager) {
        return DEV_INFO_ENULLPTR;
    }

    auto remoteObject = systemAbilityManager->GetSystemAbility(OHOS::BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    if (!remoteObject) {
        return DEV_INFO_ENULLPTR;
    }

#ifdef DEPENDENT_APPEXECFWK_BASE
    auto bundleMgr = OHOS::iface_cast<OHOS::AppExecFwk::IBundleMgr>(remoteObject);
    if (!bundleMgr) {
        return DEV_INFO_ENULLPTR;
    }

    std::string odidStr;
    if (bundleMgr->GetOdid(odidStr) != 0) {
        return DEV_INFO_EGETODID;
    }

    if (strcpy_s(odid, size, odidStr.c_str()) != EOK) {
        return DEV_INFO_ESTRCOPY;
    }
#else
    DEVINFO_LOGE("DEPENDENT_APPEXECFWK_BASE does not exist, The ODID could not be obtained");
    ret = DEV_INFO_EGETODID;
#endif

    return ret;
}

static napi_value GetDevOdid(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    static char odid[ODID_LEN] = {0};
    DevInfoError ret = AclGetDevOdid(odid, ODID_LEN);
    if (ret != DEV_INFO_OK) {
        DEVINFO_LOGE("GetDevOdid ret:%d", ret);
    }

    NAPI_CALL(env, napi_create_string_utf8(env, odid, strlen(odid), &napiValue));
    return napiValue;
}

static napi_value GetDiskSN(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    static char diskSN[DISK_SN_LEN] = {0};
    AclGetDiskSN(diskSN, DISK_SN_LEN);

    NAPI_CALL(env, napi_create_string_utf8(env, diskSN, strlen(diskSN), &napiValue));
    return napiValue;
}

static napi_value CreateDeviceTypes(napi_env env, napi_value exports)
{
    napi_value deviceTypes = nullptr;
    napi_value typeDefault = nullptr;
    napi_value typePhone = nullptr;
    napi_value typeTablet = nullptr;
    napi_value type2in1 = nullptr;
    napi_value typeTv = nullptr;
    napi_value typeWearable = nullptr;
    napi_value typeCar = nullptr;

    napi_create_object(env, &deviceTypes);

    napi_create_string_utf8(env, "default", NAPI_AUTO_LENGTH, &typeDefault);
    napi_create_string_utf8(env, "phone", NAPI_AUTO_LENGTH, &typePhone);
    napi_create_string_utf8(env, "tablet", NAPI_AUTO_LENGTH, &typeTablet);
    napi_create_string_utf8(env, "2in1", NAPI_AUTO_LENGTH, &type2in1);
    napi_create_string_utf8(env, "tv", NAPI_AUTO_LENGTH, &typeTv);
    napi_create_string_utf8(env, "wearable", NAPI_AUTO_LENGTH, &typeWearable);
    napi_create_string_utf8(env, "car", NAPI_AUTO_LENGTH, &typeCar);

    napi_set_named_property(env, deviceTypes, "TYPE_DEFAULT", typeDefault);
    napi_set_named_property(env, deviceTypes, "TYPE_PHONE", typePhone);
    napi_set_named_property(env, deviceTypes, "TYPE_TABLET", typeTablet);
    napi_set_named_property(env, deviceTypes, "TYPE_2IN1", type2in1);
    napi_set_named_property(env, deviceTypes, "TYPE_TV", typeTv);
    napi_set_named_property(env, deviceTypes, "TYPE_WEARABLE", typeWearable);
    napi_set_named_property(env, deviceTypes, "TYPE_CAR", typeCar);

    napi_set_named_property(env, exports, "DeviceTypes", deviceTypes);

    return exports;
}

static napi_value EnumLevelClassConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisArg = nullptr;
    void* data = nullptr;

    napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, &data);

    napi_value global = nullptr;
    napi_get_global(env, &global);

    return thisArg;
}

static napi_value CreateEnumLevelState(napi_env env, napi_value exports)
{
    napi_value high = nullptr;
    napi_value medium = nullptr;
    napi_value low = nullptr;

    napi_create_int32(env, (int32_t)PerformanceClassLevel::CLASS_LEVEL_HIGH, &high);
    napi_create_int32(env, (int32_t)PerformanceClassLevel::CLASS_LEVEL_MEDIUM, &medium);
    napi_create_int32(env, (int32_t)PerformanceClassLevel::CLASS_LEVEL_LOW, &low);

    napi_property_descriptor desc[] = {
        DECLARE_NAPI_STATIC_PROPERTY("CLASS_LEVEL_HIGH", high),
        DECLARE_NAPI_STATIC_PROPERTY("CLASS_LEVEL_MEDIUM", medium),
        DECLARE_NAPI_STATIC_PROPERTY("CLASS_LEVEL_LOW", low),
    };

    napi_value result = nullptr;
    napi_define_class(env, "PerformanceClassLevel", NAPI_AUTO_LENGTH, EnumLevelClassConstructor, nullptr,
        sizeof(desc) / sizeof(*desc), desc, &result);

    napi_set_named_property(env, exports, "PerformanceClassLevel", result);

    return exports;
}

static napi_value GetPerformanceClass(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    int performanceClass = GetPerformanceClass();

    NAPI_CALL(env, napi_create_int32(env, performanceClass, &napiValue));
    return napiValue;
}

EXTERN_C_START
/*
 * Module init
 */
static napi_value Init(napi_env env, napi_value exports)
{
    /*
     * Attribute definition
     */
    napi_property_descriptor desc[] = {
        {"deviceType", nullptr, nullptr, GetDeviceType, nullptr, nullptr, napi_default, nullptr},
        {"manufacture", nullptr, nullptr, GetManufacture, nullptr, nullptr, napi_default, nullptr},
        {"brand", nullptr, nullptr, GetBrand, nullptr, nullptr, napi_default, nullptr},
        {"marketName", nullptr, nullptr, GetMarketName, nullptr, nullptr, napi_default, nullptr},
        {"productSeries", nullptr, nullptr, GetProductSeries, nullptr, nullptr, napi_default, nullptr},
        {"productModel", nullptr, nullptr, GetProductModel, nullptr, nullptr, napi_default, nullptr},
        {"softwareModel", nullptr, nullptr, GetSoftwareModel, nullptr, nullptr, napi_default, nullptr},
        {"hardwareModel", nullptr, nullptr, GetHardwareModel, nullptr, nullptr, napi_default, nullptr},
        {"hardwareProfile", nullptr, nullptr, GetHardwareProfile, nullptr, nullptr, napi_default, nullptr},
        {"serial", nullptr, nullptr, GetSerial, nullptr, nullptr, napi_default, nullptr},
        {"bootloaderVersion", nullptr, nullptr, GetBootloaderVersion, nullptr, nullptr, napi_default, nullptr},
        {"abiList", nullptr, nullptr, GetAbiList, nullptr, nullptr, napi_default, nullptr},
        {"securityPatchTag", nullptr, nullptr, GetSecurityPatchTag, nullptr, nullptr, napi_default, nullptr},
        {"displayVersion", nullptr, nullptr, GetDisplayVersion, nullptr, nullptr, napi_default, nullptr},
        {"incrementalVersion", nullptr, nullptr, GetIncrementalVersion, nullptr, nullptr, napi_default, nullptr},
        {"osReleaseType", nullptr, nullptr, GetOsReleaseType, nullptr, nullptr, napi_default, nullptr},
        {"osFullName", nullptr, nullptr, GetOSFullName, nullptr, nullptr, napi_default, nullptr},
        {"majorVersion", nullptr, nullptr, GetMajorVersion, nullptr, nullptr, napi_default, nullptr},
        {"seniorVersion", nullptr, nullptr, GetSeniorVersion, nullptr, nullptr, napi_default, nullptr},
        {"featureVersion", nullptr, nullptr, GetFeatureVersion, nullptr, nullptr, napi_default, nullptr},
        {"buildVersion", nullptr, nullptr, GetBuildVersion, nullptr, nullptr, napi_default, nullptr},
        {"sdkApiVersion", nullptr, nullptr, GetSdkApiVersion, nullptr, nullptr, napi_default, nullptr},
        {"sdkMinorApiVersion", nullptr, nullptr, GetSdkMinorApiVersion, nullptr, nullptr, napi_default, nullptr},
        {"sdkPatchApiVersion", nullptr, nullptr, GetSdkPatchApiVersion, nullptr, nullptr, napi_default, nullptr},
        {"apiAvailable", nullptr, ApiAvailable, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"firstApiVersion", nullptr, nullptr, GetFirstApiVersion, nullptr, nullptr, napi_default, nullptr},
        {"versionId", nullptr, nullptr, GetVersionId, nullptr, nullptr, napi_default, nullptr},
        {"buildType", nullptr, nullptr, GetBuildType, nullptr, nullptr, napi_default, nullptr},
        {"buildUser", nullptr, nullptr, GetBuildUser, nullptr, nullptr, napi_default, nullptr},
        {"buildHost", nullptr, nullptr, GetBuildHost, nullptr, nullptr, napi_default, nullptr},
        {"buildTime", nullptr, nullptr, GetBuildTime, nullptr, nullptr, napi_default, nullptr},
        {"buildRootHash", nullptr, nullptr, GetBuildRootHash, nullptr, nullptr, napi_default, nullptr},
        {"udid", nullptr, nullptr, GetDevUdid, nullptr, nullptr, napi_default, nullptr},
        {"distributionOSName", nullptr, nullptr, NAPI_GetDistributionOSName, nullptr, nullptr, napi_default, nullptr},
        {"distributionOSVersion", nullptr, nullptr, NAPI_GetDistributionOSVersion, nullptr, nullptr, napi_default,
            nullptr},
        {"distributionOSApiVersion", nullptr, nullptr, NAPI_GetDistributionOSApiVersion, nullptr, nullptr,
            napi_default, nullptr},
        {"distributionOSApiName", nullptr, nullptr, NAPI_GetDistributionOSApiName, nullptr, nullptr, napi_default,
            nullptr},
        {"distributionOSReleaseType", nullptr, nullptr, NAPI_GetDistributionOSReleaseType, nullptr, nullptr,
            napi_default, nullptr},
        {"ODID", nullptr, nullptr, GetDevOdid, nullptr, nullptr, napi_default, nullptr},
        {"productModelAlias", nullptr, nullptr, GetProductModelAlias, nullptr, nullptr, napi_default, nullptr},
        {"diskSN", nullptr, nullptr, GetDiskSN, nullptr, nullptr, napi_default, nullptr},
        {"performanceClass", nullptr, nullptr, GetPerformanceClass, nullptr, nullptr, napi_default, nullptr},
        {"chipType", nullptr, nullptr, GetChipType, nullptr, nullptr, napi_default, nullptr},
        {"bootCount", nullptr, nullptr, GetBootCount, nullptr, nullptr, napi_default, nullptr},
        {"deviceColor", nullptr, nullptr, GetDeviceColor, nullptr, nullptr, napi_default, nullptr},
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(napi_property_descriptor), desc));

    CreateEnumLevelState(env, exports);
    CreateDeviceTypes(env, exports);

    return exports;
}
EXTERN_C_END

/*
 * Module definition
 */
static napi_module _module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = Init,
    .nm_modname = "deviceInfo",
    .nm_priv = ((void *)0),
    .reserved = { 0 }
};
/*
 * Module registration function
 */
extern "C" __attribute__((constructor)) void RegisterModule(void)
{
    napi_module_register(&_module);
}
