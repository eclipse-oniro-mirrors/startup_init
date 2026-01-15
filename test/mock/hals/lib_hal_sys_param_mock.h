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
#ifndef APPSPAWN_HAL_SYS_PARAM_MOCK_H
#define APPSPAWN_HAL_SYS_PARAM_MOCK_H

const char* MockHalgetdevicetype();

const char* MockHalgetmanufacture();

const char* MockHalgetbrand();

const char* MockHalgetmarketname();

const char* MockHalgetproductseries();

const char* MockHalgetproductmodel();

const char* MockHalgetsoftwaremodel();

const char* MockHalgethardwaremodel();

const char* MockHalgethardwareprofile();

const char* MockHalgetserial();

const char* MockHalgetbootloaderversion();

const char* MockHalgetabilist();

const char* MockHalgetdisplayversion();

const char* MockHalgetincrementalversion();

const char* MockHalgetbuildtype();

const char* MockHalgetbuilduser();

const char* MockHalgetbuildhost();

const char* MockHalgetbuildtime();

int MockHalgetfirstapiversion();

namespace OHOS {
namespace AppSpawn {
class HalSysParam {
public:
    virtual ~HalSysParam() = default;
    virtual const char* HalGetDeviceType() = 0;

    virtual const char* HalGetManufacture() = 0;

    virtual const char* HalGetBrand() = 0;

    virtual const char* HalGetMarketName() = 0;

    virtual const char* HalGetProductSeries() = 0;

    virtual const char* HalGetProductModel() = 0;

    virtual const char* HalGetSoftwareModel() = 0;

    virtual const char* HalGetHardwareModel() = 0;

    virtual const char* HalGetHardwareProfile() = 0;

    virtual const char* HalGetSerial() = 0;

    virtual const char* HalGetBootloaderVersion() = 0;

    virtual const char* HalGetAbiList() = 0;

    virtual const char* HalGetDisplayVersion() = 0;

    virtual const char* HalGetIncrementalVersion() = 0;

    virtual const char* HalGetBuildType() = 0;

    virtual const char* HalGetBuildUser() = 0;

    virtual const char* HalGetBuildHost() = 0;

    virtual const char* HalGetBuildTime() = 0;

    virtual int HalGetFirstApiVersion() = 0;

public:
    static inline std::shared_ptr<HalSysParam> halSysParamFunc = nullptr;
};

class HalSysParamMock : public HalSysParam {
public:

    MOCK_METHOD(const char*, HalGetDeviceType, ());

    MOCK_METHOD(const char*, HalGetManufacture, ());

    MOCK_METHOD(const char*, HalGetBrand, ());

    MOCK_METHOD(const char*, HalGetMarketName, ());

    MOCK_METHOD(const char*, HalGetProductSeries, ());

    MOCK_METHOD(const char*, HalGetProductModel, ());

    MOCK_METHOD(const char*, HalGetSoftwareModel, ());

    MOCK_METHOD(const char*, HalGetHardwareModel, ());

    MOCK_METHOD(const char*, HalGetHardwareProfile, ());

    MOCK_METHOD(const char*, HalGetSerial, ());

    MOCK_METHOD(const char*, HalGetBootloaderVersion, ());

    MOCK_METHOD(const char*, HalGetAbiList, ());

    MOCK_METHOD(const char*, HalGetDisplayVersion, ());

    MOCK_METHOD(const char*, HalGetIncrementalVersion, ());

    MOCK_METHOD(const char*, HalGetBuildType, ());

    MOCK_METHOD(const char*, HalGetBuildUser, ());

    MOCK_METHOD(const char*, HalGetBuildHost, ());

    MOCK_METHOD(const char*, HalGetBuildTime, ());

    MOCK_METHOD(int, HalGetFirstApiVersion, ());

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_HAL_SYS_PARAM_MOCK_H
