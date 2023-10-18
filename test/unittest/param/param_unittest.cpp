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
#include "param_base.h"
#include "param_message.h"
#include "param_stub.h"
#include "param_init.h"
#include "trigger_manager.h"
#include "param_utils.h"
#include "param_osadp.h"
#include "param_manager.h"
#include "sys_param.h"

using namespace testing::ext;
using namespace std;

extern "C" {
void ParamWorBaseLog(InitLogLevel logLevel, uint32_t domain, const char *tag, const char *fmt, ...);
static void OnClose(const TaskHandle taskHandle)
{
}
}

static int CheckServerParamValue(const char *name, const char *expectValue)
{
    char tmp[PARAM_BUFFER_SIZE] = {0};
    u_int32_t len = sizeof(tmp);
    SystemReadParam(name, tmp, &len);
    printf("CheckParamValue name %s value: \'%s\' expectValue:\'%s\' \n", name, tmp, expectValue);
    EXPECT_GE((int)strlen(tmp), 0);
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

    static void SetUpTestCase(void) {}
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
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_INDEX_DAC);
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
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_INDEX_DAC);
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
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_INDEX_DAC);
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
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
        ResetParamSecurityLabel();
#endif
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
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_INDEX_DAC);
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
        SystemDumpParameters(1, -1, nullptr);
        return 0;
    }

    uint32_t GetWorkSpaceIndex(const char *name)
    {
#ifdef PARAM_SUPPORT_SELINUX
        ParamWorkSpace *paramSpace = GetParamWorkSpace();
        PARAM_CHECK(paramSpace != nullptr, return (uint32_t)-1, "Invalid paramSpace");
        return (paramSpace->selinuxSpace.getParamLabelIndex != nullptr) ?
            paramSpace->selinuxSpace.getParamLabelIndex(name) + WORKSPACE_INDEX_BASE : (uint32_t)-1;
#else
        return 0;
#endif
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

HWTEST_F(ParamUnitTest, TestParamValue, TestSize.Level0)
{
    // support empty string
    const char *name = "test_readonly.dddddddddddddddddd.fffffffffffffffffff";
    int ret = SystemWriteParam(name, "");
    EXPECT_EQ(ret, 0);
    CheckServerParamValue(name, "");
    ret = SystemWriteParam(name, "111111111");
    EXPECT_EQ(ret, 0);
    CheckServerParamValue(name, "111111111");
    ret = SystemWriteParam(name, "");
    EXPECT_EQ(ret, 0);
    CheckServerParamValue(name, "");
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
}

HWTEST_F(ParamUnitTest, TestWorkSpace1, TestSize.Level0)
{
    int ret = AddWorkSpace("test.workspace.1", GetWorkSpaceIndex("test.workspace.1"), 0, PARAM_WORKSPACE_DEF);
    EXPECT_EQ(ret, 0);
    ret = AddWorkSpace("test.workspace.2", GetWorkSpaceIndex("test.workspace.2"), 0, PARAM_WORKSPACE_DEF);
    EXPECT_EQ(ret, 0);
    ret = AddWorkSpace("test.workspace.3", GetWorkSpaceIndex("test.workspace.3"), 0, PARAM_WORKSPACE_DEF);
    EXPECT_EQ(ret, 0);
    WorkSpace *space = GetWorkSpace(GetWorkSpaceIndex("test.workspace.1"));
    EXPECT_NE(space, nullptr);
    CloseWorkSpace(nullptr);
}

HWTEST_F(ParamUnitTest, TestWorkSpace2, TestSize.Level0)
{
    const char *spaceName = "test.workspace2";
    const size_t size = strlen(spaceName) + 1;
    WorkSpace *workSpace = (WorkSpace *)malloc(sizeof(WorkSpace) + size);
    if (workSpace == nullptr) {
        EXPECT_NE(workSpace, nullptr);
        return;
    }
    workSpace->flags = 0;
    workSpace->area = nullptr;
    int ret = PARAM_STRCPY(workSpace->fileName, size, spaceName);
    EXPECT_EQ(ret, 0);
    CloseWorkSpace(workSpace);
    free(workSpace);
}

#if !(defined __LITEOS_A__ || defined __LITEOS_M__) // can not support parameter type
HWTEST_F(ParamUnitTest, TestParamValueType1, TestSize.Level0)
{
    int ret = SystemWriteParam("test.type.int.1000", "10000");
    EXPECT_EQ(ret, 0);

    ret = SystemWriteParam("test.type.int.1001", "-1111111144444444444441111111666666661");
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);
    ret = SystemWriteParam("test.type.int.1001", "1111111111444444444444411111166666666");
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);
}

