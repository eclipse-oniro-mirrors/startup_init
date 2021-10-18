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

#include "init_unittest.h"
#include "init_utils.h"
#include "param_request.h"
#include "param_stub.h"
#include "sys_param.h"

using namespace std;

static int g_testPermissionResult = DAC_RESULT_PERMISSION;
static void ClientCheckParamValue(const char *name, const char *expectValue)
{
    char tmp[PARAM_BUFFER_SIZE] = { 0 };
    u_int32_t len = sizeof(tmp);
    int ret = SystemGetParameter(name, tmp, &len);
    printf("ClientCheckParamValue name %s value: \'%s\' expectValue:\'%s\' \n", name, tmp, expectValue);
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
    std::string name = (char *)args;
    printf("TestSendParamSetMsg name :\'%s\' \n", name.c_str());
    SystemSetParameter(name.c_str(), name.c_str());
    ClientCheckParamValue(name.c_str(), name.c_str());
    return nullptr;
}

static void *TestSendParamWaitMsg(void *args)
{
    std::string name = "Wati.";
    name = name + (char *)args;
    printf("TestSendParamWaitMsg name :\'%s\' \n", name.c_str());
    SystemWaitParameter(name.c_str(), name.c_str(), 1);
    return nullptr;
}

static void TestForMultiThread()
{
    static const int threadMaxNumer = 2;
    printf("TestForMultiThread \n");
    pthread_t tids[threadMaxNumer + threadMaxNumer];
    const char *names[] = {
        "thread.1111.2222.3333.4444.5555",
        "thread.2222.1111.2222.3333.4444",
        "thread.3333.1111.2222.4444.5555",
        "thread.4444.5555.1111.2222.3333",
        "thread.5555.1111.2222.3333.4444"
    };
    for (size_t i = 0; i < threadMaxNumer; i++) {
        pthread_create(&tids[i], nullptr, TestSendParamSetMsg, (void *)names[i % ARRAY_LENGTH(names)]);
    }
    for (size_t i = threadMaxNumer; i < threadMaxNumer + threadMaxNumer; i++) {
        pthread_create(&tids[i], nullptr, TestSendParamWaitMsg, (void *)names[i % ARRAY_LENGTH(names)]);
    }
    for (size_t i = 0; i < threadMaxNumer + threadMaxNumer; i++) {
        pthread_join(tids[i], nullptr);
    }
}

static void TestParamTraversal()
{
    SystemTraversalParameter(
        [](ParamHandle handle, void *cookie) {
            char value[PARAM_BUFFER_SIZE + PARAM_BUFFER_SIZE] = { 0 };
            uint32_t commitId = 0;
            int ret = SystemGetParameterCommitId(handle, &commitId);
            EXPECT_EQ(ret, 0);
            SystemGetParameterName(handle, value, PARAM_BUFFER_SIZE);
            u_int32_t len = PARAM_BUFFER_SIZE;
            SystemGetParameterValue(handle, ((char *)value) + PARAM_BUFFER_SIZE, &len);
            printf("$$$$$$$$Param %s=%s \n", (char *)value, ((char *)value) + PARAM_BUFFER_SIZE);
        }, nullptr);
}

static void TestPersistParam()
{
    SystemSetParameter("persist.111.ffff.bbbb.cccc.dddd.eeee", "1101");
    SystemSetParameter("persist.111.aaaa.bbbb.cccc.dddd.eeee", "1102");
    SystemSetParameter("persist.111.bbbb.cccc.dddd.eeee", "1103");
    ClientCheckParamValue("persist.111.bbbb.cccc.dddd.eeee", "1103");
    SystemSetParameter("persist.111.cccc.bbbb.cccc.dddd.eeee", "1104");
    SystemSetParameter("persist.111.eeee.bbbb.cccc.dddd.eeee", "1105");
    ClientCheckParamValue("persist.111.ffff.bbbb.cccc.dddd.eeee", "1101");
    SystemSetParameter("persist.111.ffff.bbbb.cccc.dddd.eeee", "1106");
    ClientCheckParamValue("persist.111.ffff.bbbb.cccc.dddd.eeee", "1106");
}

