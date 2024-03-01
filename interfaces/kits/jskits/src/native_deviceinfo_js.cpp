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
#include "parameter.h"
#include "sysversion.h"
#include "bundlemgr/bundle_mgr_proxy.h"
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

typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;

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

static napi_value NAPI_GetDistributionOSReleaseType(napi_env env, napi_callback_info info)
{
    napi_value napiValue = nullptr;
    const char *val = GetDistributionOSReleaseType();

    NAPI_CALL(env, napi_create_string_utf8(env, val, strlen(val), &napiValue));
    return napiValue;
}

static DevInfoError AclGetDevOdid(char *odid, int size)
{
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

    auto bundleMgrProxy = OHOS::iface_cast<OHOS::AppExecFwk::BundleMgrProxy>(remoteObject);
    if (!bundleMgrProxy) {
        return DEV_INFO_ENULLPTR;
    }

    std::string odidStr;
    if (bundleMgrProxy->GetOdid(odidStr) != 0) {
        return DEV_INFO_EGETODID;
    }

    if (strcpy_s(odid, size, odidStr.c_str()) != EOK) {
        return DEV_INFO_ESTRCOPY;
    }
    return DEV_INFO_OK;
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
        {"firstApiVersion", nullptr, nullptr, GetFirstApiVersion, nullptr, nullptr, napi_default, nullptr},
        {"versionId", nullptr, nullptr, GetVersionId, nullptr, nullptr, napi_default, nullptr},
        {"buildType", nullptr, nullptr, GetBuildType, nullptr, nullptr, napi_default, nullptr},
        {"buildUser", nullptr, nullptr, GetBuildUser, nullptr, nullptr, napi_default, nullptr},
        {"buildHost", nullptr, nullptr, GetBuildHost, nullptr, nullptr, napi_default, nullptr},
        {"buildTime", nullptr, nullptr, GetBuildTime, nullptr, nullptr, napi_default, nullptr},
        {"buildRootHash", nullptr, nullptr, GetBuildRootHash, nullptr, nullptr, napi_default, nullptr},
        {"udid", nullptr, nullptr, GetDevUdid, nullptr, nullptr, napi_default, nullptr},
        {"distributionOSName", nullptr, nullptr, NAPI_GetDistributionOSName, nullptr, nullptr, napi_default, nullptr},
        {"distributionOSVersion", nullptr, nullptr, NAPI_GetDistributionOSVersion, nullptr, nullptr, napi_default, nullptr},
        {"distributionOSApiVersion", nullptr, nullptr, NAPI_GetDistributionOSApiVersion, nullptr, nullptr, napi_default, nullptr},
        {"distributionOSReleaseType", nullptr, nullptr, NAPI_GetDistributionOSReleaseType, nullptr, nullptr, napi_default, nullptr},
        {"ODID", nullptr, nullptr, GetDevOdid, nullptr, nullptr, napi_default, nullptr},
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(napi_property_descriptor), desc));

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
