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

#include "device_info_ffi.h"
#include "device_info.h"
#include "log.h"

namespace OHOS {
namespace CJSystemapi {
namespace DeviceInfo {

extern "C" {
const char* FfiOHOSDeviceInfoGetHardwareProfile()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoGetHardwareProfile");
    return DeviceInfo::CjGetHardwareProfile();
}

const char* FfiOHOSDeviceInfoOsFullName()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoOsFullName");
    return DeviceInfo::CjGetOsFullName();
}

const char* FfiOHOSDeviceInfoProductModel()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoProductModel");
    return DeviceInfo::CjGetProductModel();
}

const char* FfiOHOSDeviceInfoBrand()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBrand");
    return DeviceInfo::CjGetBrand();
}

const char* FfiOHOSDeviceInfoUdid()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoUdid");
    return DeviceInfo::CjGetUdid();
}

const char* FfiOHOSDeviceInfoBuildRootHash()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBuildRootHash");
    return DeviceInfo::CjGetBuildRootHash();
}

const char* FfiOHOSDeviceInfoBuildTime()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBuildTime");
    return DeviceInfo::CjGetBuildTime();
}

const char* FfiOHOSDeviceInfoBuildHost()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBuildHost");
    return DeviceInfo::CjGetBuildHost();
}

const char* FfiOHOSDeviceInfoBuildUser()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBuildUser");
    return DeviceInfo::CjGetBuildUser();
}

const char* FfiOHOSDeviceInfoBuildType()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBuildType");
    return DeviceInfo::CjGetBuildType();
}

const char* FfiOHOSDeviceInfoDeviceType()
{
    LOGI("DeviceInfo::FfiOHOSFILEGetPath");
    return DeviceInfo::CjGetDeviceType();
}

const char* FfiOHOSDeviceInfoVersionId()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoVersionId");
    return DeviceInfo::CjGetVersionId();
}

int64_t FfiOHOSDeviceInfoFirstApiVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoFirstApiVersion");
    return DeviceInfo::CjGetFirstApiVersion();
}

int64_t FfiOHOSDeviceInfoSdkApiVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoSdkApiVersion");
    return DeviceInfo::CjGetSdkApiVersion();
}

int64_t FfiOHOSDeviceInfoBuildVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBuildVersion");
    return DeviceInfo::CjGetBuildVersion();
}

int64_t FfiOHOSDeviceInfoFeatureVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoFeatureVersion");
    return DeviceInfo::CjGetFeatureVersion();
}

int64_t FfiOHOSDeviceInfoSeniorVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoSeniorVersion");
    return DeviceInfo::CjGetSeniorVersion();
}

int64_t FfiOHOSDeviceInfoMajorVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoMajorVersion");
    return DeviceInfo::CjGetMajorVersion();
}

const char* FfiOHOSDeviceInfoDisplayVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoDisplayVersion");
    return DeviceInfo::CjGetDisplayVersion();
}

const char* FfiOHOSDeviceInfoSerial()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoSerial");
    return DeviceInfo::CjGetSerial();
}

const char* FfiOHOSDeviceInfoOsReleaseType()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoOsReleaseType");
    return DeviceInfo::CjGetOsReleaseType();
}

const char* FfiOHOSDeviceInfoIncrementalVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoIncrementalVersion");
    return DeviceInfo::CjGetIncrementalVersion();
}

const char* FfiOHOSDeviceInfoSecurityPatchTag()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoSecurityPatchTag");
    return DeviceInfo::CjGetSecurityPatchTag();
}

const char* FfiOHOSDeviceInfoAbiList()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoAbiList");
    return DeviceInfo::CjGetAbiList();
}

const char* FfiOHOSDeviceInfoBootloaderVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoBootloaderVersion");
    return DeviceInfo::CjGetBootloaderVersion();
}

const char* FfiOHOSDeviceInfoHardwareModel()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoHardwareModel");
    return DeviceInfo::CjGetHardwareModel();
}

const char* FfiOHOSDeviceInfoSoftwareModel()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoSoftwareModel");
    return DeviceInfo::CjGetSoftwareModel();
}

const char* FfiOHOSDeviceInfoProductSeries()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoProductSeries");
    return DeviceInfo::CjGetProductSeries();
}

const char* FfiOHOSDeviceInfoMarketName()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoMarketName");
    return DeviceInfo::CjGetMarketName();
}

const char* FfiOHOSDeviceInfoManufacture()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoManufacture");
    return DeviceInfo::CjGetManufacture();
}

const char* FfiOHOSDeviceInfoDistributionOSName()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoDistributionOSName");
    return DeviceInfo::CjGetDistributionOSName();
}

const char* FfiOHOSDeviceInfoDistributionOSVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoDistributionOSVersion");
    return DeviceInfo::CjGetDistributionOSVersion();
}

int64_t FfiOHOSDeviceInfoDistributionOSApiVersion()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoDistributionOSApiVersion");
    return DeviceInfo::CjGetDistributionOSApiVersion();
}

const char* FfiOHOSDeviceInfoDistributionOSReleaseType()
{
    LOGI("DeviceInfo::FfiOHOSDeviceInfoDistributionOSReleaseType");
    return DeviceInfo::CjGetDistributionOSReleaseType();
}
}

} // DeviceInfo
} // CJSystemapi
} // OHOS