static void TestPermission()
{
    const char *testName = "persist.111.ffff.bbbb.cccc.dddd.eeee.55555";
    char tmp[PARAM_BUFFER_SIZE] = { 0 };
    // 允许本地校验
    ParamSecurityOps *paramSecurityOps = &GetClientParamWorkSpace()->paramSecurityOps;
    paramSecurityOps->securityCheckParamPermission = TestCheckParamPermission;
    g_testPermissionResult = DAC_RESULT_FORBIDED;
    GetClientParamWorkSpace()->securityLabel->flags = LABEL_CHECK_FOR_ALL_PROCESS;
    int ret = SystemSetParameter(testName, "22202");
    EXPECT_EQ(ret, DAC_RESULT_FORBIDED);

    paramSecurityOps->securityEncodeLabel = TestEncodeSecurityLabel;
    paramSecurityOps->securityDecodeLabel = TestDecodeSecurityLabel;
    paramSecurityOps->securityFreeLabel = TestFreeLocalSecurityLabel;
    paramSecurityOps->securityCheckParamPermission = TestCheckParamPermission;
    g_testPermissionResult = 0;
    ret = SystemSetParameter(testName, "22202");
    EXPECT_EQ(ret, 0);
    ClientCheckParamValue(testName, "22202");

    const int testResult = 201;
    g_testPermissionResult = testResult;
    // 禁止写/读
    ret = SystemSetParameter(testName, "3333");
    EXPECT_EQ(ret, testResult);
    u_int32_t len = sizeof(tmp);
    ret = SystemGetParameter(testName, tmp, &len);
    EXPECT_EQ(ret, testResult);
    RegisterSecurityOps(paramSecurityOps, 0);
}

static void TestCmd()
{
    // set
    const char *argForSet[] = { "param", "set", "aaaa", "2222" };
    RunParamCommand(ARRAY_LENGTH(argForSet), const_cast<char **>(argForSet));

    // set fail
    const char *argForSet1[] = { "param", "set", "aaaa&&&&&&&&&####", "2222" };
    RunParamCommand(ARRAY_LENGTH(argForSet1), const_cast<char **>(argForSet1));

    // get
    const char *argForGet[] = { "param", "get", "aaaa" };
    RunParamCommand(ARRAY_LENGTH(argForGet), const_cast<char **>(argForGet));

    // get not exit
    const char *argForGet1[] = { "param", "get", "aaaaaaaa" };
    RunParamCommand(ARRAY_LENGTH(argForGet1), const_cast<char **>(argForGet1));

    // get all
    const char *argForGet2[] = { "param", "get" };
    RunParamCommand(ARRAY_LENGTH(argForGet2), const_cast<char **>(argForGet2));

    // set 失败
    const char *argForSet2[] = { "param", "set", "aaaa" };
    RunParamCommand(ARRAY_LENGTH(argForSet2), const_cast<char **>(argForSet2));

    const char *argForSet3[] = { "paramset", "aaaa", "aaaa" };
    RunParamCommand(ARRAY_LENGTH(argForSet3), const_cast<char **>(argForSet3));

    const char *argForSet4[] = { "paramset", "aaaa" };
    RunParamCommand(ARRAY_LENGTH(argForSet4), const_cast<char **>(argForSet4));

    const char *argForSet5[] = { "param" };
    RunParamCommand(ARRAY_LENGTH(argForSet5), const_cast<char **>(argForSet5));

    const char *argForSet6[] = { "param", "44444" };
    RunParamCommand(ARRAY_LENGTH(argForSet6), const_cast<char **>(argForSet6));

    const char *argForSet7[] = { "paramget", "aaaa" };
    RunParamCommand(ARRAY_LENGTH(argForSet7), const_cast<char **>(argForSet7));

    const char *argForSet8[] = { "param", "dump", "verbose" };
    RunParamCommand(ARRAY_LENGTH(argForSet8), const_cast<char **>(argForSet8));

    const char *argForSet9[] = { "param", "wait", "test.wait.001", "*", "1" };
    RunParamCommand(ARRAY_LENGTH(argForSet9), const_cast<char **>(argForSet9));
}

