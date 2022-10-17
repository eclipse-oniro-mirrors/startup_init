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
#include <gtest/gtest.h>

#include "init_param.h"
#include "init_utils.h"
#include "param_stub.h"

using namespace std;
using namespace testing::ext;

static void ClientCheckParamValue(const char *name, const char *expectValue)
{
    char tmp[PARAM_BUFFER_SIZE] = {0};
    u_int32_t len = sizeof(tmp);
    int ret = SystemGetParameter(name, tmp, &len);
    PARAM_LOGI("ClientCheckParamValue name %s value: \'%s\' expectValue:\'%s\' ", name, tmp, expectValue);
    if (ret == 0 && len > 0) {
        EXPECT_NE((int)strlen(tmp), 0);
        if (expectValue != nullptr) {
            EXPECT_EQ(strcmp(tmp, expectValue), 0);
        }
    } else {
        EXPECT_NE(0, 0);
    }
}

// 多线程测试
static void *TestSendParamSetMsg(void *args)
{
    if (args == nullptr) {
        return nullptr;
    }
    std::string name = (char *)args;
    PARAM_LOGI("TestSendParamSetMsg name :\'%s\' ", name.c_str());
    SystemSetParameter(name.c_str(), name.c_str());
    return nullptr;
}

static void *TestSendParamWaitMsg(void *args)
{
    if (args == nullptr) {
        return nullptr;
    }
    std::string name = "Wati.";
    name = name + (char *)args;
    PARAM_LOGI("TestSendParamWaitMsg name :\'%s\' \n", name.c_str());
    SystemWaitParameter(name.c_str(), name.c_str(), 1);
    return nullptr;
}

static void TestForMultiThread()
{
    static const int threadMaxNumer = 2;
    PARAM_LOGI("TestForMultiThread \n");
    pthread_t tids[threadMaxNumer + threadMaxNumer];
    const char *names[] = {
        "thread.1111.2222.3333.4444.5555",
        "thread.2222.1111.2222.3333.4444",
        "thread.3333.1111.2222.4444.5555",
        "thread.4444.5555.1111.2222.3333",
        "thread.5555.1111.2222.3333.4444"
    };
    for (size_t i = 0; i < threadMaxNumer; i++) {
        pthread_create(&tids[i], nullptr, TestSendParamSetMsg,
            reinterpret_cast<void *>(const_cast<char *>(names[i % ARRAY_LENGTH(names)])));
    }
    for (size_t i = threadMaxNumer; i < threadMaxNumer + threadMaxNumer; i++) {
        pthread_create(&tids[i], nullptr, TestSendParamWaitMsg,
            reinterpret_cast<void *>(const_cast<char *>(names[i % ARRAY_LENGTH(names)])));
    }
    for (size_t i = 0; i < threadMaxNumer + threadMaxNumer; i++) {
        pthread_join(tids[i], nullptr);
    }
}

static void TestParamTraversal()
{
    SystemTraversalParameter(
        "",
        [](ParamHandle handle, void *cookie) {
            char value[PARAM_BUFFER_SIZE + PARAM_BUFFER_SIZE] = {0};
            uint32_t commitId = 0;
            int ret = SystemGetParameterCommitId(handle, &commitId);
            EXPECT_EQ(ret, 0);
            SystemGetParameterName(handle, value, PARAM_BUFFER_SIZE);
            u_int32_t len = PARAM_BUFFER_SIZE;
            SystemGetParameterValue(handle, ((char *)value) + PARAM_BUFFER_SIZE, &len);
            printf("$$$$$$$$Param %s=%s \n", (char *)value, ((char *)value) + PARAM_BUFFER_SIZE);
        },
        nullptr);
}

