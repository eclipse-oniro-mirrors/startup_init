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

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#ifndef OHOS_LITE
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#endif

#include "init_param.h"
#include "init_utils.h"
#include "parameter.h"
#include "param_comm.h"
#include "param_stub.h"
#ifndef OHOS_LITE
#include "func_wrapper.h"
#include "param_wrapper.h"
#include "parameters.h"
#endif
#include "securec.h"
#include "sysversion.h"
#include "sysparam_errno.h"

using namespace testing::ext;

#ifndef OHOS_LITE
namespace {
struct ParamReadOverride {
    const char *key;
    const char *value;
    int readError;
    const ParamReadOverride *previous;
};

thread_local const ParamReadOverride *g_paramReadOverride = nullptr;

class ScopedParamRead {
public:
    ScopedParamRead(const char *key, const char *value, int readError = 0)
        : override_{key, value, readError, g_paramReadOverride}
    {
        g_paramReadOverride = &override_;
    }

    ~ScopedParamRead()
    {
        g_paramReadOverride = override_.previous;
    }

    ScopedParamRead(const ScopedParamRead &) = delete;
    ScopedParamRead &operator=(const ScopedParamRead &) = delete;

private:
    ParamReadOverride override_;
};
}  // namespace

extern "C" int SysparaRealSystemReadParam(const char *name, char *value, uint32_t *len);
extern "C" int SystemReadParam(const char *name, char *value, uint32_t *len)
{
    const auto *override = g_paramReadOverride;
    if (name != nullptr && len != nullptr) {
        while (override != nullptr && strcmp(name, override->key) != 0) {
            override = override->previous;
        }
    } else {
        override = nullptr;
    }
    if (override == nullptr) {
        return SysparaRealSystemReadParam(name, value, len);
    }
    if (override->value == nullptr) {
        return PARAM_CODE_NOT_FOUND;
    }
    if (value == nullptr) {
        *len = strlen(override->value) + 1;
        return 0;
    }
    if (override->readError != 0) {
        return override->readError;
    }
    if (strcpy_s(value, *len, override->value) != EOK) {
        return PARAM_CODE_INVALID_VALUE;
    }
    *len = strlen(override->value);
    return 0;
}

extern "C" char *NullProcessorForApiVersion(const char *key, char *value)
{
    if (key != nullptr &&
        (strcmp(key, "const.ohos.apiversion") == 0 ||
         strcmp(key, "const.product.os.dist.apiversion") == 0)) {
        free(value);
        return nullptr;
    }
    return value;
}
#endif

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

#ifndef OHOS_LITE
// const 参数一旦写入即变为只读，且无法删除；测试二进制多次执行时上一轮残留的
// const 参数仍存在于参数工作区中，导致 SystemWriteParam 返回 PARAM_CODE_READ_ONLY。
// 这里从计数器后缀开始探测，找到一个尚不存在的 const 键，保证每轮执行使用全新的键。
static std::string MakeUniqueConstKey(const char *base)
{
    static unsigned long counter = 0;
    std::string key = std::string(base) + "_" + std::to_string(counter);
    while (FindParameter(key.c_str()) != static_cast<uint32_t>(-1)) {
        counter++;
        key = std::string(base) + "_" + std::to_string(counter);
    }
    counter++;
    return key;
}
#endif

#ifndef OHOS_LITE
HWTEST_F(SysparaUnitTest, parameterTest0000DistApiVersionNullBranch, TestSize.Level0)
{
    PropertyValueProcessor prev = SetPropertyGetProcessor(NullProcessorForApiVersion);
    int ret = GetDistributionOSApiVersion();
    EXPECT_EQ(ret, 0);
    // 恢复原处理器，避免影响后续用例
    SetPropertyGetProcessor(prev);
}

HWTEST_F(SysparaUnitTest, parameterTest0000DistApiVersionNullBranch2, TestSize.Level0)
{
    int ret = GetDistributionOSApiVersion();
    EXPECT_GT(ret, 0);
}

