/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "gtest/gtest.h"

#include "init_param.h"
#include "init_utils.h"
#include "parameter.h"
#include "param_comm.h"
#include "param_stub.h"
#ifndef OHOS_LITE
#include "param_wrapper.h"
#include "parameters.h"
#endif
#include "sysversion.h"
#include "sysparam_errno.h"

using namespace testing::ext;

namespace OHOS {
constexpr int TEST_VALUE = 101;
class SysparaUnitTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp()
    {
        SetTestPermissionResult(0);
    }
    void TearDown() {}
};

HWTEST_F(SysparaUnitTest, parameterTest001, TestSize.Level0)
{
    const char *deviceType = GetDeviceType();
    printf("Device type =%s\n", deviceType);
    EXPECT_STRNE(deviceType, nullptr);
    const char *manufacture = GetManufacture();
    printf("Manufacture =%s\n", manufacture);
    EXPECT_STRNE(manufacture, nullptr);
    const char *brand = GetBrand();
    printf("GetBrand =%s\n", brand);
    EXPECT_STRNE(brand, nullptr);
    const char *marketName = GetMarketName();
    printf("MarketName =%s\n", marketName);
    EXPECT_STRNE(marketName, nullptr);
    const char *productSeries = GetProductSeries();
    printf("ProductSeries =%s\n", productSeries);
    EXPECT_STRNE(productSeries, nullptr);
    const char *productModel = GetProductModel();
    printf("ProductModel =%s\n", GetProductModel());
    EXPECT_STRNE(productModel, nullptr);
    const char *roductModelAlias = GetProductModelAlias();
    printf("ProductModelAlias =%s\n", roductModelAlias);
    EXPECT_STRNE(roductModelAlias, nullptr);
    const char *softwareModel = GetSoftwareModel();
    printf("SoftwareModel =%s\n", softwareModel);
    EXPECT_STRNE(softwareModel, nullptr);
    const char *hardwareModel = GetHardwareModel();
    printf("HardwareModel =%s\n", hardwareModel);
    EXPECT_STRNE(hardwareModel, nullptr);
    const char *softwareProfile = GetHardwareProfile();
    printf("Software profile =%s\n", softwareProfile);
    EXPECT_STRNE(softwareProfile, nullptr);
    const char *serial = GetSerial();
    printf("Serial =%s\n", serial);
    EXPECT_STRNE(serial, nullptr);
    const char *osFullName = GetOSFullName();
    printf("OS full name =%s\n", osFullName);
    EXPECT_STRNE(osFullName, nullptr);
    const char *osReleaseType = GetOsReleaseType();
    printf("OS Release type =%s\n", osReleaseType);
    EXPECT_STRNE(osReleaseType, nullptr);
    const char *displayVersion = GetDisplayVersion();
    printf("Display version =%s\n", displayVersion);
    EXPECT_STRNE(displayVersion, nullptr);
    const char *bootloaderVersion = GetBootloaderVersion();
    printf("bootloader version =%s\n", bootloaderVersion);
    EXPECT_STRNE(bootloaderVersion, nullptr);
    const char *securePatchLevel = GetSecurityPatchTag();
    printf("secure patch level =%s\n", securePatchLevel);
    EXPECT_STRNE(securePatchLevel, nullptr);
}

