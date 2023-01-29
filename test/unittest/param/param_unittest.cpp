/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <gtest/gtest.h>

#include "init_param.h"
#include "param_message.h"
#include "param_stub.h"
#include "trigger_manager.h"
#include "param_utils.h"
#include "param_osadp.h"
#include "param_manager.h"

using namespace testing::ext;
using namespace std;

extern "C" {
int WorkSpaceNodeCompare(const HashNode *node1, const HashNode *node2);
}

static void OnClose(ParamTaskPtr client)
{
    UNUSED(client);
}

static int CheckServerParamValue(const char *name, const char *expectValue)
{
    char tmp[PARAM_BUFFER_SIZE] = {0};
    u_int32_t len = sizeof(tmp);
    SystemReadParam(name, tmp, &len);
    printf("CheckParamValue name %s value: \'%s\' expectValue:\'%s\' \n", name, tmp, expectValue);
    EXPECT_NE((int)strlen(tmp), 0);
    if (expectValue != nullptr) {
        EXPECT_EQ(strcmp(tmp, expectValue), 0);
    }
    return 0;
}

namespace init_ut {
class ParamUnitTest : public ::testing::Test {
public:
    ParamUnitTest() {}
    virtual ~ParamUnitTest() {}

    static void SetUpTestCase(void)
    {
        PrepareInitUnitTestEnv();
    };
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

    int TestSetParams(const char *params[][1 + 1], int num)
    {
        for (int i = 0; i < num; i++) {
            SystemWriteParam(params[i][0], params[i][1]);
        }

        // check
        for (int i = 0; i < num; i++) {
            CheckServerParamValue(params[i][0], params[i][1]);
        }

        for (int i = num - 1; i >= 0; i--) {
            CheckServerParamValue(params[i][0], params[i][1]);
        }
        return 0;
    }

    int TestAddSecurityLabel1()
    {
        GetParamSecurityLabel()->cred.gid = 9999;  // 9999 test gid
        const char *name = "label1.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        uint32_t labelIndex = 0;
        SystemWriteParam(name, value);
        // get root
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_NAME_DAC);
        (void)FindTrieNode(workspace, name, strlen(name), &labelIndex);
        ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(workspace, labelIndex);
        if (node == nullptr) {
            EXPECT_EQ(1, 0);
            return 0;
        }
        EXPECT_EQ(node->gid, DAC_DEFAULT_GROUP);
        return 0;
    }