HWTEST_F(SysparaUnitTest, parameterTestDistInfoNotNullPtr001, TestSize.Level0)
{
    EXPECT_STRNE(GetDistributionOSName(), nullptr);
    EXPECT_STRNE(GetDistributionOSVersion(), nullptr);
    EXPECT_STRNE(GetDistributionOSApiName(), nullptr);
    EXPECT_STRNE(GetDistributionOSReleaseType(), nullptr);
}
#endif

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
    EXPECT_STRNE(GetChipType(), nullptr);
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
    EXPECT_GT(GetBootCount(), -1);
    EXPECT_STRNE(GetDeviceColor(), nullptr);
}

HWTEST_F(SysparaUnitTest, parameterTest0024, TestSize.Level0)
{
    const char *deviceColor = GetDeviceColor();
    EXPECT_STRNE(deviceColor, nullptr);
    if (strcmp(deviceColor, "") == 0) {
        printf("deviceColor is empty string (parameter not set)\n");
    } else {
        printf("deviceColor = %s\n", deviceColor);
    }
}

// GetKernelVersionInner 通过 uname 获取内核版本，缓存于静态缓冲区；
// uname 失败或 strcpy_s 失败时返回 EMPTY_STR，但永不返回 nullptr。
HWTEST_F(SysparaUnitTest, parameterTest0024_2, TestSize.Level0)
{
    const char *kernelVersion = GetKernelVersionInner();
    EXPECT_STRNE(kernelVersion, nullptr);
    if (kernelVersion == nullptr || strcmp(kernelVersion, "") == 0) {
        printf("kernelVersion is empty string (uname failed)\n");
    } else {
        printf("kernelVersion = %s\n", kernelVersion);
    }
    const char *kernelVersion2 = GetKernelVersionInner();
    EXPECT_EQ(kernelVersion, kernelVersion2);
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
    std::string key1 = MakeUniqueConstKey("const.test.for_update_test");
    std::string key2 = MakeUniqueConstKey("persist.test.for_update_test");
    char value1[] = "initSet";
    char value2[] = "initUpdate";

    int ret = SystemUpdateConstParam(key1.c_str(), value2);
    EXPECT_EQ(ret, PARAM_CODE_INVALID_NAME);
    ret = SystemWriteParam(key1.c_str(), value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1.c_str(), value2);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key2.c_str(), value2);
    EXPECT_EQ(ret, PARAM_CODE_INVALID_NAME);
}

HWTEST_F(SysparaUnitTest, parameterTest0020, TestSize.Level0)
{
    std::string key1 = MakeUniqueConstKey("const.test.for_update_test1");
    char value1[] = "initSet"; // len < 96
    char value2[] = "initUpdate_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96

    int ret = SystemWriteParam(key1.c_str(), value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1.c_str(), value2);
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);
}