HWTEST_F(SysparaUnitTest, parameterTest001_1, TestSize.Level0)
{
    const char *securePatchLevel = GetSecurityPatchTag();
    printf("secure patch level =%s\n", securePatchLevel);
    EXPECT_STRNE(securePatchLevel, nullptr);
    const char *abiList = GetAbiList();
    printf("abi list =%s\n", abiList);
    EXPECT_STRNE(abiList, nullptr);
    int firstApiVersion = GetFirstApiVersion();
    printf("first api version =%d\n", firstApiVersion);
    EXPECT_NE(firstApiVersion, -1);
    int sdkApiVersion = GetSdkApiVersion();
    printf("SDK api version =%d\n", sdkApiVersion);
    EXPECT_NE(sdkApiVersion, -1);
    int sdkMinorApiVersion = GetSdkMinorApiVersion();
    printf("SDK MinorApi version =%d\n", sdkMinorApiVersion);
    EXPECT_EQ((sdkMinorApiVersion >= -1), true);
    int sdkPatchApiVersion = GetSdkPatchApiVersion();
    printf("SDK PatchApi version =%d\n", sdkPatchApiVersion);
    EXPECT_EQ((sdkPatchApiVersion >= -1), true);
    const char *incrementalVersion = GetIncrementalVersion();
    printf("Incremental version = %s\n", incrementalVersion);
    EXPECT_STRNE(incrementalVersion, nullptr);
    const char *formalId = GetVersionId();
    printf("formal id =%s\n", formalId);
    EXPECT_STRNE(formalId, nullptr);
    const char *buildType = GetBuildType();
    printf("build type =%s\n", buildType);
    EXPECT_STRNE(buildType, nullptr);
    const char *buildUser = GetBuildUser();
    printf("build user =%s\n", buildUser);
    EXPECT_STRNE(buildUser, nullptr);
    const char *buildHost = GetBuildHost();
    printf("Build host = %s\n", buildHost);
    EXPECT_STRNE(buildHost, nullptr);
    const char *buildTime = GetBuildTime();
    printf("build time =%s\n", buildTime);
    EXPECT_STRNE(buildTime, nullptr);
    const char *buildRootLater = GetBuildRootHash();
    printf("build root later..., %s\n", buildRootLater);
    EXPECT_STRNE(buildRootLater, nullptr);
}

HWTEST_F(SysparaUnitTest, parameterTest001_2, TestSize.Level0)
{
    EXPECT_STRNE(GetDeviceType(), nullptr);
    EXPECT_STRNE(GetManufacture(), nullptr);
    EXPECT_STRNE(GetBrand(), nullptr);
    EXPECT_STRNE(GetMarketName(), nullptr);
    EXPECT_STRNE(GetProductSeries(), nullptr);
    EXPECT_STRNE(GetProductModel(), nullptr);
    EXPECT_STRNE(GetProductModelAlias(), nullptr);
    EXPECT_STRNE(GetSoftwareModel(), nullptr);
    EXPECT_STRNE(GetHardwareModel(), nullptr);
    EXPECT_STRNE(GetHardwareProfile(), nullptr);
    EXPECT_STRNE(GetOSFullName(), nullptr);
    EXPECT_STRNE(GetOsReleaseType(), nullptr);
    EXPECT_STRNE(GetDisplayVersion(), nullptr);
    EXPECT_STRNE(GetBootloaderVersion(), nullptr);
    EXPECT_STRNE(GetSecurityPatchTag(), nullptr);
}

HWTEST_F(SysparaUnitTest, parameterTest001_3, TestSize.Level0)
{
    EXPECT_STRNE(GetSecurityPatchTag(), nullptr);
    EXPECT_STRNE(GetAbiList(), nullptr);
    EXPECT_GT(GetFirstApiVersion(), 0);
    EXPECT_GT(GetSdkApiVersion(), 0);
    EXPECT_STRNE(GetIncrementalVersion(), nullptr);
    EXPECT_STRNE(GetVersionId(), nullptr);
    EXPECT_STRNE(GetBuildType(), nullptr);
    EXPECT_STRNE(GetBuildUser(), nullptr);
    EXPECT_STRNE(GetBuildHost(), nullptr);
    EXPECT_STRNE(GetBuildTime(), nullptr);
    EXPECT_STRNE(GetBuildRootHash(), nullptr);
    EXPECT_STRNE(GetChipType(), nullptr);
    EXPECT_GT(GetBootCount(), -1);
}

