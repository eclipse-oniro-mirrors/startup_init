/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "parameter.h"
#include "sysversion.h"
#include "device_info.h"
#ifdef DEPENDENT_APPEXECFWK_BASE
#include "bundlemgr/bundle_mgr_proxy.h"
#endif
#include "iservice_registry.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
#include "beget_ext.h"
#include "init_error.h"
#include "securec.h"
#include "log.h"

#include <string>
#include <memory>
#include <threads.h>

namespace OHOS {
namespace CJSystemapi {
namespace DeviceInfo {

const int UDID_LEN = 65;
const int ODID_LEN = 37;

typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;

const char* DeviceInfo::CjGetHardwareProfile()
{
    return GetHardwareProfile();
}

const char* DeviceInfo::CjGetOsFullName()
{
    return GetOSFullName();
}

const char* DeviceInfo::CjGetProductModel()
{
    return GetProductModel();
}

const char* DeviceInfo::CjGetBrand()
{
    return GetBrand();
}

const char* DeviceInfo::CjGetDeviceType()
{
    return GetDeviceType();
}

const char* DeviceInfo::CjGetUdid()
{
    char* udid = static_cast<char*>(calloc(1, UDID_LEN));
    if (udid == nullptr) {
        return nullptr;
    }
    int res = AclGetDevUdid(udid, UDID_LEN);
    if (res != 0) {
        free(udid);
        return nullptr;
    }
    return udid;
}

const char* DeviceInfo::CjGetBuildRootHash()
{
    return GetBuildRootHash();
}

const char* DeviceInfo::CjGetBuildTime()
{
    return GetBuildTime();
}

const char* DeviceInfo::CjGetBuildHost()
{
    return GetBuildHost();
}

const char* DeviceInfo::CjGetBuildUser()
{
    return GetBuildUser();
}

const char* DeviceInfo::CjGetBuildType()
{
    return GetBuildType();
}

const char* DeviceInfo::CjGetVersionId()
{
    return GetVersionId();
}

int64_t DeviceInfo::CjGetFirstApiVersion()
{
    return GetFirstApiVersion();
}

int64_t DeviceInfo::CjGetSdkApiVersion()
{
    return GetSdkApiVersion();
}

int64_t DeviceInfo::CjGetBuildVersion()
{
    return GetBuildVersion();
}

int64_t DeviceInfo::CjGetFeatureVersion()
{
    return GetFeatureVersion();
}
    
int64_t DeviceInfo::CjGetSeniorVersion()
{
    return GetSeniorVersion();
}

int64_t DeviceInfo::CjGetMajorVersion()
{
    return GetMajorVersion();
}

const char* DeviceInfo::CjGetDisplayVersion()
{
    return GetDisplayVersion();
}

const char* DeviceInfo::CjGetSerial()
{
    return AclGetSerial();
}

const char* DeviceInfo::CjGetOsReleaseType()
{
    return GetOsReleaseType();
}

const char* DeviceInfo::CjGetIncrementalVersion()
{
    return GetIncrementalVersion();
}

const char* DeviceInfo::CjGetSecurityPatchTag()
{
    return GetSecurityPatchTag();
}

const char* DeviceInfo::CjGetAbiList()
{
    return GetAbiList();
}

const char* DeviceInfo::CjGetBootloaderVersion()
{
    return GetBootloaderVersion();
}

const char* DeviceInfo::CjGetHardwareModel()
{
    return GetHardwareModel();
}

const char* DeviceInfo::CjGetSoftwareModel()
{
    return GetSoftwareModel();
}

const char* DeviceInfo::CjGetProductSeries()
{
    return GetProductSeries();
}

const char* DeviceInfo::CjGetMarketName()
{
    return GetMarketName();
}

const char* DeviceInfo::CjGetManufacture()
{
    return GetManufacture();
}

const char* DeviceInfo::CjGetDistributionOSName()
{
    return GetDistributionOSName();
}

const char* DeviceInfo::CjGetDistributionOSVersion()
{
    return GetDistributionOSVersion();
}

int64_t DeviceInfo::CjGetDistributionOSApiVersion()
{
    return GetDistributionOSApiVersion();
}

const char* DeviceInfo::CjGetDistributionOSReleaseType()
{
    return GetDistributionOSReleaseType();
}

static DevInfoError CjAclGetDevOdid(char *odid, int size)
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
    LOGE("DEPENDENT_APPEXECFWK_BASE does not exist, The ODID could not be obtained");
    ret = DEV_INFO_EGETODID;
#endif

    return ret;
}

const char* DeviceInfo::CjGetDevOdid()
{
    thread_local char odid[ODID_LEN] = {0};
    DevInfoError ret = CjAclGetDevOdid(odid, ODID_LEN);
    if (ret != DEV_INFO_OK) {
        LOGE("GetDevOdid ret:%d", ret);
        return "";
    }
    return odid;
}

const char* DeviceInfo::CjGetDistributionOSApiName()
{
    return GetDistributionOSApiName();
}

} // DeviceInfo
} // CJSystemapi
} // OHOS