HWTEST_F(SysparaUnitTest, parameterTest0021, TestSize.Level0)
{
    std::string key1 = MakeUniqueConstKey("const.test.for_update_test2");
    char value1[] = "initSet_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96
    char value2[] = "initUpdate_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96

    int ret = SystemWriteParam(key1.c_str(), value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1.c_str(), value2);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0022, TestSize.Level0)
{
    std::string key1 = MakeUniqueConstKey("const.test.for_update_test3");
    char value1[] = "initSet_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghi"; // len = 96
    char value2[] = "initUpdate_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdef"; // len = 96

    int ret = SystemWriteParam(key1.c_str(), value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1.c_str(), value2);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0023, TestSize.Level0)
{
    std::string key1 = MakeUniqueConstKey("const.test.for_update_test4");
    char value1[] = "initSet_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" \
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"; // len > 96
    char value2[] = "initUpdate_abcdefghijkl"; // len < 96

    int ret = SystemWriteParam(key1.c_str(), value1);
    EXPECT_EQ(ret, 0);
    ret = SystemUpdateConstParam(key1.c_str(), value2);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(SysparaUnitTest, parameterTest0025, TestSize.Level0)
{
    const char *deviceType = GetDeviceType();
    printf("Device type =%s\n", deviceType);
    EXPECT_STRNE(deviceType, nullptr);
}

HWTEST_F(SysparaUnitTest, parameterTest0026, TestSize.Level0)
{
    char key[] = "const.deviceManager.getdevicetype_extend_enable";
    int ret = 0;

    if (FindParameter(key) == static_cast<uint32_t>(-1)) {
        ret = SystemWriteParam(key, "true");
    } else {
        ret = SystemUpdateConstParam(key, "true");
    }
    EXPECT_EQ(ret, 0);

    const char *deviceType = GetDeviceType();
    EXPECT_STRNE(deviceType, nullptr);

    ret = SystemUpdateConstParam(key, "false");
    EXPECT_EQ(ret, 0);
}

//selinux forbid persist.ark.deferfreeze
HWTEST_F(SysparaUnitTest, parameterTest0027, TestSize.Level0)
{
    char key2[] = "persist.ark.deferfreeze";
    char value2[64] = {0};
    char defValue2[] = "value of key > 32 ...";
    int ret = GetParameter(key2, defValue2, value2, 64);
    EXPECT_NE(ret, -1);
}

//dac forbid ohos.boot.sn
HWTEST_F(SysparaUnitTest, parameterTest0028, TestSize.Level0)
{
    char key2[] = "ohos.boot.sn";
    char value2[64] = {0};
    char defValue2[] = "value of key > 32 ...";
    int ret = GetParameter(key2, defValue2, value2, 64);
    EXPECT_NE(ret, -1);
}

HWTEST_F(SysparaUnitTest, parameterTest0029, TestSize.Level0)
{
    char oldValue[PARAM_VALUE_LEN_MAX] = {0};
    int oldRet = GetParameter("persist.dupdate_engine.lastHotaInstallTime", "", oldValue, sizeof(oldValue));
    if (oldRet <= 0) {
        EXPECT_STREQ(GetInstallTime(), "");
        SetParameter("persist.dupdate_engine.lastHotaInstallTime", "1234567890");
        EXPECT_STREQ(GetInstallTime(), "1234567890");
    } else {
        const char *result = GetInstallTime();
        EXPECT_STRNE(result, nullptr);
        printf("installTime = %s\n", result);
    }
    SetParameter("persist.dupdate_engine.lastHotaInstallTime", oldValue);
    SaveParameters();
}

HWTEST_F(SysparaUnitTest, parameterTest0043, TestSize.Level0)
{
    char value[64] = {0};
    int ret = WaitParameter(nullptr, "value", 1);
    EXPECT_EQ(ret, EC_INVALID);
    ret = WaitParameter("key", nullptr, 1);
    EXPECT_EQ(ret, EC_INVALID);
    ret = FindParameter(nullptr);
    EXPECT_EQ(ret, static_cast<uint32_t>(-1));
    ret = SetParameter(nullptr, "value");
    EXPECT_EQ(ret, EC_INVALID);
    ret = SetParameter("key", nullptr);
    EXPECT_EQ(ret, EC_INVALID);
    ret = GetParameter(nullptr, "def", value, 64);
    EXPECT_EQ(ret, EC_INVALID);
    ret = GetParameter("key", "def", nullptr, 64);
    EXPECT_EQ(ret, EC_INVALID);
}

HWTEST_F(SysparaUnitTest, parameterTest0030, TestSize.Level0)
{
    char name[32] = {0};
    int ret = GetParameterName(static_cast<uint32_t>(-1), name, 32);
    EXPECT_EQ(ret, SYSPARAM_NOT_FOUND);
    ret = GetParameterName(static_cast<uint32_t>(-1), nullptr, 32);
    EXPECT_EQ(ret, EC_INVALID);
    ret = GetParameterValue(static_cast<uint32_t>(-1), nullptr, 32);
    EXPECT_EQ(ret, EC_INVALID);
}

#ifndef OHOS_LITE
HWTEST_F(SysparaUnitTest, parameterTest0031, TestSize.Level0)
{
    constexpr const char *key = "const.ohos.apiversion";
    ScopedParamRead read(key, "12");
    const char *holder = nullptr;
    PropertyValueProcessor previous = SetPropertyGetProcessor(NullProcessorForApiVersion);
    EXPECT_EQ(GetProperty(key, &holder), nullptr);
    EXPECT_EQ(GetPropertyAtomic(key, &holder), nullptr);
    SetPropertyGetProcessor(previous);
}
#endif

HWTEST_F(SysparaUnitTest, parameterTest0032, TestSize.Level0)
{
    const char *osFullName = GetOSFullName();
    EXPECT_STRNE(osFullName, nullptr);
    const char *osFullName2 = GetOSFullName();
    EXPECT_EQ(osFullName, osFullName2);
    const char *versionId = GetVersionId();
    EXPECT_STRNE(versionId, nullptr);
    const char *versionId2 = GetVersionId();
    EXPECT_EQ(versionId, versionId2);
}

HWTEST_F(SysparaUnitTest, parameterTest0036, TestSize.Level0)
{
    constexpr const char *key = "persist.startup.bootcount";
    {
        ScopedParamRead read(key, nullptr);
        EXPECT_EQ(GetBootCount(), -1);
    }
    {
        ScopedParamRead read(key, "6");
        EXPECT_EQ(GetBootCount(), 6);
    }
}

#ifndef OHOS_LITE
HWTEST_F(SysparaUnitTest, parameterTest0037, TestSize.Level0)
{
    bool bret = OHOS::system::SetParameter("testKey", "testValue");
    EXPECT_TRUE(bret);
    std::string value = OHOS::system::GetParameter("not.exist.key", "defaultVal");
    EXPECT_EQ(value, std::string("defaultVal"));
    bool bval = OHOS::system::GetBoolParameter("not.exist.key", true);
    EXPECT_TRUE(bval);
    bval = OHOS::system::GetBoolParameter("not.exist.key", false);
    EXPECT_FALSE(bval);
}

HWTEST_F(SysparaUnitTest, parameterTest0040, TestSize.Level0)
{
    constexpr const char *key = "test.syspara.bool";
    for (const char *value : {"1", "y", "yes", "on", "true"}) {
        ScopedParamRead read(key, value);
        EXPECT_TRUE(OHOS::system::GetBoolParameter(key, false)) << value;
    }
    for (const char *value : {"0", "off", "n", "no", "false"}) {
        ScopedParamRead read(key, value);
        EXPECT_FALSE(OHOS::system::GetBoolParameter(key, true)) << value;
    }
    ScopedParamRead read(key, "unknown");
    EXPECT_TRUE(OHOS::system::GetBoolParameter(key, true));
    EXPECT_FALSE(OHOS::system::GetBoolParameter(key, false));
}
#endif

HWTEST_F(SysparaUnitTest, parameterTest0041, TestSize.Level0)
{
    EXPECT_EQ(GetSystemError(0), 0);
    EXPECT_EQ(GetSystemError(PARAM_CODE_INVALID_VALUE), SYSPARAM_INVALID_VALUE);
    EXPECT_EQ(GetSystemError(PARAM_CODE_NOT_FOUND), SYSPARAM_NOT_FOUND);
    EXPECT_EQ(GetSystemError(DAC_RESULT_FORBIDED), SYSPARAM_PERMISSION_DENIED);
    EXPECT_EQ(GetSystemError(PARAM_CODE_TIMEOUT), SYSPARAM_WAIT_TIMEOUT);
    EXPECT_EQ(GetSystemError(PARAM_CODE_REACHED_MAX), SYSPARAM_SYSTEM_ERROR);
    EXPECT_EQ(GetSystemError(99999), SYSPARAM_SYSTEM_ERROR);
    EXPECT_EQ(IsValidParamValue(nullptr, 32), 0);
    char longVal[40] = {0};
    memset_s(longVal, sizeof(longVal), 'a', sizeof(longVal) - 1);
    EXPECT_EQ(IsValidParamValue(longVal, 32), 0);
    EXPECT_EQ(IsValidParamValue("ok", 4), 1);
}

HWTEST_F(SysparaUnitTest, parameterTest0042, TestSize.Level0)
{
    constexpr const char *key = "test.syspara.read";
    constexpr const char *fallback = "default";
    char value[32] = {};
    EXPECT_EQ(GetParameter_(nullptr, fallback, value, sizeof(value)), EC_INVALID);
    EXPECT_EQ(GetParameter_(key, fallback, nullptr, sizeof(value)), EC_INVALID);
    EXPECT_EQ(GetParameter_(key, fallback, value, UINT32_MAX), EC_INVALID);
    {
        ScopedParamRead read(key, nullptr);
        EXPECT_EQ(GetParameter_(key, nullptr, value, sizeof(value)), SYSPARAM_NOT_FOUND);
        EXPECT_EQ(GetParameter_(key, fallback, value, 1), EC_INVALID);
        EXPECT_EQ(GetParameter_(key, fallback, value, sizeof(value)), 0);
        EXPECT_STREQ(value, fallback);
    }
    {
        ScopedParamRead read(key, "read-value");
        EXPECT_EQ(GetParameter_(key, fallback, value, 1), EC_INVALID);
        EXPECT_EQ(GetParameter_(key, nullptr, value, sizeof(value)), 0);
        EXPECT_STREQ(value, "read-value");
    }
    {
        ScopedParamRead read(key, "read-value", PARAM_CODE_INVALID_VALUE);
        EXPECT_EQ(GetParameter_(key, fallback, value, sizeof(value)), SYSPARAM_INVALID_VALUE);
    }
}

HWTEST_F(SysparaUnitTest, ParameterReadBufferBoundaries, TestSize.Level0)
{
    constexpr const char *key = "test.syspara.buffer";
    constexpr char expected[] = "value";
    for (bool exists : {true, false}) {
        SCOPED_TRACE(exists);
        ScopedParamRead read(key, exists ? expected : nullptr);
        char value[sizeof(expected)] = {};
        EXPECT_EQ(GetParameter_(key, expected, value, sizeof(value)), 0);
        EXPECT_STREQ(value, expected);
        EXPECT_EQ(GetParameter_(key, expected, value, sizeof(value) - 1), EC_INVALID);
        EXPECT_STREQ(value, expected);
        EXPECT_EQ(GetParameter_(key, expected, value, 0), EC_INVALID);
        EXPECT_STREQ(value, expected);
    }
}

namespace {
constexpr const char *NUMERIC_KEY = "test.syspara.numeric";
}  // namespace

HWTEST_F(SysparaUnitTest, NumericWrapperConversionBranches, TestSize.Level0)
{
    constexpr int fallback = 9;
    constexpr int min = -10;
    constexpr int max = 10;
    struct SignedCase {
        const char *value;
        int expected;
    };
    const SignedCase signedCases[] = {
        {nullptr, fallback}, {"bad", fallback}, {"-11", fallback}, {"11", fallback}, {"-10", min},
    };
    for (const auto &item : signedCases) {
        SCOPED_TRACE(item.value == nullptr ? "<missing>" : item.value);
        ScopedParamRead read(NUMERIC_KEY, item.value);
        EXPECT_EQ(OHOS::system::GetIntParameter<int8_t>(NUMERIC_KEY, fallback, min, max), item.expected);
        EXPECT_EQ(OHOS::system::GetIntParameter<int16_t>(NUMERIC_KEY, fallback, min, max), item.expected);
        EXPECT_EQ(OHOS::system::GetIntParameter<int32_t>(NUMERIC_KEY, fallback, min, max), item.expected);
        EXPECT_EQ(OHOS::system::GetIntParameter<int64_t>(NUMERIC_KEY, fallback, min, max), item.expected);
    }

    struct UnsignedCase {
        const char *value;
        unsigned int expected;
    };
    const UnsignedCase unsignedCases[] = {
        {nullptr, fallback}, {"bad", fallback}, {"11", fallback}, {"10", max},
    };
    for (const auto &item : unsignedCases) {
        SCOPED_TRACE(item.value == nullptr ? "<missing>" : item.value);
        ScopedParamRead read(NUMERIC_KEY, item.value);
        EXPECT_EQ(OHOS::system::GetUintParameter<uint8_t>(NUMERIC_KEY, fallback, max), item.expected);
        EXPECT_EQ(OHOS::system::GetUintParameter<uint16_t>(NUMERIC_KEY, fallback, max), item.expected);
        EXPECT_EQ(OHOS::system::GetUintParameter<uint32_t>(NUMERIC_KEY, fallback, max), item.expected);
        EXPECT_EQ(OHOS::system::GetUintParameter<uint64_t>(NUMERIC_KEY, fallback, max), item.expected);
    }
}

HWTEST_F(SysparaUnitTest, NumericParameterConversionFailures, TestSize.Level0)
{
    constexpr int fallback = 9;
    for (const char *value : {"bad", "-2147483648", "2147483647"}) {
        ScopedParamRead read(NUMERIC_KEY, value);
        EXPECT_EQ(::GetIntParameter(NUMERIC_KEY, fallback), fallback) << value;
    }
    ScopedParamRead read(NUMERIC_KEY, "4294967295");
    EXPECT_EQ(::GetUintParameter(NUMERIC_KEY, fallback), static_cast<uint32_t>(fallback));
}

HWTEST_F(SysparaUnitTest, StringWrapperFallbackBranches, TestSize.Level0)
{
    constexpr const char *key = "test.syspara.string";
    constexpr size_t oversizedLength = 256;
    const std::string oversized(oversizedLength, 'a');
    std::string value;
    {
        ScopedParamRead read(key, "test-value");
        EXPECT_EQ(OHOS::system::GetStringParameter(key, value, oversized), 0);
        EXPECT_EQ(value, "test-value");
    }
    {
        ScopedParamRead read(key, "test-value", PARAM_CODE_INVALID_VALUE);
        EXPECT_EQ(OHOS::system::GetParameter(key, oversized), "");
        EXPECT_EQ(OHOS::system::GetStringParameter(key, value, oversized), EC_FAILURE);
        EXPECT_EQ(OHOS::system::GetParameter(key, "fallback"), "fallback");
        EXPECT_EQ(OHOS::system::GetStringParameter(key, value, "fallback"), 0);
        EXPECT_EQ(value, "fallback");
    }
}

HWTEST_F(SysparaUnitTest, PropertyHolderBranches, TestSize.Level0)
{
    constexpr const char *key = "test.syspara.property";
    EXPECT_EQ(GetProperty(key, nullptr), nullptr);
    EXPECT_EQ(GetPropertyAtomic(key, nullptr), nullptr);
    const char *holder = nullptr;
    {
        ScopedParamRead read(key, nullptr);
        EXPECT_EQ(GetProperty(key, &holder), nullptr);
        EXPECT_EQ(GetPropertyAtomic(key, &holder), nullptr);
    }
    {
        ScopedParamRead read(key, "test-value", PARAM_CODE_INVALID_VALUE);
        EXPECT_EQ(GetProperty(key, &holder), nullptr);
        EXPECT_EQ(GetPropertyAtomic(key, &holder), nullptr);
    }
    {
        ScopedParamRead read(key, "test-value");
        EXPECT_STREQ(GetPropertyAtomic(key, &holder), "test-value");
        EXPECT_EQ(GetPropertyAtomic(key, &holder), holder);
        free(const_cast<char *>(holder));
    }
}

HWTEST_F(SysparaUnitTest, PropertyAllocationFailure, TestSize.Level0)
{
    constexpr const char *key = "test.syspara.alloc";
    ScopedParamRead read(key, "test-value");
    const char *holder = nullptr;
    UpdateCallocFunc([](size_t, size_t) -> void * { return nullptr; });
    const char *plain = GetProperty(key, &holder);
    const char *atomic = GetPropertyAtomic(key, &holder);
    UpdateCallocFunc(nullptr);
    EXPECT_EQ(plain, nullptr);
    EXPECT_EQ(atomic, nullptr);
    EXPECT_EQ(holder, nullptr);
}

namespace {
constexpr const char *COMPETING_PROPERTY_KEY = "test.syspara.race";
thread_local const char **g_competingHolder = nullptr;

char *PopulateCompetingProperty(const char *key, char *value)
{
    if (g_competingHolder != nullptr && strcmp(key, COMPETING_PROPERTY_KEY) == 0) {
        const auto processor = SetPropertyGetProcessor(nullptr);
        ScopedParamRead winner(key, "winner");
        const char *cached = GetPropertyAtomic(key, g_competingHolder);
        SetPropertyGetProcessor(processor);
        EXPECT_STREQ(cached, "winner");
    }
    return value;
}
}  // namespace

HWTEST_F(SysparaUnitTest, AtomicPropertyUsesWinningValue, TestSize.Level0)
{
    const char *key = COMPETING_PROPERTY_KEY;
    ScopedParamRead read(key, "loser");
    const char *holder = nullptr;
    g_competingHolder = &holder;
    const auto previous = SetPropertyGetProcessor(PopulateCompetingProperty);
    const char *result = GetPropertyAtomic(key, &holder);
    SetPropertyGetProcessor(previous);
    g_competingHolder = nullptr;
    EXPECT_STREQ(result, "winner");
    EXPECT_EQ(result, holder);
    free(const_cast<char *>(holder));
}

namespace {
// Restart the selected test so each case begins with empty product caches.
template<typename Case, size_t count, typename Check>
testing::AssertionResult CheckWithColdCaches(const char *testName, const Case (&cases)[count], Check check)
{
    const char *selected = std::getenv(SYSPARA_TEST_CHILD_CASE);
    if (selected != nullptr) {
        for (size_t index = 0; index < count; ++index) {
            if (std::to_string(index) == selected) {
                SCOPED_TRACE(index);
                check(cases[index]);
                std::exit(testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
            }
        }
        std::exit(EXIT_FAILURE);
    }

    const std::string filter = std::string("--gtest_filter=SysparaUnitTest.") + testName;
    for (size_t index = 0; index < count; ++index) {
        std::string childCase = std::string(SYSPARA_TEST_CHILD_CASE) + "=" + std::to_string(index);
        std::vector<char *> childEnv;
        for (char **entry = environ; *entry != nullptr; ++entry) {
            childEnv.push_back(*entry);
        }
        childEnv.push_back(childCase.data());
        childEnv.push_back(nullptr);
        pid_t pid = fork();
        if (pid == -1) {
            return testing::AssertionFailure() << "fork failed for case " << index << ", errno: " << errno;
        }
        if (pid == 0) {
            execle("/proc/self/exe", "init_unittest", filter.c_str(), "--gtest_brief=1",
                static_cast<char *>(nullptr), childEnv.data());
            _exit(EXIT_FAILURE);
        }

        int status = 0;
        pid_t waited;
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited == -1 && errno == EINTR);
        if (waited != pid || !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            return testing::AssertionFailure() << "Cold-cache case " << index << " failed, waitpid: "
                << waited << ", status: " << status;
        }
    }
    return testing::AssertionSuccess();
}
}  // namespace
HWTEST_F(SysparaUnitTest, VersionParsingAndCache, TestSize.Level0)
{
    struct VersionCase {
        const char *name;
        int major;
    };
    const VersionCase cases[] = {{nullptr, 2}, {"without_separator", 2},
        {"OpenHarmony-8", 8}, {"OpenHarmony-9.1.3.5", 9}};
    const auto result = CheckWithColdCaches("VersionParsingAndCache", cases, [](const auto &item) {
        ScopedParamRead fullName("const.ohos.fullname", item.name);
        EXPECT_EQ(GetMajorVersion(), item.major);
        EXPECT_EQ(GetMajorVersion(), item.major);
    });
    EXPECT_TRUE(result);
}

HWTEST_F(SysparaUnitTest, DeviceTypeMappingAndFallback, TestSize.Level0)
{
    struct DeviceCase {
        const char *primary;
        const char *fallback;
        const char *expected;
    };
    const DeviceCase cases[] = {{nullptr, nullptr, ""}, {nullptr, "watch", "wearable"},
        {"fitnessWatch", nullptr, "liteWearable"}, {"tablet", nullptr, "tablet"}};
    const auto result = CheckWithColdCaches("DeviceTypeMappingAndFallback", cases, [](const auto &item) {
        ScopedParamRead primary("const.product.devicetype", item.primary);
        ScopedParamRead fallback("const.build.characteristics", item.fallback);
        EXPECT_EQ(OHOS::system::GetDeviceType(), item.expected);
    });
    EXPECT_TRUE(result);
}

HWTEST_F(SysparaUnitTest, PerformanceClassBoundaries, TestSize.Level0)
{
    struct PerformanceCase {
        const char *value;
        int expected;
    };
    const PerformanceCase cases[] = {{nullptr, 0}, {"-1", 0}, {"99", 0}, {"1", 1}};
    const auto result = CheckWithColdCaches("PerformanceClassBoundaries", cases, [](const auto &item) {
        ScopedParamRead value("const.sys.performance_class", item.value);
        EXPECT_EQ(GetPerformanceClass(), item.expected);
    });
    EXPECT_TRUE(result);
}

HWTEST_F(SysparaUnitTest, OperatingSystemNameBranches, TestSize.Level0)
{
    struct NameCase {
        const char *name;
        const char *release;
        const char *expected;
        bool failAllocation = false;
    };
    constexpr size_t propertyLength = OS_FULL_NAME_LEN / 2;
    const std::string longProperty(propertyLength, 'a');
    const NameCase cases[] = {{nullptr, "Release", ""}, {"TestOS", nullptr, ""},
        {"TestOS", "Release", "TestOS"}, {"TestOS", "Beta", "TestOS(Beta)"},
        {longProperty.c_str(), longProperty.c_str(), ""}, {"TestOS", "Beta", "", true}};
    const auto result = CheckWithColdCaches("OperatingSystemNameBranches", cases, [](const auto &item) {
        ScopedParamRead fullName("const.ohos.fullname", item.name);
        ScopedParamRead releaseType("const.ohos.releasetype", item.release);
        // Cache the inputs before injecting failure into the output allocation.
        if (item.failAllocation) {
            ASSERT_NE(GetFullName_(), nullptr);
            ASSERT_NE(GetOsReleaseType(), nullptr);
            UpdateCallocFunc([](size_t, size_t) -> void * { return nullptr; });
        }
        const char *actual = GetOSFullName();
        if (item.failAllocation) {
            UpdateCallocFunc(nullptr);
        }
        EXPECT_STREQ(actual, item.expected);
    });
    EXPECT_TRUE(result);
}
HWTEST_F(SysparaUnitTest, MissingAndOversizedVersionProperties, TestSize.Level0)
{
    const bool cases[] = {false, true};
    const auto result = CheckWithColdCaches("MissingAndOversizedVersionProperties", cases, [](bool oversized) {
        if (oversized) {
            constexpr size_t propertyLength = VERSION_ID_MAX_LEN / 4;
            const std::string longProperty(propertyLength, 'a');
            ScopedParamRead device("const.product.devicetype", longProperty.c_str());
            ScopedParamRead manufacture("const.product.manufacturer", longProperty.c_str());
            ScopedParamRead brand("const.product.brand", longProperty.c_str());
            ScopedParamRead series("const.build.product", longProperty.c_str());
            EXPECT_STREQ(GetDeviceType(), longProperty.c_str());
            EXPECT_STREQ(GetVersionId(), "");
            return;
        }
        ScopedParamRead device("const.product.devicetype", nullptr);
        ScopedParamRead characteristics("const.build.characteristics", nullptr);
        ScopedParamRead firstApi("const.product.firstapiversion", nullptr);
        ScopedParamRead api("const.ohos.apiversion", nullptr);
        ScopedParamRead minor("const.ohos.apiminorversion", nullptr);
        ScopedParamRead patch("const.ohos.apipatchversion", nullptr);
        EXPECT_STREQ(GetVersionId(), "");
        EXPECT_EQ(GetFirstApiVersion(), 0);
        EXPECT_EQ(GetSdkApiVersion(), 0);
        EXPECT_EQ(GetSdkMinorApiVersion(), -1);
        EXPECT_EQ(GetSdkPatchApiVersion(), -1);
    });
    EXPECT_TRUE(result);
}

HWTEST_F(SysparaUnitTest, InstallTimeMissingAndPresent, TestSize.Level0)
{
    const char *cases[] = {nullptr, "1234567890"};
    const auto result = CheckWithColdCaches("InstallTimeMissingAndPresent", cases, [](const char *value) {
        ScopedParamRead installTime("persist.dupdate_engine.lastHotaInstallTime", value);
        EXPECT_STREQ(GetInstallTime(), value == nullptr ? "" : value);
    });
    EXPECT_TRUE(result);
}

#endif
}  // namespace OHOS
