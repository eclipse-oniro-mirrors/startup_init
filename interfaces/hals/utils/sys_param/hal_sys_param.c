/*
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd.
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
#include "hal_sys_param.h"

static const char OHOS_ABI_LIST[] = {"****"};
static const char OHOS_BOOTLOADER_VERSION[] = {"bootloader"};
static const char OHOS_BRAND[] = {"****"};
static const char OHOS_DEVICE_TYPE[] = {"****"};
static const char OHOS_DISPLAY_VERSION[] = {"OpenHarmony 3.1"};
static const char OHOS_HARDWARE_MODEL[] = {"****"};
static const char OHOS_HARDWARE_PROFILE[] = {"aout:true,display:true"};
static const char OHOS_MARKET_NAME[] = {"****"};
static const char OHOS_MANUFACTURE[] = {"****"};
static const char OHOS_PRODUCT_MODEL[] = {"****"};
static const char OHOS_PRODUCT_SERIES[] = {"****"};
static const char OHOS_SERIAL[] = {"****"};  // provided by OEM.
static const char OHOS_SOFTWARE_MODEL[] = {"****"};
static const int OHOS_FIRST_API_VERSION = 1;

__attribute__((weak)) const char* HalGetDeviceType(void)
{
    return OHOS_DEVICE_TYPE;
}

__attribute__((weak)) const char* HalGetManufacture(void)
{
    return OHOS_MANUFACTURE;
}

__attribute__((weak)) const char* HalGetBrand(void)
{
    return OHOS_BRAND;
}

__attribute__((weak)) const char* HalGetMarketName(void)
{
    return OHOS_MARKET_NAME;
}

__attribute__((weak)) const char* HalGetProductSeries(void)
{
    return OHOS_PRODUCT_SERIES;
}

__attribute__((weak)) const char* HalGetProductModel(void)
{
    return OHOS_PRODUCT_MODEL;
}

__attribute__((weak)) const char* HalGetSoftwareModel(void)
{
    return OHOS_SOFTWARE_MODEL;
}

__attribute__((weak)) const char* HalGetHardwareModel(void)
{
    return OHOS_HARDWARE_MODEL;
}

__attribute__((weak)) const char* HalGetHardwareProfile(void)
{
    return OHOS_HARDWARE_PROFILE;
}

__attribute__((weak)) const char* HalGetSerial(void)
{
    return OHOS_SERIAL;
}

__attribute__((weak)) const char* HalGetBootloaderVersion(void)
{
    return OHOS_BOOTLOADER_VERSION;
}

__attribute__((weak)) const char* HalGetAbiList(void)
{
    return OHOS_ABI_LIST;
}

__attribute__((weak)) const char* HalGetDisplayVersion(void)
{
    return OHOS_DISPLAY_VERSION;
}

__attribute__((weak)) const char* HalGetIncrementalVersion(void)
{
    return INCREMENTAL_VERSION;
}

__attribute__((weak)) const char* HalGetBuildType(void)
{
    return BUILD_TYPE;
}

__attribute__((weak)) const char* HalGetBuildUser(void)
{
    return BUILD_USER;
}

__attribute__((weak)) const char* HalGetBuildHost(void)
{
    return BUILD_HOST;
}

__attribute__((weak)) const char* HalGetBuildTime(void)
{
    return BUILD_TIME;
}

__attribute__((weak)) int HalGetFirstApiVersion(void)
{
    return OHOS_FIRST_API_VERSION;
}