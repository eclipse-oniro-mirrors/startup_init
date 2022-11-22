/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <thread>
#include <gtest/gtest.h>
#include <cstdint>
#include "param_wrapper.h"
#include "parameter.h"
#include "parameters.h"
#include "sysversion.h"
#include "param_comm.h"
#include "init_param.h"
#include "sysparam_errno.h"

using namespace std;
using namespace testing::ext;
using namespace OHOS;

const int THREAD_NUM = 5;

namespace initModuleTest {
static void GetAllParameterTestFunc(void)
{
    EXPECT_STRNE(GetSecurityPatchTag(), nullptr);
    EXPECT_STRNE(GetOSFullName(), nullptr);
    EXPECT_STRNE(GetVersionId(), nullptr);
    EXPECT_STRNE(GetBuildRootHash(), nullptr);
    EXPECT_STRNE(GetOsReleaseType(), nullptr);
    GetSdkApiVersion();
    EXPECT_STRNE(GetDeviceType(), nullptr);
    EXPECT_STRNE(GetProductModel(), nullptr);
    EXPECT_STRNE(GetManufacture(), nullptr);
    EXPECT_STRNE(GetBrand(), nullptr);
    EXPECT_STRNE(GetMarketName(), nullptr);
    EXPECT_STRNE(GetProductSeries(), nullptr);
    EXPECT_STRNE(GetSoftwareModel(), nullptr);
    EXPECT_STRNE(GetHardwareModel(), nullptr);
    EXPECT_STRNE(GetHardwareProfile(), nullptr);
    EXPECT_STRNE(GetSerial(), nullptr);
    EXPECT_STRNE(GetAbiList(), nullptr);
    EXPECT_STRNE(GetDisplayVersion(), nullptr);
    EXPECT_STRNE(GetIncrementalVersion(), nullptr);
    EXPECT_STRNE(GetBootloaderVersion(), nullptr);
    EXPECT_STRNE(GetBuildType(), nullptr);
    EXPECT_STRNE(GetBuildUser(), nullptr);
    EXPECT_STRNE(GetBuildHost(), nullptr);
    EXPECT_STRNE(GetBuildTime(), nullptr);
    GetFirstApiVersion();
    EXPECT_STRNE(system::GetDeviceType().c_str(), nullptr);
}

static void GetUdidTestFunc(char* udid, int size)
{
    int ret = GetDevUdid(udid, size);
    EXPECT_EQ(ret, 0);
    EXPECT_STRNE(udid, nullptr);
}

static void SetParameterTestFunc(const char *key, const char *value)
{
    EXPECT_EQ(SetParameter(key, value), 0);
    uint32_t handle = FindParameter(key);
    EXPECT_NE(handle, static_cast<unsigned int>(-1));
    uint32_t result = GetParameterCommitId(handle);
    EXPECT_NE(result, static_cast<unsigned int>(-1));
    char nameGet[PARAM_NAME_LEN_MAX] = {0};
    int ret = GetParameterName(handle, nameGet, PARAM_NAME_LEN_MAX);
    EXPECT_EQ(ret, strlen(nameGet));
    EXPECT_STREQ(key, nameGet);
    char valueGet[PARAM_VALUE_LEN_MAX] = {0};
    ret = GetParameterValue(handle, valueGet, PARAM_VALUE_LEN_MAX);
    EXPECT_EQ(ret, strlen(valueGet));
    EXPECT_STREQ(value, valueGet);
    EXPECT_NE(GetSystemCommitId(), 0);
}

static void GetParameterTestReInt(const char *key, const char *def, char *value, uint32_t len)
{
    int ret = GetParameter(key, def, value, len);
    EXPECT_EQ(ret, strlen(value));
    EXPECT_STREQ(value, "v10.1.1");
}

static void GetParameterTestFuncReStr(string key, string def)
{
    string ret = system::GetParameter(key, def);
    EXPECT_STREQ(ret.c_str(), "v10.1.1");
}

static void ParamSetFun(string key, string value)
{
    bool ret = system::SetParameter(key, value);
    EXPECT_TRUE(ret);
    string testValue = system::GetParameter(key, "");
    EXPECT_STREQ(testValue.c_str(), value.c_str());
}

static void TestParameterChange(const char *key, const char *value, void *context)
{
    std::cout<<"TestParameterChange key: "<<key<<"value: "<<value<<endl;
}

static void TestParameterWatchChange(void)
{
    size_t index = 1;
    int ret = WatchParameter("test.param.watcher.test1", TestParameterChange, reinterpret_cast<void *>(index));
    EXPECT_EQ(ret, 0);
    ret = RemoveParameterWatcher("test.param.watcher.test1", TestParameterChange, reinterpret_cast<void *>(index));
    EXPECT_EQ(ret, 0);
}

class SysparaModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(SysparaModuleTest, Syspara_SysVersion_test_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_SysVersion_test_001 start";
    GetMajorVersion();
    GetSeniorVersion();
    GetFeatureVersion();
    GetBuildVersion();
    GTEST_LOG_(INFO) << "Syspara_SysVersion_test_001 end";
}

HWTEST_F(SysparaModuleTest, Syspara_GetParam_test_002, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_GetParam_test_002 start";
    for (int i = 0; i < THREAD_NUM; ++i) {
        std::thread(GetAllParameterTestFunc).join();
    }
    GTEST_LOG_(INFO) << "Syspara_GetParam_test_002 end";
}

HWTEST_F(SysparaModuleTest, Syspara_GetUdid_test_003, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_GetUdid_test_003 start";
    for (int i = 0; i < THREAD_NUM; ++i) {
        char udid[UDID_LEN] = {0};
        std::thread(GetUdidTestFunc, udid, UDID_LEN).join();
    }
    GTEST_LOG_(INFO) << "Syspara_GetUdid_test_003 end";
}

HWTEST_F(SysparaModuleTest, Syspara_SetParameter_test_004, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_SetParameter_test_004 start";
    char key1[] = "test1.param.version";
    char value1[] = "v10.1.1";
    char key2[] = "test2.param.version";
    char value2[] = "v10.2.2";
    char key3[] = "test3.param.version";
    char value3[] = "v10.3.3";
    std::thread(SetParameterTestFunc, key1, value1).join();
    std::thread(SetParameterTestFunc, key2, value2).join();
    std::thread(SetParameterTestFunc, key3, value3).join();
    GTEST_LOG_(INFO) << "Syspara_SetParameter_test_004 end";
}

HWTEST_F(SysparaModuleTest, Syspara_SetParameter_test_005, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_SetParameter_test_005 start";
    // check param name length
    char key1[] = "test.param.name.xxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxx";
    char value[] = "test.value.xxxx";
    int ret = SetParameter(key1, value);
    EXPECT_EQ(ret, EC_INVALID);
    // check param name, Illegal param name
    char key2[] = ".test.param.name.xxxxx";
    ret = SetParameter(key2, value);
    EXPECT_EQ(ret, EC_INVALID);
    char key3[] = "test..param.name.xxxxx";
    ret = SetParameter(key3, value);
    EXPECT_EQ(ret, EC_INVALID);
    char key4[] = "test..param.   .name";
    ret = SetParameter(key4, value);
    EXPECT_EQ(ret, EC_INVALID);
    // check param name, legal param name
    char key5[] = "test.param.name.--__.:::";
    ret = SetParameter(key5, value);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(value, "test.value.xxxx");
    char key6[] = "test.param.name.@@@.---";
    ret = SetParameter(key6, value);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(value, "test.value.xxxx");
    // not const param, check param value, bool 8, int 32, other 96
    char key7[] = "test.param.name.xxxx";
    char value1[] = "test.value.xxxxxxxxx.xxxxxxxxxxxxx.xxxxxxxxxxxx.xxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxx";
    ret = SetParameter(key7, value1);
    EXPECT_EQ(ret, SYSPARAM_INVALID_VALUE);
    char key8[] = "startup.service.ctl.test.int";
    char value2[] = "111111111111111111111111111111111";
    ret = SetParameter(key8, value2);
    EXPECT_EQ(ret, SYSPARAM_INVALID_VALUE);
    GTEST_LOG_(INFO) << "Syspara_SetParameter_test_005 end";
}

HWTEST_F(SysparaModuleTest, Syspara_Getparameter_test_006, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_Getparameter_test_006 start";
    string key = "test.param.set.001";
    string value = "xxx.xxx.xxx";
    bool ret = system::SetParameter(key, value);
    EXPECT_TRUE(ret);
    string testValue = system::GetParameter(key, "");
    EXPECT_STREQ(testValue.c_str(), value.c_str());
    // not read param value,the result is default
    testValue = system::GetParameter("test.param.set.002", "aaa.aaa.aaa");
    EXPECT_STREQ(testValue.c_str(), "aaa.aaa.aaa");
    testValue = system::GetParameter("test.param.set.003", "");
    EXPECT_STREQ(testValue.c_str(), "");
    // correct set value
    string key1 = "test.param.set.bool";
    ret = system::SetParameter(key1, "1");
    EXPECT_TRUE(ret);
    EXPECT_TRUE(system::GetBoolParameter(key1, false));
    ret = system::SetParameter(key1, "y");
    EXPECT_TRUE(ret);
    EXPECT_TRUE(system::GetBoolParameter(key1, false));
    ret = system::SetParameter(key1, "yes");
    EXPECT_TRUE(ret);
    EXPECT_TRUE(system::GetBoolParameter(key1, false));
    ret = system::SetParameter(key1, "on");
    EXPECT_TRUE(ret);
    EXPECT_TRUE(system::GetBoolParameter(key1, false));
    ret = system::SetParameter(key1, "true");
    EXPECT_TRUE(ret);
    EXPECT_TRUE(system::GetBoolParameter(key1, false));
    ret = system::SetParameter(key1, "0");
    EXPECT_TRUE(ret);
    EXPECT_FALSE(system::GetBoolParameter(key1, true));
    ret = system::SetParameter(key1, "off");
    EXPECT_TRUE(ret);
    EXPECT_FALSE(system::GetBoolParameter(key1, true));
    ret = system::SetParameter(key1, "n");
    EXPECT_TRUE(ret);
    EXPECT_FALSE(system::GetBoolParameter(key1, true));
    ret = system::SetParameter(key1, "no");
    EXPECT_TRUE(ret);
    EXPECT_FALSE(system::GetBoolParameter(key1, true));
    ret = system::SetParameter(key1, "false");
    EXPECT_TRUE(ret);
    EXPECT_FALSE(system::GetBoolParameter(key1, true));
    // set value type not bool,the result get form def
    ret = system::SetParameter(key1, "test");
    EXPECT_TRUE(ret);
    EXPECT_TRUE(system::GetBoolParameter(key1, true));
    EXPECT_FALSE(system::GetBoolParameter(key1, false));
    GTEST_LOG_(INFO) << "Syspara_Getparameter_test_006 end";
}

HWTEST_F(SysparaModuleTest, Syspara_SetParameter_test_007, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_SetParameter_test_007 start";
    string key1 = "const.param.test";
    string value1 = "test.param.value.001";

    int ret = 0;
    string vRet = "";
    uint32_t handle1 = FindParameter(key1.c_str());
    cout<<"handle1 is: "<<handle1<<std::endl;
    if (handle1 == static_cast<uint32_t>(-1)) {
        ret = SetParameter(key1.c_str(), value1.c_str());
        EXPECT_EQ(ret, 0);
        vRet = system::GetParameter(key1, "");
        EXPECT_STREQ(vRet.c_str(), value1.c_str());
    }
    string value2 = "test.param.value.002";
    ret = SetParameter(key1.c_str(), value2.c_str());
    EXPECT_EQ(ret, EC_INVALID);

    string key2 = "ro.param.test";
    string value3 = "test.param.value.003";
    uint32_t handle2 = FindParameter(key2.c_str());
    cout<<"handle2 is: "<<handle2<<std::endl;
    if (handle2 == static_cast<uint32_t>(-1)) {
        ret = SetParameter(key2.c_str(), value3.c_str());
        EXPECT_EQ(ret, 0);
        vRet = system::GetParameter(key2, "");
        EXPECT_STREQ(vRet.c_str(), value3.c_str());
    }
    string value4 = "test.param.value.004";
    ret = SetParameter(key2.c_str(), value4.c_str());
    EXPECT_EQ(ret, EC_INVALID);
    GTEST_LOG_(INFO) << "Syspara_SetParameter_test_007 end";
}

HWTEST_F(SysparaModuleTest, Syspara_GetParameterReIntOrStr_test_008, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_GetParameterReIntOrStr_test_008 start";
    string key = "test1.param.version";
    string value = "v10.1.1";
    int ret = SetParameter(key.c_str(), value.c_str());
    EXPECT_EQ(ret, 0);
    char retValue[PARAM_VALUE_LEN_MAX] = {0};
    for (int i = 0; i < THREAD_NUM; ++i) {
        std::thread(GetParameterTestReInt, key.c_str(), "", retValue, PARAM_VALUE_LEN_MAX).join();
    }
    for (int j = 0; j < THREAD_NUM; ++j) {
        std::thread(GetParameterTestFuncReStr, key, "").join();
    }
    GTEST_LOG_(INFO) << "Syspara_GetParameterReIntOrStr_test_008 end";
}

HWTEST_F(SysparaModuleTest, Syspara_WaitParameter_test_009, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_WaitParameter_test_009 start";
    // param already set succeed,waitParamter succeed.
    char key[] = "test1.param.wait";
    char value[] = "aaa.bbb.ccc";
    int ret = SetParameter(key, value);
    EXPECT_EQ(ret, 0);
    ret = WaitParameter(key, value, 5);
    EXPECT_EQ(ret, 0);
    // param not set,waitParamter will wait param set,return succeed.
    char key1[] = "test2.param.wait";
    char value1[] = "aaa.aaa.aaa";
    std::thread(ParamSetFun, key1, value1).join();
    ret = WaitParameter(key1, value1, 5);
    EXPECT_EQ(ret, 0);
    char key2[] = "test3.param.wait";
    std::thread(ParamSetFun, key2, "*****").join();
    ret = WaitParameter(key2, "*****", 5);
    EXPECT_EQ(ret, 0);
    // param not set,waitParamter will timeout,return failed.
    char key3[] = "test4.param.wait";
    ret = WaitParameter(key3, "*****", 5);
    EXPECT_EQ(ret, SYSPARAM_WAIT_TIMEOUT);
    GTEST_LOG_(INFO) << "Syspara_WaitParameter_test_009 end";
}

HWTEST_F(SysparaModuleTest, Syspara_watcherParameter_test_010, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_watcherParameter_test_010 start";
    for (int i = 0; i < THREAD_NUM; ++i) {
        std::thread(TestParameterWatchChange).join();
    }

    std::thread(ParamSetFun, "test.param.watcher.test1", "test.param.value.xxx").join();

    GTEST_LOG_(INFO) << "Syspara_watcherParameter_test_010 end";
}

HWTEST_F(SysparaModuleTest, Syspara_GetParameter_test_011, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "Syspara_GetParameter_test_011 start";

    char key1[] = "test.param.int1";
    char value1[] = "0x111111";
    EXPECT_EQ(SetParameter(key1, value1), 0);
    EXPECT_EQ(GetIntParameter(key1, 0), 1118481); // 0x111111 decimalism result
    EXPECT_EQ(GetUintParameter(key1, 0), 1118481);

    char key2[] = "test.param.int2";
    char value2[] = "-0x111111";
    EXPECT_EQ(SetParameter(key2, value2), 0);
    EXPECT_EQ(GetIntParameter(key2, 0), -1118481);  // 0x111111 decimalism result

    GetUintParameter(key2, 0);

    char key3[] = "test.param.int3";
    char value3[] = "9999999";
    EXPECT_EQ(SetParameter(key3, value3), 0);
    EXPECT_EQ(GetIntParameter(key3, 0), 9999999); // value3 int result
    EXPECT_EQ(GetUintParameter(key3, 0), 9999999); // value3 uint result

    char key4[] = "test.param.int4";
    char value4[] = "-9999999";
    EXPECT_EQ(SetParameter(key4, value4), 0);
    EXPECT_EQ(GetIntParameter(key4, 0), -9999999); // value4 int result
    EXPECT_EQ(GetUintParameter(key4, 0), 0);

    char key5[] = "test.param.int5";
    char value5[] = "-2147483648"; // INT32_MIN
    EXPECT_EQ(SetParameter(key5, value5), 0);
    EXPECT_EQ(GetIntParameter(key5, 0), 0);

    char key6[] = "test.param.int6";
    char value6[] = "2147483647"; // INT32_MAX
    EXPECT_EQ(SetParameter(key6, value6), 0);
    EXPECT_EQ(GetIntParameter(key6, 0), 0);

    char key7[] = "test.param.uint7";
    char value7[] = "4294967295"; // UINT32_MAX
    EXPECT_EQ(SetParameter(key7, value7), 0);
    EXPECT_EQ(GetUintParameter(key7, 0), 0);

    GTEST_LOG_(INFO) << "Syspara_GetParameter_test_011 end";
}
}