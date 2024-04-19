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

#ifndef OHOS_DEVICE_INFO_FFI_H
#define OHOS_DEVICE_INFO_FFI_H

#include "cj_common_ffi.h"
#include <cstdint>

extern "C" {
    const char* FfiOHOSDeviceInfoGetHardwareProfile();
    const char* FfiOHOSDeviceInfoDeviceType();
    const char* FfiOHOSDeviceInfoOsFullName();
    const char* FfiOHOSDeviceInfoProductModel();
    const char* FfiOHOSDeviceInfoBrand();
    const char* FfiOHOSDeviceInfoUdid();
    const char* FfiOHOSDeviceInfoBuildRootHash();
    const char* FfiOHOSDeviceInfoBuildTime();
    const char* FfiOHOSDeviceInfoBuildHost();
    const char* FfiOHOSDeviceInfoBuildUser();
    const char* FfiOHOSDeviceInfoBuildType();
    const char* FfiOHOSDeviceInfoVersionId();
    int64_t FfiOHOSDeviceInfoFirstApiVersion();
    int64_t FfiOHOSDeviceInfoSdkApiVersion();
    int64_t FfiOHOSDeviceInfoBuildVersion();
    int64_t FfiOHOSDeviceInfoFeatureVersion();
    int64_t FfiOHOSDeviceInfoSeniorVersion();
    int64_t FfiOHOSDeviceInfoMajorVersion();
    const char* FfiOHOSDeviceInfoDisplayVersion();
    const char* FfiOHOSDeviceInfoSerial();
    const char* FfiOHOSDeviceInfoOsReleaseType();
    const char* FfiOHOSDeviceInfoIncrementalVersion();
    const char* FfiOHOSDeviceInfoSecurityPatchTag();
    const char* FfiOHOSDeviceInfoAbiList();
    const char* FfiOHOSDeviceInfoBootloaderVersion();
    const char* FfiOHOSDeviceInfoHardwareModel();
    const char* FfiOHOSDeviceInfoSoftwareModel();
    const char* FfiOHOSDeviceInfoProductSeries();
    const char* FfiOHOSDeviceInfoMarketName();
    const char* FfiOHOSDeviceInfoManufacture();
    const char* FfiOHOSDeviceInfoDistributionOSName();
    const char* FfiOHOSDeviceInfoDistributionOSVersion();
    int64_t FfiOHOSDeviceInfoDistributionOSApiVersion();
    const char* FfiOHOSDeviceInfoDistributionOSReleaseType();
}

#endif // OHOS_DEVICE_INFO_FFI_H