void TestClient(int index)
{
    char testBuffer[PARAM_BUFFER_SIZE] = { 0 };
    const std::string value = "test.add.client.value.001";
    std::string name = "test.add.client.001";
    name += std::to_string(index);
    switch (index) {
        case 0: {
            ParamWorkSpace *space = GetClientParamWorkSpace();
            if (space != nullptr && space->securityLabel != nullptr) {
                space->securityLabel->cred.uid = 1000; // 1000 test uid
                space->securityLabel->cred.gid = 1000; // 1000 test gid
            }
        }
        case 1: { // 1 set test
            SystemSetParameter(name.c_str(), value.c_str());
            ClientCheckParamValue(name.c_str(), value.c_str());
            SystemWaitParameter(name.c_str(), value.c_str(), 1);
            TestPersistParam();
            // wait
            SystemWaitParameter(name.c_str(), value.c_str(), 1);
            SystemWaitParameter(name.c_str(), nullptr, 0);
            break;
        }
        case 2: { // 2 api test
            ParamHandle handle;
            uint32_t size = PARAM_BUFFER_SIZE;
            int ret = SystemFindParameter(name.c_str(), &handle);
            SystemSetParameter(name.c_str(), value.c_str());
            ret = SystemFindParameter(name.c_str(), &handle);
            EXPECT_EQ(ret, 0);
            uint32_t commitId = 0;
            ret = SystemGetParameterCommitId(handle, &commitId);
            EXPECT_EQ(ret, 0);
            ret = SystemGetParameterName(handle, testBuffer, size);
            EXPECT_EQ(ret, 0);
            EXPECT_EQ(strcmp(testBuffer, name.c_str()), 0);
            ret = SystemGetParameterValue(handle, testBuffer, &size);
            EXPECT_EQ(ret, 0);
            EXPECT_EQ(strcmp(testBuffer, value.c_str()), 0);
            break;
        }
        case 3: // 3 Traversal test
            TestParamTraversal();
            SystemDumpParameters(1);
            break;
        case 4: { // 4 watcher test
            int ret = WatchParamCheck(name.c_str());
            EXPECT_EQ(ret, 0);
            ret = WatchParamCheck("&&&&&.test.tttt");
            EXPECT_NE(ret, 0);
            // test permission
            TestPermission();
            // test cmd
            TestCmd();
            break;
        }
        case 5: // 5 multi thread test
            TestForMultiThread();
            break;
        default:
            break;
    }
}

int TestEncodeSecurityLabel(const ParamSecurityLabel *srcLabel, char *buffer, uint32_t *bufferSize)
{
    PARAM_CHECK(bufferSize != nullptr, return -1, "Invalid param");
    if (buffer == nullptr) {
        *bufferSize = sizeof(ParamSecurityLabel);
        return 0;
    }
    PARAM_CHECK(*bufferSize >= sizeof(ParamSecurityLabel), return -1, "Invalid buffersize %u", *bufferSize);
    *bufferSize = sizeof(ParamSecurityLabel);
    return memcpy_s(buffer, *bufferSize, srcLabel, sizeof(ParamSecurityLabel));
}

int TestDecodeSecurityLabel(ParamSecurityLabel **srcLabel, const char *buffer, uint32_t bufferSize)
{
    PARAM_CHECK(bufferSize >= sizeof(ParamSecurityLabel), return -1, "Invalid buffersize %u", bufferSize);
    PARAM_CHECK(srcLabel != nullptr && buffer != nullptr, return -1, "Invalid param");
    *srcLabel = (ParamSecurityLabel *)buffer;
    return 0;
}

int TestCheckParamPermission(const ParamSecurityLabel *srcLabel, const ParamAuditData *auditData, uint32_t mode)
{
    // DAC_RESULT_FORBIDED
    return g_testPermissionResult;
}

int TestFreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}