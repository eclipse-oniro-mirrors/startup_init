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

#include <memory>

#include "init_param.h"
#include "init_unittest.h"
#include "param_stub.h"
#include "trigger_manager.h"

using namespace testing::ext;
using namespace std;
static int CheckServerParamValue(const char *name, const char *expectValue)
{
    char tmp[PARAM_BUFFER_SIZE] = { 0 };
    u_int32_t len = sizeof(tmp);
    SystemReadParam(name, tmp, &len);
    printf("CheckParamValue name %s value: \'%s\' expectValue:\'%s\' \n", name, tmp, expectValue);
    EXPECT_NE((int)strlen(tmp), 0);
    if (expectValue != nullptr) {
        EXPECT_EQ(strcmp(tmp, expectValue), 0);
    }
    return 0;
}

extern "C" {
extern void TimerCallbackForSave(ParamTaskPtr timer, void *context);
}

static ParamTask *g_worker = nullptr;
class ParamUnitTest : public ::testing::Test {
public:
    ParamUnitTest() {}
    virtual ~ParamUnitTest() {}

    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

    int TestParamServiceInit()
    {
        InitParamService();
        // parse parameters
        LoadDefaultParams(PARAM_DEFAULT_PATH"/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
        LoadDefaultParams(PARAM_DEFAULT_PATH"/vendor/etc/param", LOAD_PARAM_NORMAL);
        LoadDefaultParams(PARAM_DEFAULT_PATH"/system/etc/param", LOAD_PARAM_ONLY_ADD);
        CheckServerParamValue("const.actionable_compatible_property.enabled", "false");
        CheckServerParamValue("build_version", "2.0");
        CheckServerParamValue("ohos.boot.hardware", "Hi3516DV300");
        return 0;
    }

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
        GetParamWorkSpace()->securityLabel->cred.gid = 9999; // 9999 test gid
        const char *name = "label1.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        uint32_t labelIndex = 0;
        SystemWriteParam(name, value);
        // 获取到跟属性
        ParamTrieNode *paramNode = FindTrieNode(&GetParamWorkSpace()->paramSpace, name, strlen(name), &labelIndex);
        ParamSecruityNode *node = (ParamSecruityNode *)GetTrieNode(&GetParamWorkSpace()->paramSpace, labelIndex);
        if (paramNode == nullptr || node == nullptr) {
            EXPECT_EQ(1, 0);
            return 0;
        }
        EXPECT_EQ(node->gid, getegid());
        return 0;
    }

    // 添加一个label，最长匹配到这个节点
    int TestAddSecurityLabel2()
    {
        GetParamWorkSpace()->securityLabel->cred.gid =  9999; // 9999 test gid
        const char *name = "label2.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        ParamAuditData auditData = {};
        auditData.name = "label2.test.aaa";
        auditData.label = "label2.test.aaa";
        auditData.dacData.gid = 202; // 202 test dac gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666; // 0666 test mode
        SystemWriteParam(name, value);
        uint32_t labelIndex = 0;
        AddSecurityLabel(&auditData, GetParamWorkSpace());
        ParamTrieNode *paramNode = FindTrieNode(&GetParamWorkSpace()->paramSpace, name, strlen(name), &labelIndex);
        ParamSecruityNode *node = (ParamSecruityNode *)GetTrieNode(&GetParamWorkSpace()->paramSpace, labelIndex);
        if (paramNode == nullptr || node == nullptr) {
            EXPECT_EQ(1, 0);
        }
        EXPECT_EQ(node->gid, auditData.dacData.gid);
        return 0;
    }

    // 添加一个label，最长匹配到最后一个相同节点
    int TestAddSecurityLabel3()
    {
        GetParamWorkSpace()->securityLabel->cred.gid =  9999; // 9999 test gid
        const char *name = "label3.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        ParamAuditData auditData = {};
        auditData.name = "label3.test.aaa";
        auditData.label = "label3.test.aaa";
        auditData.dacData.gid = 203; // 203 test gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666; // 0666 test mode
        SystemWriteParam(name, value);
        AddSecurityLabel(&auditData, GetParamWorkSpace());

        auditData.name = "label3.test.aaa.bbb.ccc.dddd.eee.dddd";
        auditData.label = "label3.test.aaa.bbb.ccc.dddd.eee.dddd";
        auditData.dacData.gid = 202; // 202 test dac gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666; // 0666 test mode
        SystemWriteParam(name, value);
        AddSecurityLabel(&auditData, GetParamWorkSpace());

        uint32_t labelIndex = 0;
        ParamTrieNode *paramNode = FindTrieNode(&GetParamWorkSpace()->paramSpace, name, strlen(name), &labelIndex);
        ParamSecruityNode *node = (ParamSecruityNode *)GetTrieNode(&GetParamWorkSpace()->paramSpace, labelIndex);
        if (paramNode == nullptr || node == nullptr) {
            EXPECT_EQ(1, 0);
        }
        EXPECT_EQ((int)node->gid, 203); // 203 test gid
        return 0;
    }

