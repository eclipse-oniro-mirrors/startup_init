/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "lib_hal_sys_param_mock.h"

using namespace OHOS::AppSpawn;
const char* MockHalgetdevicetype()
{
    return HalSysParam::halSysParamFunc->HalGetDeviceType();
}

const char* MockHalgetmanufacture()
{
    return HalSysParam::halSysParamFunc->HalGetManufacture();
}

const char* MockHalgetbrand()
{
    return HalSysParam::halSysParamFunc->HalGetBrand();
}

const char* MockHalgetmarketname()
{
    return HalSysParam::halSysParamFunc->HalGetMarketName();
}

const char* MockHalgetproductseries()
{
    return HalSysParam::halSysParamFunc->HalGetProductSeries();
}

const char* MockHalgetproductmodel()
{
    return HalSysParam::halSysParamFunc->HalGetProductModel();
}

const char* MockHalgetsoftwaremodel()
{
    return HalSysParam::halSysParamFunc->HalGetSoftwareModel();
}

const char* MockHalgethardwaremodel()
{
    return HalSysParam::halSysParamFunc->HalGetHardwareModel();
}

const char* MockHalgethardwareprofile()
{
    return HalSysParam::halSysParamFunc->HalGetHardwareProfile();
}

const char* MockHalgetserial()
{
    return HalSysParam::halSysParamFunc->HalGetSerial();
}

const char* MockHalgetbootloaderversion()
{
    return HalSysParam::halSysParamFunc->HalGetBootloaderVersion();
}

const char* MockHalgetabilist()
{
    return HalSysParam::halSysParamFunc->HalGetAbiList();
}

const char* MockHalgetdisplayversion()
{
    return HalSysParam::halSysParamFunc->HalGetDisplayVersion();
}

const char* MockHalgetincrementalversion()
{
    return HalSysParam::halSysParamFunc->HalGetIncrementalVersion();
}

const char* MockHalgetbuildtype()
{
    return HalSysParam::halSysParamFunc->HalGetBuildType();
}

const char* MockHalgetbuilduser()
{
    return HalSysParam::halSysParamFunc->HalGetBuildUser();
}

const char* MockHalgetbuildhost()
{
    return HalSysParam::halSysParamFunc->HalGetBuildHost();
}

const char* MockHalgetbuildtime()
{
    return HalSysParam::halSysParamFunc->HalGetBuildTime();
}

int MockHalgetfirstapiversion()
{
    return HalSysParam::halSysParamFunc->HalGetFirstApiVersion();
}