HWTEST_F(SysparaUnitTest, parameterTest002, TestSize.Level0)
{
    char key1[] = "test.ro.sys.version";
    char value1[] = "set read only key";
    int ret = SetParameter(key1, value1);
    EXPECT_EQ(ret, EC_SUCCESS);
    ret = SetParameter(nullptr, nullptr);
    EXPECT_EQ(ret, EC_INVALID);
}

HWTEST_F(SysparaUnitTest, parameterTest003, TestSize.Level0)
{
    char key2[] = "rw.sys.version*%version";
    char value2[] = "set value with illegal key";
    int ret = SetParameter(key2, value2);
    EXPECT_EQ(ret, EC_INVALID);
}

/* key = 32 */
HWTEST_F(SysparaUnitTest, parameterTest004, TestSize.Level0)
{
    char key3[] = "rw.sys.version.utilskvparameter0";
    char value3[] = "set with key = 32";
    int ret = SetParameter(key3, value3);
    EXPECT_EQ(ret, EC_SUCCESS);
}

/* value > 128 */
HWTEST_F(SysparaUnitTest, parameterTest005, TestSize.Level0)
{
    char key4[] = "rw.sys.version.version";
    char value4[] = "rw.sys.version.version.version.version flash_offset = *(hi_u32 *)DT_SetGetU32(&g_Element[0], 0)a\
    size = *(hi_u32 *)DT_SetGetU32(&g_Element[1], 0)a";
    int ret = SetParameter(key4, value4);
    EXPECT_EQ(ret, SYSPARAM_INVALID_VALUE);
}

HWTEST_F(SysparaUnitTest, parameterTest006, TestSize.Level0)
{
    char key1[] = "rw.product.not.exist";
    char value1[64] = {0};
    char defValue1[] = "value of key not exist...";
    int ret = GetParameter(key1, defValue1, value1, 64);
    EXPECT_EQ(ret, static_cast<int>(strlen(defValue1)));
}

HWTEST_F(SysparaUnitTest, parameterTest007, TestSize.Level0)
{
    char key2[] = "rw.sys.version.version.version.version";
    char value2[64] = {0};
    char defValue2[] = "value of key > 32 ...";
    int ret = GetParameter(key2, defValue2, value2, 64);
    EXPECT_EQ(ret, static_cast<int>(strlen(defValue2)));
}

HWTEST_F(SysparaUnitTest, parameterTest008, TestSize.Level0)
{
    char key4[] = "test.rw.sys.version";
    char* value4 = nullptr;
    char defValue3[] = "value of key > 32 ...";
    int ret = GetParameter(key4, defValue3, value4, 0);
    EXPECT_EQ(ret, EC_INVALID);
}

HWTEST_F(SysparaUnitTest, parameterTest009, TestSize.Level0)
{
    char key5[] = "test.rw.product.type.2222222";
    char value5[] = "rw.sys.version.version.version.version     \
    flash_offset = *(hi_u32 *)DT_SetGetU32(&g_Element[0], 0)";
    int ret = SetParameter(key5, value5);
    EXPECT_EQ(ret, SYSPARAM_INVALID_VALUE);
    char valueGet[2] = {0};
    char defValue3[] = "value of key > 32 ...";
    ret = GetParameter(key5, defValue3, valueGet, 2);
    EXPECT_EQ(ret, EC_INVALID);
}