HWTEST_F(ParamUnitTest, TestParamValueType2, TestSize.Level0)
{
    int ret = SystemWriteParam("test.type.bool.1000", "10000");
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);

    ret = SystemWriteParam("test.type.bool.1001", "-1111111111111111");
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);
    ret = SystemWriteParam("test.type.bool.1001", "1111111111111111");
    EXPECT_EQ(ret, PARAM_CODE_INVALID_VALUE);
    ret = SystemWriteParam("test.type.bool.1001", "true");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "false");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "1");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "0");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "on");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "off");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "yes");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "no");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "y");
    EXPECT_EQ(ret, 0);
    ret = SystemWriteParam("test.type.bool.1001", "n");
    EXPECT_EQ(ret, 0);
}
#endif

HWTEST_F(ParamUnitTest, TestGetServiceCtlName, TestSize.Level0)
{
    ServiceCtrlInfo *serviceInfo = nullptr;
    GetServiceCtrlInfo("ohos.startup.powerctrl", "reboot,updater", &serviceInfo);
    if (serviceInfo != nullptr) {
        EXPECT_STREQ(serviceInfo->realKey, "ohos.servicectrl.reboot.updater.reboot,updater");
        free(serviceInfo);
    }
    GetServiceCtrlInfo("ohos.ctl.stop", "test", &serviceInfo);
    if (serviceInfo != nullptr) {
        EXPECT_STREQ(serviceInfo->realKey, "ohos.servicectrl.stop.test");
        free(serviceInfo);
    }
    GetServiceCtrlInfo("ohos.servicectrl.stop", "test", &serviceInfo);
    if (serviceInfo != nullptr) {
        EXPECT_STREQ(serviceInfo->realKey, "ohos.servicectrl.stop.test");
        free(serviceInfo);
    }
    ParamWorBaseLog(INIT_DEBUG, PARAN_DOMAIN, "PARAM", "%s", "ParamWorBaseLog");
}

HWTEST_F(ParamUnitTest, TestFindTrieNode, TestSize.Level0)
{
    int ret = AddWorkSpace("test.workspace.1", GetWorkSpaceIndex("test.workspace.1"), 0, PARAM_WORKSPACE_DEF);
    EXPECT_EQ(ret, 0);
    WorkSpace *space = GetWorkSpaceByName("test.workspace.1");
    ASSERT_NE(space, nullptr);
    ParamTrieNode *node = FindTrieNode(nullptr, nullptr, 0, nullptr);
    ASSERT_EQ(node, nullptr);
    node = FindTrieNode(space, nullptr, 0, nullptr);
    ASSERT_EQ(node, nullptr);
    node = FindTrieNode(space, "111111", 0, nullptr);
    ASSERT_EQ(node, nullptr);
    node = FindTrieNode(space, "find.test.111111", strlen("find.test.111111"), nullptr);
    ASSERT_EQ(node, nullptr);
}

#ifndef OHOS_LITE
HWTEST_F(ParamUnitTest, TestConnectServer, TestSize.Level0)
{
    int ret = ConnectServer(-1, CLIENT_PIPE_NAME);
    EXPECT_NE(ret, 0);
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    ret = ConnectServer(fd, "");
    EXPECT_NE(ret, 0);
    ret = ConnectServer(fd, CLIENT_PIPE_NAME);
    EXPECT_EQ(ret, 0);
    close(fd);
}

