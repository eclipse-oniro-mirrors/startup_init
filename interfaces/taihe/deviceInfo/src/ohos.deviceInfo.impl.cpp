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
#include <atomic>
#include <cstring>
#include <string>
#include "ohos.deviceInfo.impl.hpp"
#include "stdexcept"

#include "beget_ext.h"
#include "parameter.h"
#include "sysversion.h"
#include "bundlemgr/bundle_mgr_proxy.h"
#include "iservice_registry.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
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

constexpr int UDID_LEN = 65;
constexpr int ODID_LEN = 37;
constexpr int DISK_SN_LEN = 65;

#define API_VERSION_MAX 99
// apiAvailable 上线版本号。number 入参合法范围为 [1, LAUNCH-1]；string major >= LAUNCH 时不允许带括号后缀。
#define APIAVAILABLE_LAUNCH_VERSION 26
// 版本比较时匹配的 OS 名称。用拼接构造，避免源码中出现该名称的完整字面量。
#define TARGET_OS_NAME_PART1 "Harmony"
#define TARGET_OS_NAME_PART2 "OS"
#define TARGET_OS_NAME TARGET_OS_NAME_PART1 TARGET_OS_NAME_PART2
#define DISTRIBUTION_OS_API_VER_MAX 999999
#define DISTRIBUTION_OS_API_VER_MIN 10000
#define DISTRIBUTION_PERCENT 100
#define DECIMAL_BASE 10

typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;