    // 添加一个label，最长匹配到这个节点
    int TestAddSecurityLabel2()
    {
        GetParamSecurityLabel()->cred.gid = 9999;  // 9999 test gid
        const char *name = "label2.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        ParamAuditData auditData = {};
        auditData.name = "label2.test.aaa";
        auditData.dacData.gid = 202;  // 202 test dac gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666;  // 0666 test mode
        SystemWriteParam(name, value);
        uint32_t labelIndex = 0;
        AddSecurityLabel(&auditData);
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_NAME_DAC);
        (void)FindTrieNode(workspace, name, strlen(name), &labelIndex);
        ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(workspace, labelIndex);
        if (node == nullptr) {
            EXPECT_EQ(1, 0);
            return 0;
        }
        EXPECT_EQ(node->gid, auditData.dacData.gid);
        return 0;
    }

    // 添加一个label，最长匹配到最后一个相同节点
    int TestAddSecurityLabel3()
    {
        GetParamSecurityLabel()->cred.gid = 9999;  // 9999 test gid
        const char *name = "label3.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        ParamAuditData auditData = {};
        auditData.name = "label3.test.aaa";
        auditData.dacData.gid = 203;  // 203 test gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666;  // 0666 test mode
        SystemWriteParam(name, value);
        AddSecurityLabel(&auditData);

        auditData.name = "label3.test.aaa.bbb.ccc.dddd.eee.dddd";
        auditData.dacData.gid = 202;  // 202 test dac gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666;  // 0666 test mode
        SystemWriteParam(name, value);
        AddSecurityLabel(&auditData);

        uint32_t labelIndex = 0;
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_NAME_DAC);
        ParamTrieNode *paramNode = FindTrieNode(workspace, name, strlen(name), &labelIndex);
        ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(workspace, labelIndex);
        if (paramNode == nullptr || node == nullptr) {
            EXPECT_EQ(1, 0);
            return 0;
        }
        EXPECT_EQ((int)node->gid, 203);  // 203 test gid
        return 0;
    }

    // 添加一个label，完全匹配
    int TestAddSecurityLabel4()
    {
        GetParamSecurityLabel()->cred.gid = 9999;  // 9999 test gid
        const char *name = "label4.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        ParamAuditData auditData = {};
        auditData.name = "label4.test.aaa.bbb.ccc.dddd.eee";
        auditData.dacData.gid = 203;  // 203 test gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666;  // 0666 test mode
        SystemWriteParam(name, value);
        uint32_t labelIndex = 0;
        AddSecurityLabel(&auditData);
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_NAME_DAC);
        ParamTrieNode *paramNode = FindTrieNode(workspace, name, strlen(name), &labelIndex);
        ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(workspace, labelIndex);
        if (paramNode == nullptr || node == nullptr) {
            EXPECT_EQ(1, 0);
            return 0;
        }
        EXPECT_EQ(node->gid, auditData.dacData.gid);
        return 0;
    }

    void TestBufferValue(char *buffer, uint32_t len)
    {
        const int printBase = 10;
        for (uint32_t index = 0; index <= len; index++) {
            buffer[index] = '0' + index % printBase;
            if (index != 0 && index % printBase == 0) {
                buffer[index] = '.';
            }
        }
        buffer[len] = '\0';
    }

    int TestNameIsValid()
    {
        char buffer[PARAM_BUFFER_SIZE];
        // set name length = 127
        TestBufferValue(buffer, PARAM_NAME_LEN_MAX - 1);
        int ret = SystemWriteParam(buffer, "1111");
        EXPECT_EQ(ret, 0);
        TestBufferValue(buffer, PARAM_NAME_LEN_MAX);
        ret = SystemWriteParam(buffer, "1111");
        EXPECT_NE(ret, 0);
        TestBufferValue(buffer, PARAM_NAME_LEN_MAX + 1);
        ret = SystemWriteParam(buffer, "1111");
        EXPECT_NE(ret, 0);

        // 保存一个只读的属性，大于最大值
        TestBufferValue(buffer, PARAM_VALUE_LEN_MAX - 1);
        ret = SystemWriteParam("const.test_readonly.dddddddddddddddddd.fffffffffffffffffff", buffer);
        EXPECT_EQ(ret, 0);

        TestBufferValue(buffer, PARAM_VALUE_LEN_MAX + 1);
        ret = SystemWriteParam("const.test_readonly.aaaaaaaaaaaaaaaaaa.fffffffffffffffffff", buffer);
        EXPECT_EQ(ret, 0);

        // 更新只读项目
        TestBufferValue(buffer, PARAM_VALUE_LEN_MAX - 1);
        ret = SystemWriteParam("const.test_readonly.dddddddddddddddddd.fffffffffffffffffff", buffer);
        EXPECT_NE(ret, 0);

        // 写普通属性
        TestBufferValue(buffer, PARAM_VALUE_LEN_MAX - 1);
        ret = SystemWriteParam("test.bigvalue.dddddddddddddddddd.fffffffffffffffffff", buffer);
        EXPECT_EQ(ret, 0);
        TestBufferValue(buffer, PARAM_VALUE_LEN_MAX + 1);
        ret = SystemWriteParam("test.bigvalue.dddddddddddddddddd.fffffffffffffffffff", buffer);
        EXPECT_NE(ret, 0);

        // invalid name
        TestBufferValue(buffer, PARAM_VALUE_LEN_MAX - 1);
        ret = SystemWriteParam("test.invalidname..fffffffffffffffffff", buffer);
        EXPECT_NE(ret, 0);
        ret = SystemWriteParam("test.invalidname.%fffffffffffffffffff", buffer);
        EXPECT_NE(ret, 0);
        ret = SystemWriteParam("test.invalidname.$fffffffffffffffffff", buffer);
        EXPECT_NE(ret, 0);
        return 0;
    }

    int TestParamTraversal()
    {
        char value[PARAM_BUFFER_SIZE + PARAM_BUFFER_SIZE] = {0};
        SystemTraversalParameter(
            "",
            [](ParamHandle handle, void *cookie) {
                SystemGetParameterName(handle, (char *)cookie, PARAM_BUFFER_SIZE);
                u_int32_t len = PARAM_BUFFER_SIZE;
                SystemGetParameterValue(handle, ((char *)cookie) + PARAM_BUFFER_SIZE, &len);
                printf("$$$$$$$$Param %s=%s \n", (char *)cookie, ((char *)cookie) + PARAM_BUFFER_SIZE);
            },
            (void *)value);
        return 0;
    }

    int TestUpdateParam(const char *name, const char *value)
    {
        SystemWriteParam(name, value);
        SystemWriteParam(name, value);
        SystemWriteParam(name, value);
        SystemWriteParam(name, value);
        CheckServerParamValue(name, value);
        return 0;
    }

    int TestPersistParam()
    {
        ParamAuditData auditData = {};
        auditData.name = "persist.";
        auditData.dacData.gid = 0;
        auditData.dacData.uid = 0;
        auditData.dacData.mode = 0777;  // 0777 test mode
        AddSecurityLabel(&auditData);

        LoadPersistParams();
        SystemWriteParam("persist.111.ffff.bbbb.cccc.dddd.eeee", "1101");
        SystemWriteParam("persist.111.aaaa.bbbb.cccc.dddd.eeee", "1102");
        SystemWriteParam("persist.111.bbbb.cccc.dddd.eeee", "1103");
        CheckServerParamValue("persist.111.bbbb.cccc.dddd.eeee", "1103");

        SystemWriteParam("persist.111.cccc.bbbb.cccc.dddd.eeee", "1104");
        SystemWriteParam("persist.111.eeee.bbbb.cccc.dddd.eeee", "1105");
        CheckServerParamValue("persist.111.ffff.bbbb.cccc.dddd.eeee", "1101");
        SystemWriteParam("persist.111.ffff.bbbb.cccc.dddd.eeee", "1106");
        CheckServerParamValue("persist.111.ffff.bbbb.cccc.dddd.eeee", "1106");
        sleep(1);
        SystemWriteParam("persist.111.bbbb.cccc.dddd.1107", "1107");
        SystemWriteParam("persist.111.bbbb.cccc.dddd.1108", "1108");
        SystemWriteParam("persist.111.bbbb.cccc.dddd.1109", "1108");
        sleep(1);
        SystemWriteParam("persist.111.bbbb.cccc.dddd.1110", "1108");
        SystemWriteParam("persist.111.bbbb.cccc.dddd.1111", "1108");
        TimerCallbackForSave(nullptr, nullptr);
        LoadPersistParams();
        return 0;
    }

    int TestDumpParamMemory()
    {
        SystemDumpParameters(1, NULL);
        return 0;
    }
};