HWTEST_F(ParamUnitTest, TestRequestMessage, TestSize.Level0)
{
    const int maxSize = 1024 * 64 + 10;
    const int msgSize = sizeof(ParamMessage) + 128; // 128 TEST
    ParamMessage *msg = CreateParamMessage(0, nullptr, msgSize);
    EXPECT_EQ(msg, nullptr);
    msg = CreateParamMessage(0, "nullptr", maxSize);
    EXPECT_EQ(msg, nullptr);
    msg = CreateParamMessage(0, "22222222222222222222222222222222222222222"
        "333333333333333333333333333333333333333333333333333333333333333333333"
        "555555555555555555555555555555555555555555555555555555555555555555555", msgSize);
    EXPECT_EQ(msg, nullptr);

    // success
    msg = CreateParamMessage(0, "22222222222222222222222222222222222222222", msgSize);
    EXPECT_NE(msg, nullptr);
    uint32_t start = 0;
    int ret = FillParamMsgContent(nullptr, &start, 0, nullptr, 0);
    EXPECT_NE(ret, 0);
    ret = FillParamMsgContent(msg, nullptr, 0, nullptr, 0);
    EXPECT_NE(ret, 0);
    ret = FillParamMsgContent(msg, &start, 0, nullptr, 0);
    EXPECT_NE(ret, 0);
    ret = FillParamMsgContent(msg, &start, 0, "22222", 0);
    EXPECT_EQ(ret, 0);
    ret = FillParamMsgContent(msg, &start, 0, "22222", msgSize);
    EXPECT_NE(ret, 0);
    // fill success
    ret = FillParamMsgContent(msg, &start, 0, "22222", strlen("22222"));
    EXPECT_EQ(ret, 0);
    msg->msgSize = start + sizeof(ParamMessage);

    uint32_t offset = 0;
    ParamMsgContent *content = GetNextContent(nullptr, &offset);
    EXPECT_EQ(content, nullptr);
    content = GetNextContent(msg, nullptr);
    EXPECT_EQ(content, nullptr);
    offset = 0;
    content = GetNextContent(msg, &offset);
    EXPECT_NE(content, nullptr);
    content = GetNextContent(msg, &offset);
    EXPECT_NE(content, nullptr);
    free(msg);
}

HWTEST_F(ParamUnitTest, TestServerTaskFail, TestSize.Level0)
{
    ParamTaskPtr serverTask = nullptr;
    ParamStreamInfo info = {};
    info.server = const_cast<char *>(PIPE_NAME);
    info.close = nullptr;
    info.recvMessage = nullptr;
    info.incomingConnect = nullptr;
    int ret = ParamServerCreate(nullptr, &info);
    EXPECT_NE(ret, 0);
    ret = ParamServerCreate(&serverTask, nullptr);
    EXPECT_NE(ret, 0);
    ret = ParamServerCreate(&serverTask, &info);
    EXPECT_NE(ret, 0);
}

HWTEST_F(ParamUnitTest, TestStreamTaskFail, TestSize.Level0)
{
    ParamTaskPtr client = nullptr;
    ParamStreamInfo info = {};
    info.flags = PARAM_TEST_FLAGS;
    info.server = nullptr;
    info.close = OnClose;
    info.recvMessage = ProcessMessage;
    info.incomingConnect = nullptr;

    int ret = ParamStreamCreate(&client, nullptr, &info, sizeof(ParamWatcher));
    EXPECT_NE(ret, 0);
    ret = ParamStreamCreate(&client, GetParamService()->serverTask, nullptr, sizeof(ParamWatcher));
    EXPECT_NE(ret, 0);
    info.close = nullptr;
    ret = ParamStreamCreate(&client, GetParamService()->serverTask, &info, sizeof(ParamWatcher));
    EXPECT_NE(ret, 0);
    info.close = OnClose;
    info.recvMessage = nullptr;
    ret = ParamStreamCreate(&client, GetParamService()->serverTask, &info, sizeof(ParamWatcher));
    EXPECT_NE(ret, 0);
    ret = ParamStreamCreate(&client, nullptr, nullptr, sizeof(ParamWatcher));
    EXPECT_NE(ret, 0);

    void *data = ParamGetTaskUserData(client);
    EXPECT_EQ(data, nullptr);

    ret = ParamTaskSendMsg(nullptr, nullptr);
    EXPECT_NE(ret, 0);
    ret = ParamTaskSendMsg(GetParamService()->serverTask, nullptr);
    EXPECT_NE(ret, 0);
}

HWTEST_F(ParamUnitTest, TestParamCache, TestSize.Level0)
{
    const char *value = CachedParameterGet(nullptr);
    EXPECT_EQ(value, nullptr);
    const char *name = "test.write.1111111.222222";
    CachedHandle cacheHandle = CachedParameterCreate(name, "true");
    EXPECT_NE(cacheHandle, nullptr);
    value = CachedParameterGet(cacheHandle);
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
    int valueChange = 0;
    value = CachedParameterGetChanged(cacheHandle3, &valueChange);
    EXPECT_EQ(strcmp(value, "2222222"), 0);
    EXPECT_EQ(valueChange, 1);
    CachedParameterGetChanged(cacheHandle3, nullptr);
    CachedParameterDestroy(cacheHandle3);
}
#endif
}