    // 添加一个label，完全匹配
    int TestAddSecurityLabel4()
    {
        GetParamWorkSpace()->securityLabel->cred.gid =  9999; // 9999 test gid
        const char *name = "label4.test.aaa.bbb.ccc.dddd.eee";
        const char *value = "2001";
        ParamAuditData auditData = {};
        auditData.name = "label4.test.aaa.bbb.ccc.dddd.eee";
        auditData.label = "label4.test.aaa.bbb.ccc.dddd.eee";
        auditData.dacData.gid = 203; // 203 test gid
        auditData.dacData.uid = geteuid();
        auditData.dacData.mode = 0666; // 0666 test mode
        SystemWriteParam(name, value);
        uint32_t labelIndex = 0;
        AddSecurityLabel(&auditData, GetParamWorkSpace());
        ParamTrieNode *paramNode = FindTrieNode(&GetParamWorkSpace()->paramSpace, name, strlen(name), &labelIndex);
        ParamSecruityNode *node = (ParamSecruityNode *)GetTrieNode(&GetParamWorkSpace()->paramSpace, labelIndex);
        if (paramNode == nullptr || node == nullptr) {
            EXPECT_EQ(1, 0);
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
        char value[PARAM_BUFFER_SIZE + PARAM_BUFFER_SIZE] = { 0 };
        TraversalParam (GetParamWorkSpace(),
            [](ParamHandle handle, void *cookie) {
                ReadParamName(GetParamWorkSpace(), handle, (char *)cookie, PARAM_BUFFER_SIZE);
                u_int32_t len = PARAM_BUFFER_SIZE;
                ReadParamValue(GetParamWorkSpace(), handle, ((char *)cookie) + PARAM_BUFFER_SIZE, &len);
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
        if (GetParamWorkSpace() != nullptr) {
            TimerCallbackForSave(nullptr, &(GetParamWorkSpace()->paramSpace));
        }
        LoadPersistParams();
        return 0;
    }

    int FillLabelContent(ParamSecurityOps *paramSecurityOps, ParamMessage *request, uint32_t *start, uint32_t length)
    {
        if (length == 0) {
            return 0;
        }
        uint32_t bufferSize = request->msgSize - sizeof(ParamMessage);
        uint32_t offset = *start;
        PARAM_CHECK((offset + sizeof(ParamMsgContent) + length) <= bufferSize,
            return -1, "Invalid msgSize %u offset %u length %u", request->msgSize, offset, bufferSize);
        ParamMsgContent *content = (ParamMsgContent *)(request->data + offset);
        content->type = PARAM_LABEL;
        content->contentSize = 0;
        if (length != 0 && paramSecurityOps->securityEncodeLabel != nullptr) {
            int ret = paramSecurityOps->securityEncodeLabel(
                GetParamWorkSpace()->securityLabel, content->content, &length);
            PARAM_CHECK(ret == 0, return -1, "Failed to get label length");
            content->contentSize = length;
        }
        offset += sizeof(ParamMsgContent) + PARAM_ALIGN(content->contentSize);
        *start = offset;
        return 0;
    }

    ParamTask *CreateAndGetStreamTask()
    {
        LibuvServerTask *server = (LibuvServerTask *)GetParamWorkSpace()->serverTask;
        if (server == nullptr) {
            EXPECT_NE(0, 0);
            return nullptr;
        }

        // 创建stream task
        server->incomingConnect((ParamTaskPtr)server, 0);
        ParamWatcher *watcher = GetNextParamWatcher(GetTriggerWorkSpace(), nullptr);
        return watcher != nullptr ? watcher->stream : nullptr;
    }

    int TestServiceProcessMessage(const char *name, const char *value, int userLabel)
    {
        if (g_worker == nullptr) {
            g_worker = CreateAndGetStreamTask();
        }
        uint32_t labelLen = 0;
        ParamSecurityOps *paramSecurityOps = &GetParamWorkSpace()->paramSecurityOps;
        if (userLabel) {
            paramSecurityOps->securityEncodeLabel = TestEncodeSecurityLabel;
            paramSecurityOps->securityDecodeLabel = TestDecodeSecurityLabel;
            paramSecurityOps->securityFreeLabel = TestFreeLocalSecurityLabel;
            paramSecurityOps->securityCheckParamPermission = TestCheckParamPermission;
        }
        uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + PARAM_ALIGN(strlen(value) + 1);
        if (paramSecurityOps->securityEncodeLabel != nullptr) {
            int ret = paramSecurityOps->securityEncodeLabel(GetParamWorkSpace()->securityLabel, nullptr, &labelLen);
            PARAM_CHECK(ret == 0, return -1, "Failed to get label length");
            msgSize += sizeof(ParamMsgContent) + PARAM_ALIGN(labelLen);
        }
        ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SET_PARAM, name, msgSize);
        PARAM_CHECK(request != nullptr, return -1, "Failed to malloc for connect");
        do {
            request->type = MSG_SET_PARAM;
            uint32_t offset = 0;
            int ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
            PARAM_CHECK(ret == 0, break, "Failed to fill value");
            ret = FillLabelContent(paramSecurityOps, request, &offset, labelLen);
            PARAM_CHECK(ret == 0, break, "Failed to fill label");
            ProcessMessage((const ParamTaskPtr)g_worker, (const ParamMessage *)request);
        } while (0);
        free(request);
        CheckServerParamValue(name, value);
        RegisterSecurityOps(paramSecurityOps, 1);
        return 0;
    }

    int AddWatch(int type, const char *name, const char *value)
    {
        if (g_worker == nullptr) {
            g_worker = CreateAndGetStreamTask();
        }
        uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + PARAM_ALIGN(strlen(value) + 1);
        ParamMessage *request = (ParamMessage *)(ParamMessage *)CreateParamMessage(type, name, msgSize);
        PARAM_CHECK(request != nullptr, return -1, "Failed to malloc for connect");
        do {
            uint32_t offset = 0;
            int ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
            PARAM_CHECK(ret == 0, return -1, "Failed to fill value");
            ProcessMessage((const ParamTaskPtr)g_worker, (const ParamMessage *)request);
        } while (0);
        free(request);
        return 0;
    }

    // 无匹配
    int TestAddParamWait1()
    {
        const char *name = "wait.aaa.bbb.ccc.111";
        const char *value = "wait1";
        AddWatch(MSG_WAIT_PARAM, name, value);
        SystemWriteParam(name, value);
        return 0;
    }

    // 模糊匹配
    int TestAddParamWait2()
    {
        const char *name = "wait.aaa.bbb.ccc.222";
        const char *value = "wait2";
        AddWatch(MSG_WAIT_PARAM, name, "*");
        SystemWriteParam(name, value);
        return 0;
    }

    // 属性存在
    int TestAddParamWait3()
    {
        const char *name = "wait.aaa.bbb.ccc.333";
        const char *value = "wait3";
        SystemWriteParam(name, value);
        AddWatch(MSG_WAIT_PARAM, name, value);
        return 0;
    }

    int TestAddParamWatch1()
    {
        const char *name = "watch.aaa.bbb.ccc.111";
        const char *value = "watch1";
        AddWatch(MSG_ADD_WATCHER, name, value);
        std::string newName = name;
        newName += ".test.test.test";
        SystemWriteParam(newName.c_str(), value);
        return 0;
    }

    int TestAddParamWatch2()
    {
        const char *name = "watch.aaa.bbb.ccc.222";
        const char *value = "watch2";
        AddWatch(MSG_ADD_WATCHER, name, "*");
        SystemWriteParam(name, value);
        return 0;
    }

    int TestAddParamWatch3()
    {
        const char *name = "watch.aaa.bbb.ccc.333";
        const char *value = "watch3";
        std::string newName = name;
        newName += ".test.test.test";
        SystemWriteParam(newName.c_str(), value);
        AddWatch(MSG_ADD_WATCHER, name, value);
        return 0;
    }

    int TestCloseTriggerWatch()
    {
        ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(g_worker);
        ClearWatcherTrigger(watcher);
        ParamTaskClose(g_worker);
        g_worker = nullptr;
        SystemWriteParam("init.svc.param_watcher", "stopped");
        return 0;
    }

    int TestDumpParamMemory()
    {
        DumpParametersAndTriggers();
        return 0;
    }

    int TestServiceCtrl(const char *serviceName, int mode)
    {
        // service forbit
        ParamAuditData auditData = {};
        auditData.name = "ohos.servicectrl.";
        auditData.label = "";
        auditData.dacData.gid = 202; // 202 test dac gid
        auditData.dacData.uid = 202; // 202 test dac uid
        auditData.dacData.mode = mode;
        AddSecurityLabel(&auditData, GetParamWorkSpace());
        return SystemWriteParam("ohos.ctl.start", serviceName);
    }

    int TestPowerCtrl(const char *reboot, int mode)
    {
        // service forbit
        ParamAuditData auditData = {};
        auditData.name = "ohos.servicectrl.";
        auditData.label = "";
        auditData.dacData.gid = 202; // 202 test dac gid
        auditData.dacData.uid = 202; // 202 test dac uid
        auditData.dacData.mode = mode;
        AddSecurityLabel(&auditData, GetParamWorkSpace());
        return SystemWriteParam("sys.powerctrl", reboot);
    }
};

HWTEST_F(ParamUnitTest, TestParamServiceInit, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestParamServiceInit();
}

HWTEST_F(ParamUnitTest, TestPersistParam, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestPersistParam();
}

HWTEST_F(ParamUnitTest, TestSetParam_1, TestSize.Level0)
{
    ParamUnitTest test;
    const char *params[][2] = { { "111.2222", "1" },
                                { "111.2222.3333", "2" },
                                { "111.2222.3333.4444", "3" },
                                { "111.2222.3333.4444.666", "4" },
                                { "111.2222.3333.4444.666.777", "5" } };
    test.TestSetParams(params, 5);
}

HWTEST_F(ParamUnitTest, TestSetParam_2, TestSize.Level0)
{
    ParamUnitTest test;
    const char *params[][2] = { { "111.2222.xxxx.xxx.xxx", "1_1" },
                                { "111.2222.3333.xxx", "1_2" },
                                { "111.2222.xxxx.3333.4444", "1_3" },
                                { "111.2222.3333.xxxx.4444.666", "1_4" },
                                { "111.2222.3333.4444.666.xxxxx.777", "1_5" } };
    test.TestSetParams(params, 5);

    const char *ctrlParams[][2] = { { "ctl.start.111.2222.xxxx.xxx.xxx", "2_1" },
                                    { "ctl.start.111.2222.3333.xxx", "2_2" },
                                    { "ctl.start.111.2222.xxxx.3333.4444", "2_3" },
                                    { "ctl.start.111.2222.3333.xxxx.4444.666", "2_4" },
                                    { "ctl.start.111.2222.3333.4444.666.xxxxx.777", "2_5" } };
    test.TestSetParams(ctrlParams, 5);

    const char *sysParams[][2] = { { "sys.powerctrl.111.2222.xxxx.xxx.xxx", "3_1" },
                                   { "sys.powerctrl.111.2222.3333.xxx", "3_2" },
                                   { "sys.powerctrl.111.2222.xxxx.3333.4444", "3_3" },
                                   { "sys.powerctrl.111.2222.3333.xxxx.4444.666", "3_4" },
                                   { "sys.powerctrl.111.2222.3333.4444.666.xxxxx.777", "3_5" } };
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

HWTEST_F(ParamUnitTest, TestServiceProcessMessage, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestServiceProcessMessage("wertt.qqqq.wwww.rrrr", "wwww.eeeee", 1);
    test.TestServiceProcessMessage("wertt.2222.wwww.3333", "wwww.eeeee", 0);
}

HWTEST_F(ParamUnitTest, TestAddParamWait1, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddParamWait1();
}

HWTEST_F(ParamUnitTest, TestAddParamWait2, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddParamWait2();
}

HWTEST_F(ParamUnitTest, TestAddParamWait3, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddParamWait3();
}

HWTEST_F(ParamUnitTest, TestAddParamWatch1, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddParamWatch1();
}

HWTEST_F(ParamUnitTest, TestAddParamWatch2, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddParamWatch2();
}

HWTEST_F(ParamUnitTest, TestAddParamWatch3, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestAddParamWatch3();
}

HWTEST_F(ParamUnitTest, TestCloseTriggerWatch, TestSize.Level0)
{
    ParamUnitTest test;
    test.TestCloseTriggerWatch();
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

HWTEST_F(ParamUnitTest, TestServiceCtrl, TestSize.Level0)
{
    ParamUnitTest test;
    int ret = test.TestServiceCtrl("server1", 0770);
    EXPECT_NE(ret, 0);
    ret = test.TestServiceCtrl("server2", 0772);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(ParamUnitTest, TestPowerCtrl, TestSize.Level0)
{
    ParamUnitTest test;
    int ret = test.TestPowerCtrl("reboot,shutdown", 0770);
    EXPECT_NE(ret, 0);
    ret = test.TestPowerCtrl("reboot,shutdown", 0772);
    EXPECT_EQ(ret, 0);
    ret = test.TestPowerCtrl("reboot,updater", 0770);
    EXPECT_NE(ret, 0);
    ret = test.TestPowerCtrl("reboot,updater", 0772);
    EXPECT_EQ(ret, 0);
    ret = test.TestPowerCtrl("reboot,flash", 0770);
    EXPECT_NE(ret, 0);
    ret = test.TestPowerCtrl("reboot,flash", 0772);
    EXPECT_EQ(ret, 0);
    ret = test.TestPowerCtrl("reboot", 0770);
    EXPECT_NE(ret, 0);
    ret = test.TestPowerCtrl("reboot", 0772);
    EXPECT_EQ(ret, 0);
}