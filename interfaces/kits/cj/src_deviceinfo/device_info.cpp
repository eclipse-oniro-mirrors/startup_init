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

#include <string>
#include <memory>

namespace OHOS {
namespace CJSystemapi {
namespace DeviceInfo {

const int UDID_LEN = 65;

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
    char* udid = static_cast<char*>(malloc(UDID_LEN));
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

} // DeviceInfo
} // CJSystemapi
} // OHOS
