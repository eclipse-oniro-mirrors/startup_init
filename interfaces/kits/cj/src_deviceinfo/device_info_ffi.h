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
    FFI_EXPORT const char* FfiOHOSDeviceInfoGetHardwareProfile();
    FFI_EXPORT const char* FfiOHOSDeviceInfoDeviceType();
    FFI_EXPORT const char* FfiOHOSDeviceInfoOsFullName();
    FFI_EXPORT const char* FfiOHOSDeviceInfoProductModel();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBrand();
    FFI_EXPORT const char* FfiOHOSDeviceInfoUdid();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBuildRootHash();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBuildTime();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBuildHost();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBuildUser();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBuildType();
    FFI_EXPORT const char* FfiOHOSDeviceInfoVersionId();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoFirstApiVersion();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoSdkApiVersion();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoBuildVersion();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoFeatureVersion();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoSeniorVersion();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoMajorVersion();
    FFI_EXPORT const char* FfiOHOSDeviceInfoDisplayVersion();
    FFI_EXPORT const char* FfiOHOSDeviceInfoSerial();
    FFI_EXPORT const char* FfiOHOSDeviceInfoOsReleaseType();
    FFI_EXPORT const char* FfiOHOSDeviceInfoIncrementalVersion();
    FFI_EXPORT const char* FfiOHOSDeviceInfoSecurityPatchTag();
    FFI_EXPORT const char* FfiOHOSDeviceInfoAbiList();
    FFI_EXPORT const char* FfiOHOSDeviceInfoBootloaderVersion();
    FFI_EXPORT const char* FfiOHOSDeviceInfoHardwareModel();
    FFI_EXPORT const char* FfiOHOSDeviceInfoSoftwareModel();
    FFI_EXPORT const char* FfiOHOSDeviceInfoProductSeries();
    FFI_EXPORT const char* FfiOHOSDeviceInfoMarketName();
    FFI_EXPORT const char* FfiOHOSDeviceInfoManufacture();
    FFI_EXPORT const char* FfiOHOSDeviceInfoDistributionOSName();
    FFI_EXPORT const char* FfiOHOSDeviceInfoDistributionOSVersion();
    FFI_EXPORT int64_t FfiOHOSDeviceInfoDistributionOSApiVersion();
    FFI_EXPORT const char* FfiOHOSDeviceInfoDistributionOSReleaseType();
}

#endif // OHOS_DEVICE_INFO_FFI_H