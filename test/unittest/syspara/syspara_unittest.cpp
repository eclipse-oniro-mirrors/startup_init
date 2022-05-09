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
#include "parameter.h"
#include "sysparam_errno.h"

using namespace testing::ext;

namespace OHOS {
class SysparaUnitTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
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
    printf("build tyep =%s\n", GetBuildType());
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
    EXPECT_EQ(ret, EC_FAILURE);
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
    EXPECT_EQ(ret, EC_FAILURE);
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
    EXPECT_EQ(ret, EC_FAILURE);
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
    EXPECT_EQ(ret, 105);
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
    EXPECT_EQ(ret, -1);
    ret = GetParameterName(handle, nameGet1, 32);
    EXPECT_EQ(ret, -1);
}
}  // namespace OHOS