HWTEST_F(ParamUnitTest, TestPersistParam, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestPersistParam();
}

HWTEST_F(ParamUnitTest, TestSetParam_1, TestSize.Level0)
{
    ParamUnitTest test;
    const char *params[][2] = {{"111.2222", "1"},
                               {"111.2222.3333", "2"},
                               {"111.2222.3333.4444", "3"},
                               {"111.2222.3333.4444.666", "4"},
                               {"111.2222.3333.4444.666.777", "5"}};
    test.TestSetParams(params, 5);
}

HWTEST_F(ParamUnitTest, TestSetParam_2, TestSize.Level0)
{
    ParamUnitTest test;
    const char *params[][2] = {{"111.2222.xxxx.xxx.xxx", "1_1"},
                               {"111.2222.3333.xxx", "1_2"},
                               {"111.2222.xxxx.3333.4444", "1_3"},
                               {"111.2222.3333.xxxx.4444.666", "1_4"},
                               {"111.2222.3333.4444.666.xxxxx.777", "1_5"}};
    test.TestSetParams(params, 5);

    const char *ctrlParams[][2] = {{"ctl.start.111.2222.xxxx.xxx.xxx", "2_1"},
                                   {"ctl.start.111.2222.3333.xxx", "2_2"},
                                   {"ctl.start.111.2222.xxxx.3333.4444", "2_3"},
                                   {"ctl.start.111.2222.3333.xxxx.4444.666", "2_4"},
                                   {"ctl.start.111.2222.3333.4444.666.xxxxx.777", "2_5"}};
    test.TestSetParams(ctrlParams, 5);

    const char *sysParams[][2] = {{"ohos.startup.powerctrl.111.2222.xxxx.xxx.xxx", "3_1"},
                                  {"ohos.startup.powerctrl.111.2222.3333.xxx", "3_2"},
                                  {"ohos.startup.powerctrl.111.2222.xxxx.3333.4444", "3_3"},
                                  {"ohos.startup.powerctrl.111.2222.3333.xxxx.4444.666", "3_4"},
                                  {"ohos.startup.powerctrl.111.2222.3333.4444.666.xxxxx.777", "3_5"}};
    test.TestSetParams(sysParams, 5);
}

HWTEST_F(ParamUnitTest, TestNameIsValid, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestNameIsValid();
}

HWTEST_F(ParamUnitTest, TestAddSecurityLabel1, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddSecurityLabel1();
}

HWTEST_F(ParamUnitTest, TestAddSecurityLabel2, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddSecurityLabel2();
}