static void TestPermission()
{
    const char *testName = "persist.111.ffff.bbbb.cccc.dddd.eeee.55555";
    char tmp[PARAM_BUFFER_SIZE] = {0};
    int ret;

    ParamSecurityOps *paramSecurityOps = GetParamSecurityOps(0);
    EXPECT_NE(paramSecurityOps, nullptr);
    paramSecurityOps->securityCheckParamPermission = TestCheckParamPermission;
    SetTestPermissionResult(DAC_RESULT_FORBIDED);
    if ((GetParamSecurityLabel() != nullptr)) {
        GetParamSecurityLabel()->flags[0] = LABEL_CHECK_IN_ALL_PROCESS;
        ret = SystemSetParameter(testName, "22202");
#ifdef __LITEOS_A__
        EXPECT_EQ(ret, DAC_RESULT_FORBIDED);
#else
        EXPECT_EQ(ret, 0); // 本地不在校验
#endif
    }
    paramSecurityOps->securityFreeLabel = TestFreeLocalSecurityLabel;
    paramSecurityOps->securityCheckParamPermission = TestCheckParamPermission;
    SetTestPermissionResult(0);
    SystemWriteParam(testName, "22202");
    ret = SystemSetParameter(testName, "22202");
    ClientCheckParamValue(testName, "22202");

    const int testResult = 201;
    SetTestPermissionResult(testResult);
    ret = SystemSetParameter(testName, "3333");
#ifdef __LITEOS_A__
    EXPECT_EQ(ret, testResult);
#else
    EXPECT_EQ(ret, 0); // 本地不在校验
#endif

    u_int32_t len = sizeof(tmp);
    SetTestPermissionResult(DAC_RESULT_FORBIDED);
    ret = SystemGetParameter(testName, tmp, &len);
    EXPECT_EQ(ret, DAC_RESULT_FORBIDED);
    RegisterSecurityOps(0);
    SetTestPermissionResult(0); // recover testpermission result
}

void TestClientApi(char testBuffer[], uint32_t size, const char *name, const char *value)
{
    ParamHandle handle;
    int ret = SystemFindParameter(name, &handle);
    SystemWriteParam(name, value);
    SystemSetParameter(name, value);
    ret = SystemFindParameter(name, &handle);
    EXPECT_EQ(ret, 0);
    uint32_t commitId = 0;
    ret = SystemGetParameterCommitId(handle, &commitId);
    EXPECT_EQ(ret, 0);
    ret = SystemGetParameterName(handle, testBuffer, size);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(strcmp(testBuffer, name), 0);
    ret = SystemGetParameterValue(handle, testBuffer, &size);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(strcmp(testBuffer, value), 0);
}

namespace init_ut {
class ClientUnitTest : public ::testing::Test {
public:
    ClientUnitTest() {}
    virtual ~ClientUnitTest() {}
    static void SetUpTestCase(void)
    {
        PrepareInitUnitTestEnv();
    };

    void SetUp(void)
    {
        if (GetParamSecurityLabel() != nullptr) {
            GetParamSecurityLabel()->cred.uid = 1000;  // 1000 test uid
            GetParamSecurityLabel()->cred.gid = 1000;  // 1000 test gid
        }
    }
    void TearDown(void) {}
    void TestBody(void) {}
};

HWTEST_F(ClientUnitTest, TestClient_01, TestSize.Level0)
{
    const std::string name = "test.add.client.001.001";
    const std::string value = "test.add.client.value.001.001";
    // direct write
    SystemWriteParam(name.c_str(), value.c_str());
    SystemSetParameter(name.c_str(), value.c_str());
    ClientCheckParamValue(name.c_str(), value.c_str());
    SystemWaitParameter(name.c_str(), value.c_str(), 1);
    // wait
    SystemWaitParameter(name.c_str(), value.c_str(), 1);
    SystemWaitParameter(name.c_str(), nullptr, 0);
}

HWTEST_F(ClientUnitTest, TestClient_02, TestSize.Level0)
{
    char testBuffer[PARAM_BUFFER_SIZE] = {0};
    const std::string value = "test.add.client.value.001";
    const std::string name = "test.add.client.001.003";
    TestClientApi(testBuffer, PARAM_BUFFER_SIZE, name.c_str(), value.c_str());
}

HWTEST_F(ClientUnitTest, TestClient_03, TestSize.Level0)
{
    // 3 Traversal test
    TestParamTraversal();
    SystemDumpParameters(1, NULL);
}

HWTEST_F(ClientUnitTest, TestClient_04, TestSize.Level0)
{
    const std::string name = "test.add.client.001.004";
    int ret = WatchParamCheck(name.c_str());
#ifndef OHOS_LITE
    EXPECT_EQ(ret, 0);
#endif
    ret = WatchParamCheck("&&&&&.test.tttt");
    EXPECT_NE(ret, 0);
    // test permission
    TestPermission();
}

HWTEST_F(ClientUnitTest, TestClient_05, TestSize.Level0)
{
    TestForMultiThread();
}
}  // namespace init_ut