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
    printf("Device type =%s\n", GetDeviceType());
    printf("Manufacture =%s\n", GetManufacture());
    printf("GetBrand =%s\n", GetBrand());
    printf("MarketName =%s\n", GetMarketName());
    printf("ProductSeries =%s\n", GetProductSeries());
    printf("ProductModel =%s\n", GetProductModel());
    printf("SoftwareModel =%s\n", GetSoftwareModel());
    printf("HardwareModel =%s\n", GetHardwareModel());
    printf("Software profile =%s\n", GetHardwareProfile());
    printf("Serial =%s\n", GetSerial());
    printf("OS full name =%s\n", GetOSFullName());
    printf("OS Release type =%s\n", GetOsReleaseType());
    printf("Display version =%s\n", GetDisplayVersion());
    printf("bootloader version =%s\n", GetBootloaderVersion());
    printf("secure patch level =%s\n", GetSecurityPatchTag());
}

HWTEST_F(SysparaUnitTest, parameterTest001_1, TestSize.Level0)
{
    printf("secure patch level =%s\n", GetSecurityPatchTag());
    printf("abi list =%s\n", GetAbiList());
    printf("first api version =%d\n", GetFirstApiVersion());
    printf("SDK api version =%d\n", GetSdkApiVersion());
    printf("Incremental version = %s\n", GetIncrementalVersion());
    printf("formal id =%s\n", GetVersionId());
    printf("build type =%s\n", GetBuildType());
    printf("build user =%s\n", GetBuildUser());
    printf("Build host = %s\n", GetBuildHost());
    printf("build time =%s\n", GetBuildTime());
    printf("build root later..., %s\n", GetBuildRootHash());
}

HWTEST_F(SysparaUnitTest, parameterTest001_2, TestSize.Level0)
{
    EXPECT_STRNE(GetDeviceType(), nullptr);
    EXPECT_STRNE(GetManufacture(), nullptr);
    EXPECT_STRNE(GetBrand(), nullptr);
    EXPECT_STRNE(GetMarketName(), nullptr);
    EXPECT_STRNE(GetProductSeries(), nullptr);
    EXPECT_STRNE(GetProductModel(), nullptr);
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
}

HWTEST_F(SysparaUnitTest, parameterTest002, TestSize.Level0)
{
    char key1[] = "test.ro.sys.version";
    char value1[] = "set read only key";
    int ret = SetParameter(key1, value1);
    EXPECT_EQ(ret, EC_SUCCESS);
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
    GetSeniorVersion();
    GetFeatureVersion();
    GetBuildVersion();
}

#ifndef OHOS_LITE
// for test param_wrapper.cpp
HWTEST_F(SysparaUnitTest, parameterTest0014, TestSize.Level0)
{
    const std::string key1 = "test.int.get";
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
#endif
}  // namespace OHOS