HWTEST_F(SysparaUnitTest, parameterTest0010, TestSize.Level0)
{
    char key1[] = "test.rw.sys.version";
    char value1[] = "10.1.0";
    int ret = SetParameter(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    char valueGet1[32] = {0};
    ret = GetParameter(key1, "version=10.1.0", valueGet1, 32);
    EXPECT_EQ(ret, static_cast<int>(strlen(valueGet1)));

    char key2[] = "test.rw.product.type";
    char value2[] = "wifi_iot";
    ret = SetParameter(key2, value2);
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam(key2, value2);
    EXPECT_EQ(ret, 0);
    char valueGet2[32] = {0};
    ret = GetParameter(key2, "version=10.1.0", valueGet2, 32);
    EXPECT_EQ(ret, static_cast<int>(strlen(valueGet2)));

    char key3[] = "test.rw.product.manufacturer";
    char value3[] = "TEST MANUFACTURER";
    ret = SetParameter(key3, value3);
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam(key3, value3);
    EXPECT_EQ(ret, 0);
    char valueGet3[32] = {0};
    ret = GetParameter(key3, "version=10.1.0", valueGet3, 32);
    EXPECT_EQ(ret, static_cast<int>(strlen(valueGet3)));

    char key4[] = "test.rw.product.marketname";
    char value4[] = "TEST MARKETNAME";
    ret = SetParameter(key4, value4);
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam(key4, value4);
    EXPECT_EQ(ret, 0);
    char valueGet4[32] = {0};
    ret = GetParameter(key4, "version=10.1.0", valueGet4, 32);
    EXPECT_EQ(ret, static_cast<int>(strlen(valueGet4)));
}

HWTEST_F(SysparaUnitTest, parameterTest0011, TestSize.Level0)
{
    char key1[] = "test.rw.sys.version.wait1";
    char value1[] = "10.1.0";
    int ret = SetParameter(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = WaitParameter(key1, value1, 10);
    EXPECT_EQ(ret, 0);
    ret = WaitParameter(key1, "*", 10);
    EXPECT_EQ(ret, 0);
    char key2[] = "test.rw.sys.version.wait2";
    ret = WaitParameter(key2, "*", 1);
    EXPECT_EQ(ret, SYSPARAM_WAIT_TIMEOUT);
}

HWTEST_F(SysparaUnitTest, parameterTest0012, TestSize.Level0)
{
    char key1[] = "test.rw.sys.version.version1";
    char value1[] = "10.1.0";
    int ret = SetParameter(key1, value1);
    EXPECT_EQ(ret, 0);

    ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    // success
    unsigned int handle = FindParameter(key1);
    EXPECT_NE(handle, static_cast<unsigned int>(-1));
    char valueGet1[32] = {0};
    ret = GetParameterValue(handle, valueGet1, 32);
    EXPECT_EQ(ret, static_cast<int>(strlen(valueGet1)));
    char nameGet1[32] = {0};
    ret = GetParameterName(handle, nameGet1, 32);
    EXPECT_EQ(ret, static_cast<int>(strlen(nameGet1)));

    // fail
    char key2[] = "test.rw.sys.version.version2";
    handle = FindParameter(key2);
    EXPECT_EQ(handle, static_cast<unsigned int>(-1));
    ret = GetParameterValue(handle, valueGet1, 32);
    EXPECT_EQ(ret, SYSPARAM_NOT_FOUND);
    ret = GetParameterName(handle, nameGet1, 32);
    EXPECT_EQ(ret, SYSPARAM_NOT_FOUND);
    ret = GetParameterValue(handle, nullptr, 32);
    EXPECT_EQ(ret, EC_INVALID);
    ret = GetParameterCommitId(handle);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(SysparaUnitTest, parameterTest0013, TestSize.Level0)
{
    long long int out = 0;
    unsigned long long int uout = 0;
    GetParameter_(nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(GetIntParameter("test.int.get", 0) == -TEST_VALUE, 1);
    EXPECT_EQ(GetUintParameter("test.int.get", 0), 0);
    EXPECT_EQ(GetIntParameter("test.uint.get", 0), TEST_VALUE);
    EXPECT_EQ(GetUintParameter("test.uint.get", 0), TEST_VALUE);
    EXPECT_EQ(GetIntParameter("test.int.default", 10), 10); // key not find,value = default
    EXPECT_EQ(GetUintParameter("test.uint.default", 10), 10); // key not find,value = default
    EXPECT_EQ(IsValidParamValue(nullptr, 0), 0);
    EXPECT_EQ(IsValidParamValue("testvalue", strlen("testvalue") + 1), 1);
    EXPECT_EQ(StringToLL("0x11", &out), 0);
    EXPECT_EQ(StringToULL("0x11", &uout), 0);
    EXPECT_EQ(StringToLL("not vailed", &out), -1);
    EXPECT_EQ(StringToULL("not vailed", &uout), -1);
    char udid[UDID_LEN] = {0};
    GetDevUdid(udid, UDID_LEN);
    EXPECT_NE(GetMajorVersion(), 0);
    int ret = GetDevUdid(nullptr, UDID_LEN);
    EXPECT_EQ(ret, EC_FAILURE);
    GetSeniorVersion();
    GetFeatureVersion();
    GetBuildVersion();
}

#ifndef OHOS_LITE
// for test param_wrapper.cpp
HWTEST_F(SysparaUnitTest, parameterTest0014, TestSize.Level0)
{
    const std::string key1 = "test.int.get";
    OHOS::system::SetParameter(std::string("testKey"), std::string("testValue"));
    int v = OHOS::system::GetIntParameter(key1, 0);
    EXPECT_EQ(v, -TEST_VALUE);
    int8_t v1 = OHOS::system::GetIntParameter(key1, 0, -127, 128); // -127, 128 range
    EXPECT_EQ(v1, -TEST_VALUE);
    int16_t v2 = OHOS::system::GetIntParameter(key1, 0, -127, 128); // -127, 128 range
    EXPECT_EQ(v2, -TEST_VALUE);
    int32_t v3 = OHOS::system::GetIntParameter(key1, 0, -127, 128); // -127, 128 range
    EXPECT_EQ(v3, -TEST_VALUE);
    int64_t v4 = OHOS::system::GetIntParameter(key1, 0, -127, 128); // -127, 128 range
    EXPECT_EQ(v4, -TEST_VALUE);

    int8_t v5 = OHOS::system::GetIntParameter(key1, 0, -10, 10); // -10, 10 range
    EXPECT_EQ(v5, 0);

    const std::string key2 = "test.uint.get";
    uint8_t u1 = OHOS::system::GetUintParameter<uint8_t>(key2, 0, (uint8_t)255); // 255 max value
    EXPECT_EQ(u1, TEST_VALUE);
    uint16_t u2 = OHOS::system::GetUintParameter<uint16_t>(key2, 0,  (uint16_t)255); // 255 max value
    EXPECT_EQ(u2, TEST_VALUE);
    uint32_t u3 = OHOS::system::GetUintParameter<uint32_t>(key2, 0,  (uint32_t)255); // 255 max value
    EXPECT_EQ(u3, TEST_VALUE);
    uint64_t u4 = OHOS::system::GetUintParameter<uint64_t>(key2, 0,  (uint64_t)255); // 255 max value
    EXPECT_EQ(u4 == TEST_VALUE, 1);
    const std::string key3 = "test.uint.get3";
    u1 = OHOS::system::GetUintParameter<uint8_t>(key3, 0, (uint8_t)255); // 255 max value
    EXPECT_EQ(u1, 0);
    u1 = OHOS::system::GetUintParameter<uint8_t>(key2, 0, (uint8_t)10); // 10 max value
    EXPECT_EQ(u1, 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0015, TestSize.Level0)
{
    std::string type = OHOS::system::GetDeviceType();
    printf("device type %s \n", type.c_str());

    const std::string key1 = "test.string.get";
    std::string v1 = OHOS::system::GetParameter(key1, "");
    EXPECT_EQ(strcmp(v1.c_str(), "101"), 0);

    const std::string key2 = "test.string.get2";
    v1 = OHOS::system::GetParameter(key2, "test2");
    EXPECT_EQ(strcmp(v1.c_str(), "test2"), 0);

    int ret = OHOS::system::GetStringParameter(key1, v1, "");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(strcmp(v1.c_str(), "101"), 0);
    ret = OHOS::system::GetStringParameter(key2, v1, "test2");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(strcmp(v1.c_str(), "test2"), 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0016, TestSize.Level0)
{
    const std::string key1 = "test.bool.get.true";
    bool ret = OHOS::system::GetBoolParameter(key1, false);
    EXPECT_EQ(ret, true);
    const std::string key2 = "test.bool.get.false";
    ret = OHOS::system::GetBoolParameter(key2, true);
    EXPECT_EQ(ret, false);
    const std::string key3 = "test.bool.get3";
    ret = OHOS::system::GetBoolParameter(key3, false);
    EXPECT_EQ(ret, false);
}

HWTEST_F(SysparaUnitTest, parameterTest0017, TestSize.Level0)
{
    printf("distributionOS name =%s\n", GetDistributionOSName());
    EXPECT_STRNE(GetDistributionOSName(), nullptr);
    printf("distributionOS version =%s\n", GetDistributionOSVersion());
    EXPECT_STRNE(GetDistributionOSVersion(), nullptr);
    printf("distributionOS api version =%d\n", GetDistributionOSApiVersion());
    EXPECT_GT(GetDistributionOSApiVersion(), 0);
    printf("distributionOS name =%s\n", GetDistributionOSReleaseType());
    EXPECT_STRNE(GetDistributionOSReleaseType(), nullptr);
    printf("distributionOS api name =%s\n", GetDistributionOSApiName());
    printf("PerformanceClassLevel =%d\n", GetPerformanceClass());
    EXPECT_GE(GetPerformanceClass(), 0);
    EXPECT_LE(GetPerformanceClass(), 2);
}
#endif

HWTEST_F(SysparaUnitTest, parameterTest0018, TestSize.Level0)
{
    char key1[] = "test.ro.sys.version";
    char value1[] = "set read only key";
    int ret = SetParameter(key1, value1);
    EXPECT_EQ(ret, EC_SUCCESS);
    char key2[] = "persist.test.ro.sys.version";
    char value2[] = "set persist read only key";
    ret = SetParameter(key2, value2);
    EXPECT_EQ(ret, EC_SUCCESS);
    ret = SaveParameters();
    EXPECT_EQ(ret, 0);
}

#ifndef OHOS_LITE
HWTEST_F(SysparaUnitTest, parameterTest0019, TestSize.Level0)
{
    char key1[] = "const.test.for_update_test";
    char key2[] = "persist.test.for_update_test";
    char value1[] = "initSet";
    char value2[] = "initUpdate";

    int ret = SystemUpdateConstParam(key1, value2);
    EXPECT_EQ(ret, PARAM_CODE_INVALID_NAME);
    ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1, value2);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key2, value2);
    EXPECT_EQ(ret, PARAM_CODE_INVALID_NAME);
}

HWTEST_F(SysparaUnitTest, parameterTest0020, TestSize.Level0)
{
    char key1[] = "const.test.for_update_test1";
    char value1[] = "initSet"; // len < 96
    char value2[] = "initUpdate_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96

    int ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1, value2);
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);
}

HWTEST_F(SysparaUnitTest, parameterTest0021, TestSize.Level0)
{
    char key1[] = "const.test.for_update_test2";
    char value1[] = "initSet_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96
    char value2[] = "initUpdate_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96

    int ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1, value2);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0022, TestSize.Level0)
{
    char key1[] = "const.test.for_update_test3";
    char value1[] = "initSet_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghi"; // len = 96
    char value2[] = "initUpdate_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdef"; // len = 96

    int ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1, value2);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0023, TestSize.Level0)
{
    char key1[] = "const.test.for_update_test4";
    char value1[] = "initSet_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96
    char value2[] = "initUpdate_abcdefghijkl"; // len < 96

    int ret = SystemWriteParam(key1, value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1, value2);
    EXPECT_EQ(ret, 0);
}
#endif
}  // namespace OHOS