HWTEST_F(ParamUnitTest, TestAddSecurityLabel3, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddSecurityLabel3();
}

HWTEST_F(ParamUnitTest, TestAddSecurityLabel4, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddSecurityLabel4();
}

HWTEST_F(ParamUnitTest, TestUpdateParam, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestUpdateParam("test.aaa.bbb.ccc.ddd", "100");
    test.TestUpdateParam("test.aaa.bbb.ccc.ddd", "101");
    test.TestUpdateParam("test.aaa.bbb.ccc.ddd", "102");
    test.TestUpdateParam("test.aaa.bbb.ccc.ddd", "103");
    test.TestUpdateParam("net.tcp.default_init_rwnd", "60");
}

HWTEST_F(ParamUnitTest, TestParamTraversal, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestParamTraversal();
}

HWTEST_F(ParamUnitTest, TestDumpParamMemory, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestDumpParamMemory();
}
HWTEST_F(ParamUnitTest, TestLinuxRWLock, TestSize.Level0)
{
    ParamRWMutexCreate(nullptr);
    ParamRWMutexWRLock(nullptr);
    ParamRWMutexRDLock(nullptr);
    ParamRWMutexUnlock(nullptr);
    ParamRWMutexDelete(nullptr);
    ParamMutexDelete(nullptr);
    WorkSpace *workspace1 = (WorkSpace *)malloc(sizeof(WorkSpace) + strlen("testfilename1"));
    ASSERT_NE(nullptr, workspace1);
    WorkSpace *workspace2 = (WorkSpace *)malloc(sizeof(WorkSpace) + strlen("testfilename1"));
    ASSERT_NE(nullptr, workspace2);
    if (strcpy_s(workspace1->fileName, strlen("testfilename1"), "testfilename") != EOK) {
        EXPECT_EQ(0, 1);
    }
    if (strcpy_s(workspace2->fileName, strlen("testfilename1"), "testfilename") != EOK) {
        EXPECT_EQ(0, 1);
    }
    EXPECT_EQ(WorkSpaceNodeCompare(&(workspace1->hashNode), &(workspace2->hashNode)), 0);
    free(workspace1);
    free(workspace2);
}

HWTEST_F(ParamUnitTest, TestGetServiceCtlName, TestSize.Level0)
{
    ServiceCtrlInfo *serviceInfo = NULL;
    GetServiceCtrlInfo("ohos.startup.powerctrl", "reboot,updater", &serviceInfo);
    if (serviceInfo != NULL) {
        EXPECT_STREQ(serviceInfo->realKey, "ohos.servicectrl.reboot.updater.reboot,updater");
        free(serviceInfo);
    }
    GetServiceCtrlInfo("ohos.ctl.stop", "test", &serviceInfo);
    if (serviceInfo != NULL) {
        EXPECT_STREQ(serviceInfo->realKey, "ohos.servicectrl.stop.test");
        free(serviceInfo);
    }
    GetServiceCtrlInfo("ohos.servicectrl.stop", "test", &serviceInfo);
    if (serviceInfo != NULL) {
        EXPECT_STREQ(serviceInfo->realKey, "ohos.servicectrl.stop.test");
        free(serviceInfo);
    }
}

HWTEST_F(ParamUnitTest, TestParamCache, TestSize.Level0)
{
    const char *name = "test.write.1111111.222222";
    CachedHandle cacheHandle = CachedParameterCreate(name, "true");
    EXPECT_NE(cacheHandle, nullptr);
    const char *value = CachedParameterGet(cacheHandle);
    EXPECT_EQ(strcmp(value, "true"), 0);
    uint32_t dataIndex = 0;
    int ret = WriteParam(name, "false", &dataIndex, 0);
    EXPECT_EQ(ret, 0);
    value = CachedParameterGet(cacheHandle);
    EXPECT_EQ(strcmp(value, "false"), 0);
    CachedParameterDestroy(cacheHandle);

    // cache 2, for parameter exist
    CachedHandle cacheHandle3 = CachedParameterCreate(name, "true");
    EXPECT_NE(cacheHandle3, nullptr);
    value = CachedParameterGet(cacheHandle3);
    EXPECT_EQ(strcmp(value, "false"), 0);
    ret = WriteParam(name, "2222222", &dataIndex, 0);
    EXPECT_EQ(ret, 0);
    value = CachedParameterGet(cacheHandle3);
    EXPECT_EQ(strcmp(value, "2222222"), 0);
    CachedParameterDestroy(cacheHandle3);
}
}