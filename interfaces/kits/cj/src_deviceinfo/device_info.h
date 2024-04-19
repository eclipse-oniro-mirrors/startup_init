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

#ifndef OHOS_DEVICE_INFO_H
#define OHOS_DEVICE_INFO_H

#include <string>
#include <memory>

namespace OHOS {
namespace CJSystemapi {
namespace DeviceInfo {

class DeviceInfo {
public:
    static const char* CjGetHardwareProfile();
    static const char* CjGetOsFullName();
    static const char* CjGetProductModel();
    static const char* CjGetBrand();
    static const char* CjGetDeviceType();
    static const char* CjGetUdid();
    static const char* CjGetBuildRootHash();
    static const char* CjGetBuildTime();
    static const char* CjGetBuildHost();
    static const char* CjGetBuildUser();
    static const char* CjGetBuildType();
    static const char* CjGetVersionId();
    static int64_t CjGetFirstApiVersion();
    static int64_t CjGetSdkApiVersion();
    static int64_t CjGetBuildVersion();
    static int64_t CjGetFeatureVersion();
    static int64_t CjGetSeniorVersion();
    static int64_t CjGetMajorVersion();
    static const char* CjGetSerial();
    static const char* CjGetDisplayVersion();
    static const char* CjGetOsReleaseType();
    static const char* CjGetIncrementalVersion();
    static const char* CjGetSecurityPatchTag();
    static const char* CjGetAbiList();
    static const char* CjGetBootloaderVersion();
    static const char* CjGetHardwareModel();
    static const char* CjGetSoftwareModel();
    static const char* CjGetProductSeries();
    static const char* CjGetMarketName();
    static const char* CjGetManufacture();
    static const char* CjGetDistributionOSName();
    static const char* CjGetDistributionOSVersion();
    static int64_t CjGetDistributionOSApiVersion();
    static const char* CjGetDistributionOSReleaseType();
};

} // DeviceInfo
} // CJSystemapi
} // OHOS

#endif // OHOS_DEVICE_INFO_H