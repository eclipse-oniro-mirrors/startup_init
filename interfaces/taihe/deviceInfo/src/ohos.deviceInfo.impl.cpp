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
#include "ohos.deviceInfo.proj.hpp"
#include "ohos.deviceInfo.impl.hpp"
#include "taihe/runtime.hpp"
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
constexpr int DISK_SN_LEN = 20;

typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;

using namespace taihe;
// using namespace ohos::deviceInfo;

namespace {
// To be implemented.

class deviceInfoImpl {
public:
    deviceInfoImpl() {
        // Don't forget to implement the constructor.
    }
};

string getbrand()
{
    const char *value = GetBrand();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdeviceType()
{
    const char *value = GetDeviceType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getproductSeries()
{
    const char *value = GetProductSeries();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getproductModel()
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
#else
    DEVINFO_LOGE("DEPENDENT_APPEXECFWK_BASE does not exist, The ODID could not be obtained");
    ret = DEV_INFO_EGETODID;
#endif

    return ret;
}

string getODID()
{
    static char value[ODID_LEN] = {0};
    DevInfoError ret = AclGetDevOdid(value, ODID_LEN);
    if (ret != DEV_INFO_OK) {
        DEVINFO_LOGE("GetDevOdid ret:%d", ret);
    }
    return value;
}

string getudid()
{
    char value[UDID_LEN] = {0};
    AclGetDevUdid(value, UDID_LEN);
    return value;
}

string getserial()
{
    const char *value = AclGetSerial();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getmanufacture()
{
    const char *value = GetManufacture();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getmarketName()
{
    const char *value = GetMarketName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getproductModelAlias()
{
    const char *value = GetProductModelAlias();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getsoftwareModel()
{
    const char *value = GetSoftwareModel();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string gethardwareModel()
{
    const char *value = GetHardwareModel();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getbootloaderVersion()
{
    const char *value = GetBootloaderVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getabiList()
{
    const char *value = GetAbiList();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getsecurityPatchTag()
{
    const char *value = GetSecurityPatchTag();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdisplayVersion()
{
    const char *value = GetDisplayVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getincrementalVersion()
{
    const char *value = GetIncrementalVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getosReleaseType()
{
    const char *value = GetOsReleaseType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getosFullName()
{
    const char *value = GetOSFullName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getversionId()
{
    const char *value = GetVersionId();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getbuildType()
{
    const char *value = GetBuildType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getbuildUser()
{
    const char *value = GetBuildUser();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getbuildHost()
{
    const char *value = GetBuildHost();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getbuildTime()
{
    const char *value = GetBuildTime();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getbuildRootHash()
{
    const char *value = GetBuildRootHash();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdistributionOSName()
{
    const char *value = GetDistributionOSName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdistributionOSVersion()
{
    const char *value = GetDistributionOSVersion();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdistributionOSApiName()
{
    const char *value = GetDistributionOSApiName();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdistributionOSReleaseType()
{
    const char *value = GetDistributionOSReleaseType();
    if (value == nullptr) {
        value = "";
    }
    return value;
}

string getdiskSN()
{
    static char value[DISK_SN_LEN] = {0};
    AclGetDiskSN(value, DISK_SN_LEN);
    return value;
}

int32_t getsdkApiVersion()
{
    int value = GetSdkApiVersion();
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
}  // namespace

TH_EXPORT_CPP_API_getbrand(getbrand);
TH_EXPORT_CPP_API_getdeviceType(getdeviceType);
TH_EXPORT_CPP_API_getproductSeries(getproductSeries);
TH_EXPORT_CPP_API_getproductModel(getproductModel);
TH_EXPORT_CPP_API_getODID(getODID);
TH_EXPORT_CPP_API_getudid(getudid);
TH_EXPORT_CPP_API_getserial(getserial);
TH_EXPORT_CPP_API_getmanufacture(getmanufacture);
TH_EXPORT_CPP_API_getmarketName(getmarketName);
TH_EXPORT_CPP_API_getproductModelAlias(getproductModelAlias);
TH_EXPORT_CPP_API_getsoftwareModel(getsoftwareModel);
TH_EXPORT_CPP_API_gethardwareModel(gethardwareModel);
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
TH_EXPORT_CPP_API_getmajorVersion(getmajorVersion);
TH_EXPORT_CPP_API_getseniorVersion(getseniorVersion);
TH_EXPORT_CPP_API_getfeatureVersion(getfeatureVersion);
TH_EXPORT_CPP_API_getbuildVersion(getbuildVersion);
TH_EXPORT_CPP_API_getfirstApiVersion(getfirstApiVersion);
TH_EXPORT_CPP_API_getdistributionOSApiVersion(getdistributionOSApiVersion);