namespace {
// To be implemented.

::taihe::string getbrand()
{
    const char *value = GetBrand();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdeviceType()
{
    const char *value = GetDeviceType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getproductSeries()
{
    const char *value = GetProductSeries();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getproductModel()
{
    const char *value = GetProductModel();
    if (value == nullptr) {
        value = "";
    }
    return value;
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

::taihe::string ODID()
{
    static char value[ODID_LEN] = {0};
    DevInfoError ret = AclGetDevOdid(value, ODID_LEN);
    if (ret != DEV_INFO_OK) {
        DEVINFO_LOGE("GetDevOdid ret:%d", ret);
    }
    return value;
}

::taihe::string getudid()
{
    char value[UDID_LEN] = {0};
    AclGetDevUdid(value, UDID_LEN);
    return value;
}

::taihe::string getserial()
{
    const char *value = AclGetSerial();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getmanufacture()
{
    const char *value = GetManufacture();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getmarketName()
{
    const char *value = GetMarketName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getproductModelAlias()
{
    const char *value = GetProductModelAlias();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getsoftwareModel()
{
    const char *value = GetSoftwareModel();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string gethardwareModel()
{
    const char *value = GetHardwareModel();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string gethardwareProfile()
{
    const char *value = GetHardwareProfile();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getbootloaderVersion()
{
    const char *value = GetBootloaderVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getabiList()
{
    const char *value = GetAbiList();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getsecurityPatchTag()
{
    const char *value = GetSecurityPatchTag();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdisplayVersion()
{
    const char *value = GetDisplayVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getincrementalVersion()
{
    const char *value = GetIncrementalVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getosReleaseType()
{
    const char *value = GetOsReleaseType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getosFullName()
{
    const char *value = GetOSFullName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getversionId()
{
    const char *value = GetVersionId();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getbuildType()
{
    const char *value = GetBuildType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getbuildUser()
{
    const char *value = GetBuildUser();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getbuildHost()
{
    const char *value = GetBuildHost();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getbuildTime()
{
    const char *value = GetBuildTime();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getbuildRootHash()
{
    const char *value = GetBuildRootHash();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdistributionOSName()
{
    const char *value = GetDistributionOSName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdistributionOSVersion()
{
    const char *value = GetDistributionOSVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdistributionOSApiName()
{
    const char *value = GetDistributionOSApiName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdistributionOSReleaseType()
{
    const char *value = GetDistributionOSReleaseType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdiskSN()
{
    static char value[DISK_SN_LEN] = {0};
    AclGetDiskSN(value, DISK_SN_LEN);
    return value;
}

::taihe::string getchipType()
{
    const char *value = GetChipType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::taihe::string getdeviceColor()
{
    const char *value = GetDeviceColor();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

::ohos::deviceInfo::PerformanceClassLevel getperformanceClass()
{
    int value = GetPerformanceClass();
    return (ohos::deviceInfo::PerformanceClassLevel::key_t)value;
}

int32_t getsdkApiVersion()
{
    int value = GetSdkApiVersion();
    return value;
}

int32_t getsdkMinorApiVersion()
{
    int value = GetSdkMinorApiVersion();
    return value;
}

int32_t getsdkPatchApiVersion()
{
    int value = GetSdkPatchApiVersion();
    return value;
}

int32_t getmajorVersion()
{
    int value = GetMajorVersion();
    return value;
}

int32_t getseniorVersion()
{
    int value = GetSeniorVersion();
    return value;
}

int32_t getfeatureVersion()
{
    int value = GetFeatureVersion();
    return value;
}

int32_t getbuildVersion()
{
    int value = GetBuildVersion();
    return value;
}

int32_t getfirstApiVersion()
{
    int value = GetFirstApiVersion();
    return value;
}

int32_t getdistributionOSApiVersion()
{
    int value = GetDistributionOSApiVersion();
    return value;
}

int32_t getbootCount()
{
    int value = GetBootCount();
    return value;
}

// ===== apiAvailable（逻辑移植自 native_deviceinfo_js.cpp）=====

// 系统侧 API 版本信息，由调用方通过系统参数 getter 收集后传入，使版本比较逻辑成为纯函数。
struct OsApiInfo {
    const char* distributionOSName;    // GetDistributionOSName()
    int32_t distributionOSApiVersion;  // GetDistributionOSApiVersion()
    int32_t sdkMajor;                  // GetSdkApiVersion()
    int32_t sdkMinor;                  // GetSdkMinorApiVersion()
    int32_t sdkPatch;                  // GetSdkPatchApiVersion()
};

// 校验版本三段是否都在合法范围：major∈[1, API_VERSION_MAX]，minor/patch∈[0, API_VERSION_MAX]
static bool IsVersionInRange(int32_t major, int32_t minor, int32_t patch)
{
    return major >= 1 && major <= API_VERSION_MAX &&
           minor >= 0 && minor <= API_VERSION_MAX &&
           patch >= 0 && patch <= API_VERSION_MAX;
}

// 校验数字入参是否为合法 API level：必须是 [1, APIAVAILABLE_LAUNCH_VERSION) 内的整数。
// IDL 中 numVersion 类型为 i32，小数 / NaN / Infinity 在类型层已被排除，这里只校验区间。
static bool IsValidNumberApiLevel(int32_t value)
{
    return value >= 1 && value < APIAVAILABLE_LAUNCH_VERSION;
}

// 解析 DistributionOS 分发版本（打包编码 = major*10000 + minor*100 + patch）。
// 仅依据打包值与各字段范围校验；OS 名称的判定由调用方完成。
// 打包值、各字段均合法时返回 true 并写出三段版本。
static bool TryResolveDistributionOSVersion(int32_t packedVersion, int32_t* major, int32_t* minor, int32_t* patch)
{
    // 校验范围：必须在 [10000, 999999] 之间（对应 1.0.0 ~ 99.99.99）
    if (packedVersion < DISTRIBUTION_OS_API_VER_MIN || packedVersion > DISTRIBUTION_OS_API_VER_MAX) {
        return false;
    }

    int32_t majorDistribution = packedVersion / DISTRIBUTION_OS_API_VER_MIN;
    int32_t minorDistribution = (packedVersion / DISTRIBUTION_PERCENT) % DISTRIBUTION_PERCENT;
    int32_t patchDistribution = packedVersion % DISTRIBUTION_PERCENT;
    // packedVersion ∈ [10000, 999999] 已保证三段分别 ∈ [1,99]/[0,99]/[0,99]，无需再校验

    *major = majorDistribution;
    *minor = minorDistribution;
    *patch = patchDistribution;
    DEVINFO_LOGI("Using DistributionOS API version: %d.%d.%d (from %d)", *major, *minor, *patch, packedVersion);
    return true;
}

// 读取一段无符号整数（至少 1 位数字）。成功返回 true，并通过 endp 回传结束位置、value 回传数值。
// 严格要求以数字开头，杜绝 sscanf 跳过前导空白导致的 "1. 2. 3"、" 1.2.3" 误判。
// 禁止前导 0："010"、"00" 视为非法，单独的 "0" 合法。
static bool ReadVersionSegment(const char* p, const char** endp, int* value)
{
    if (*p < '0' || *p > '9') {
        *endp = p;
        return false;
    }
    const char* start = p; // 段起始位置，用于循环后判断前导 0
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
    // 前导 0：首位为 '0' 且段长 > 1（如 "010"、"00"）；单独的 "0" 合法。
    // 用段长 (p - start) 判断，避免对 p+1 做前置窥探（防静态分析误报越界）。
    if (*start == '0' && (p - start) > 1) {
        *endp = p;
        return false;
    }
    *value = v;
    *endp = p;
    return true;
}

// 单趟严格解析：格式为 "N.N.N" 或 "N.N.N(M)"，N/M 为无符号整数（无空格、符号、前导 0）。
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
    // （ohVersion 取值 [1, API_VERSION_MAX]，禁 "0"）。
    // major >= 上线版本时不允许带括号后缀。
    if (*p == '(') {
        if (major >= APIAVAILABLE_LAUNCH_VERSION) {
            return false;
        }
        ++p;
        int ohVersion = 0;
        if (!ReadVersionSegment(p, &p, &ohVersion) || ohVersion < 1 || *p != ')') {
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

// 判断当前进程(调用方 app)的 compileSdkType 是否为 "OpenHarmony"。
// 用于：OpenHarmony 工程的字符串版本号 major 必须 >= 上线版本。
// 进程内缓存结果(compileSdkType 不变)。无 bundle 框架(DEPENDENT_APPEXECFWK_BASE 未定义)时返回 false(不做该校验)。
// 查询失败也返回 false(fail-open：无法判定时不强制限制)。
static bool IsOpenHarmonyCompileSdkType()
{
#ifdef DEPENDENT_APPEXECFWK_BASE
    // 三态缓存：NOT_RESOLVED 未查/上次失败；NOT_OPEN_HARMONY_SDK 否；IS_OPEN_HARMONY_SDK 是。
    // 只在拿到非空 compileSdkType 时缓存；空值/失败留 NOT_RESOLVED，下次调用重试。
    // 用 atomic 避免 cache 的数据竞争；首次并发可能重复查询一次，但结果幂等（compileSdkType 不变），无害。
    constexpr int8_t NOT_RESOLVED = -1;
    constexpr int8_t NOT_OPEN_HARMONY_SDK = 0;
    constexpr int8_t IS_OPEN_HARMONY_SDK = 1;
    static std::atomic<int8_t> cacheState{NOT_RESOLVED};
    int8_t cachedValue = cacheState.load(std::memory_order_relaxed);
    if (cachedValue != NOT_RESOLVED) {
        return cachedValue == IS_OPEN_HARMONY_SDK;
    }
    bool isOpenHarmonySdk = false;
    auto systemAbilityManager = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    auto remoteObject = (systemAbilityManager != nullptr)
        ? systemAbilityManager->GetSystemAbility(OHOS::BUNDLE_MGR_SERVICE_SYS_ABILITY_ID)
        : nullptr;
    auto bundleMgr = (remoteObject != nullptr)
        ? OHOS::iface_cast<OHOS::AppExecFwk::IBundleMgr>(remoteObject)
        : nullptr;
    if (bundleMgr == nullptr) {
        DEVINFO_LOGE("IsOpenHarmonyCompileSdkType: get bundleMgr proxy failed");
    } else {
        // GetBundleInfoForSelf 直接取调用方自己的 BundleInfo（服务端按 caller 解析，无需 uid/userId）。
        // WITH_APPLICATION 确保 applicationInfo 被填充（含 compileSdkType）。
        OHOS::AppExecFwk::BundleInfo bundleInfo;
        if (bundleMgr->GetBundleInfoForSelf(
            static_cast<int32_t>(OHOS::AppExecFwk::GetBundleInfoFlag::GET_BUNDLE_INFO_WITH_APPLICATION),
            bundleInfo) != 0) {
            DEVINFO_LOGE("IsOpenHarmonyCompileSdkType: GetBundleInfoForSelf failed");
        } else if (bundleInfo.applicationInfo.compileSdkType.empty()) {
            DEVINFO_LOGE("IsOpenHarmonyCompileSdkType: compileSdkType is empty");
        } else {
            isOpenHarmonySdk = (bundleInfo.applicationInfo.compileSdkType == "OpenHarmony");
            // 拿到非空 compileSdkType，直接缓存；空值/失败不缓存，下次调用重试
            cacheState.store(isOpenHarmonySdk ? IS_OPEN_HARMONY_SDK : NOT_OPEN_HARMONY_SDK,
                std::memory_order_relaxed);
        }
    }
    return isOpenHarmonySdk; // 未取到值时为 false（fail-open：无法判定时不强制限制）
#else
    return false; // 无 bundle 框架，不做 compileSdkType 校验
#endif
}

// 把 taihe 联合体 VersionParam 解析成三段版本号，等价于 native_deviceinfo_js.cpp 的 ParseVersionFromArg。
static bool ParseVersionFromArg(const ohos::deviceInfo::VersionParam& version,
    int32_t* majorVersion, int32_t* minorVersion, int32_t* patchVersion)
{
    if (version.holds_numVersion()) {
        int32_t value = *version.get_numVersion_ptr();
        // 区间校验（逻辑见 IsValidNumberApiLevel）
        if (!IsValidNumberApiLevel(value)) {
            return false;
        }
        *majorVersion = value;
        *minorVersion = 0;
        *patchVersion = 0;
        return true;
    }

    if (version.holds_strVersion()) {
        if (!ParseVersionFromString(version.get_strVersion_ptr()->c_str(), majorVersion, minorVersion,
            patchVersion)) {
            return false;
        }
        // OpenHarmony 工程：字符串 major 必须 >= 上线版本（distributionOS 不限制）
        if (IsOpenHarmonyCompileSdkType() && *majorVersion < APIAVAILABLE_LAUNCH_VERSION) {
            return false;
        }
        return true;
    }

    return false;
}

// 通过判断系统是 DistributionOS 还是 OH 进行版本号对比。
// name 为 DistributionOS 且分发打包值可解码时用分发版本；否则（或解码失败时）回退到 OH SDK 版本。
static bool CheckApiVersionGreaterOrEqualByOS(int32_t reqMajor, int32_t reqMinor, int32_t reqPatch,
    const OsApiInfo& os)
{
    if (!IsVersionInRange(reqMajor, reqMinor, reqPatch)) {
        return false;
    }

    int32_t osMajor = 0;
    int32_t osMinor = 0;
    int32_t osPatch = 0;
    bool isDistributionOS = (os.distributionOSName != nullptr &&
        strcmp(os.distributionOSName, TARGET_OS_NAME) == 0);
    if (!isDistributionOS ||
        !TryResolveDistributionOSVersion(os.distributionOSApiVersion, &osMajor, &osMinor, &osPatch)) {
        osMajor = os.sdkMajor;
        osMinor = os.sdkMinor;
        osPatch = os.sdkPatch;
    }

    // 统一比较：系统版本 >= 请求版本（major 不同比 major，其次 minor，最后 patch>=）
    if (reqMajor != osMajor) {
        return osMajor > reqMajor;
    }
    if (reqMinor != osMinor) {
        return osMinor > reqMinor;
    }
    return osPatch >= reqPatch;
}

// 判定请求的 API 版本在当前系统是否可用（taihe 入口，返回布尔）
bool apiAvailable(const ohos::deviceInfo::VersionParam& version)
{
    int32_t majorVersion = 0;
    int32_t minorVersion = 0;
    int32_t patchVersion = 0;
    // 解析版本号
    bool parsed = ParseVersionFromArg(version, &majorVersion, &minorVersion, &patchVersion);
    bool ret = false;
    if (parsed) {
        // 收集系统侧版本信息后判定（系统版本 >= 请求版本）
        OsApiInfo os = { GetDistributionOSName(), GetDistributionOSApiVersion(),
                         GetSdkApiVersion(), GetSdkMinorApiVersion(), GetSdkPatchApiVersion() };
        ret = CheckApiVersionGreaterOrEqualByOS(majorVersion, minorVersion, patchVersion, os);
    }
    return ret;
}
}  // namespace

TH_EXPORT_CPP_API_getbrand(getbrand);
TH_EXPORT_CPP_API_getdeviceType(getdeviceType);
TH_EXPORT_CPP_API_getproductSeries(getproductSeries);
TH_EXPORT_CPP_API_getproductModel(getproductModel);
TH_EXPORT_CPP_API_ODID(ODID);
TH_EXPORT_CPP_API_getudid(getudid);
TH_EXPORT_CPP_API_getserial(getserial);
TH_EXPORT_CPP_API_getmanufacture(getmanufacture);
TH_EXPORT_CPP_API_getmarketName(getmarketName);
TH_EXPORT_CPP_API_getproductModelAlias(getproductModelAlias);
TH_EXPORT_CPP_API_getsoftwareModel(getsoftwareModel);
TH_EXPORT_CPP_API_gethardwareModel(gethardwareModel);
TH_EXPORT_CPP_API_gethardwareProfile(gethardwareProfile);
TH_EXPORT_CPP_API_getbootloaderVersion(getbootloaderVersion);
TH_EXPORT_CPP_API_getabiList(getabiList);
TH_EXPORT_CPP_API_getsecurityPatchTag(getsecurityPatchTag);
TH_EXPORT_CPP_API_getdisplayVersion(getdisplayVersion);
TH_EXPORT_CPP_API_getincrementalVersion(getincrementalVersion);
TH_EXPORT_CPP_API_getosReleaseType(getosReleaseType);
TH_EXPORT_CPP_API_getosFullName(getosFullName);
TH_EXPORT_CPP_API_getversionId(getversionId);
TH_EXPORT_CPP_API_getbuildType(getbuildType);
TH_EXPORT_CPP_API_getbuildUser(getbuildUser);
TH_EXPORT_CPP_API_getbuildHost(getbuildHost);
TH_EXPORT_CPP_API_getbuildTime(getbuildTime);
TH_EXPORT_CPP_API_getbuildRootHash(getbuildRootHash);
TH_EXPORT_CPP_API_getdistributionOSName(getdistributionOSName);
TH_EXPORT_CPP_API_getdistributionOSVersion(getdistributionOSVersion);
TH_EXPORT_CPP_API_getdistributionOSApiName(getdistributionOSApiName);
TH_EXPORT_CPP_API_getdiskSN(getdiskSN);
TH_EXPORT_CPP_API_getdistributionOSReleaseType(getdistributionOSReleaseType);
TH_EXPORT_CPP_API_getsdkApiVersion(getsdkApiVersion);
TH_EXPORT_CPP_API_getsdkMinorApiVersion(getsdkMinorApiVersion);
TH_EXPORT_CPP_API_getsdkPatchApiVersion(getsdkPatchApiVersion);
TH_EXPORT_CPP_API_getmajorVersion(getmajorVersion);
TH_EXPORT_CPP_API_getseniorVersion(getseniorVersion);
TH_EXPORT_CPP_API_getfeatureVersion(getfeatureVersion);
TH_EXPORT_CPP_API_getbuildVersion(getbuildVersion);
TH_EXPORT_CPP_API_getfirstApiVersion(getfirstApiVersion);
TH_EXPORT_CPP_API_getdistributionOSApiVersion(getdistributionOSApiVersion);
TH_EXPORT_CPP_API_getbootCount(getbootCount);
TH_EXPORT_CPP_API_getchipType(getchipType);
TH_EXPORT_CPP_API_getdeviceColor(getdeviceColor);
TH_EXPORT_CPP_API_getperformanceClass(getperformanceClass);
TH_EXPORT_CPP_API_apiAvailable(apiAvailable);
