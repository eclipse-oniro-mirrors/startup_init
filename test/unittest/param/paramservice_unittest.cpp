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
#include "le_timer.h"

using namespace testing::ext;
using namespace std;

static int TestTriggerExecute(TriggerNode *trigger, const char *content, uint32_t size)
{
    return 0;
}

extern "C" {
void OnClose(ParamTaskPtr client);
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
static ParamTaskPtr g_worker = nullptr;
class ParamServiceUnitTest : public ::testing::Test {
public:
    ParamServiceUnitTest() {}
    virtual ~ParamServiceUnitTest() {}

    static void SetUpTestCase(void)
    {
        PrepareInitUnitTestEnv();
    }
    void SetUp()
    {
        if (GetParamSecurityLabel() != nullptr) {
            GetParamSecurityLabel()->cred.uid = 1000;  // 1000 test uid
            GetParamSecurityLabel()->cred.gid = 1000;  // 1000 test gid
        }
    }
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
        // 获取到跟属性
        WorkSpace *workspace = GetWorkSpace(WORKSPACE_NAME_DAC);
        (void)FindTrieNode(workspace, name, strlen(name), &labelIndex);
        ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(workspace, labelIndex);
        if (node == nullptr) {
            EXPECT_EQ(1, 0);
            return 0;
        }
        EXPECT_EQ(node->gid, 0);
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

    ParamTaskPtr CreateAndGetStreamTask()
    {
        ParamStreamInfo info = {};
        info.flags = PARAM_TEST_FLAGS;
        info.server = NULL;
        info.close = OnClose;
        info.recvMessage = ProcessMessage;
        info.incomingConnect = NULL;
        ParamTaskPtr client = NULL;
        int ret = ParamStreamCreate(&client, GetParamService()->serverTask, &info, sizeof(ParamWatcher));
        PARAM_CHECK(ret == 0, return NULL, "Failed to create client");

        ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(client);
        PARAM_CHECK(watcher != NULL, return NULL, "Failed to get watcher");
        OH_ListInit(&watcher->triggerHead);
        watcher->stream = client;
        GetParamService()->watcherTask = client;
        return GetParamService()->watcherTask;
    }

    int TestServiceProcessMessage(const char *name, const char *value, int userLabel)
    {
        if (g_worker == nullptr) {
            g_worker = CreateAndGetStreamTask();
        }
        if (g_worker == nullptr) {
            return 0;
        }
        ParamSecurityOps *paramSecurityOps = GetParamSecurityOps(0);
        if (userLabel && paramSecurityOps != NULL) {
            paramSecurityOps->securityFreeLabel = TestFreeLocalSecurityLabel;
            paramSecurityOps->securityCheckParamPermission = TestCheckParamPermission;
        }
        uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + PARAM_ALIGN(strlen(value) + 1);
        ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SET_PARAM, name, msgSize);
        PARAM_CHECK(request != nullptr, return -1, "Failed to malloc for connect");
        do {
            request->type = MSG_SET_PARAM;
            uint32_t offset = 0;
            int ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
            PARAM_CHECK(ret == 0, break, "Failed to fill value");
            ProcessMessage((const ParamTaskPtr)g_worker, (const ParamMessage *)request);
        } while (0);
        free(request);
        RegisterSecurityOps(1);
        return 0;
    }

    int AddWatch(int type, const char *name, const char *value)
    {
        if (g_worker == nullptr) {
            g_worker = CreateAndGetStreamTask();
        }
        if (g_worker == nullptr) {
            return 0;
        }
        uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + PARAM_ALIGN(strlen(value) + 1);
        ParamMessage *request = (ParamMessage *)(ParamMessage *)CreateParamMessage(type, name, msgSize);
        PARAM_CHECK(request != nullptr, return -1, "Failed to malloc for connect");
        do {
            uint32_t offset = 0;
            int ret = FillParamMsgContent(request, &offset, PARAM_VALUE, value, strlen(value));
            PARAM_CHECK(ret == 0, break, "Failed to fill value");
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
        char buffer[] = "testbuff";
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM_WATCH, buffer, strlen(buffer), TestTriggerExecute);
#ifdef PARAM_SUPPORT_TRIGGER
        SystemDumpTriggers(1, NULL);
#endif
        AddWatch(MSG_DEL_WATCHER, name, value);
        return 0;
    }

    int TestCloseTriggerWatch()
    {
        ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(g_worker);
        ClearWatchTrigger(watcher, TRIGGER_PARAM_WAIT);
        ParamTaskClose(g_worker);
        g_worker = nullptr;
        SystemWriteParam("init.svc.param_watcher", "stopped");
        return 0;
    }

    int TestServiceCtrl(const char *serviceName, uint16_t mode)
    {
        // service forbid
        TestSetParamCheckResult("ohos.servicectrl.", mode, 1);
        return SystemWriteParam("ohos.ctl.start", serviceName);
    }

    int TestPowerCtrl(const char *reboot, uint16_t mode)
    {
        // service forbid
        TestSetParamCheckResult("ohos.servicectrl.reboot", mode, 1);
        return SystemWriteParam("ohos.startup.powerctrl", reboot);
    }
};

HWTEST_F(ParamServiceUnitTest, TestServiceProcessMessage, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestServiceProcessMessage("wertt.qqqq.wwww.rrrr", "wwww.eeeee", 1);
    test.TestServiceProcessMessage("wertt.2222.wwww.3333", "wwww.eeeee", 0);
}

HWTEST_F(ParamServiceUnitTest, TestAddParamWait1, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestAddParamWait1();
}

HWTEST_F(ParamServiceUnitTest, TestAddParamWait2, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestAddParamWait2();
}

HWTEST_F(ParamServiceUnitTest, TestAddParamWait3, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestAddParamWait3();
}

HWTEST_F(ParamServiceUnitTest, TestAddParamWatch1, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestAddParamWatch1();
}

HWTEST_F(ParamServiceUnitTest, TestAddParamWatch2, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestAddParamWatch2();
}

HWTEST_F(ParamServiceUnitTest, TestAddParamWatch3, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestAddParamWatch3();
    if (GetParamService()->timer != nullptr) {
        ((TimerTask *)GetParamService()->timer)->processTimer(nullptr, nullptr);
    }
    int hashCode = CheckWatchTriggerTimeout();
    EXPECT_EQ(hashCode, 0);
}

HWTEST_F(ParamServiceUnitTest, TestCloseTriggerWatch, TestSize.Level0)
{
    ParamServiceUnitTest test;
    test.TestCloseTriggerWatch();
}

HWTEST_F(ParamServiceUnitTest, TestServiceCtrl, TestSize.Level0)
{
    ParamServiceUnitTest test;
    int ret = test.TestServiceCtrl("server1", 0770);
    EXPECT_NE(ret, 0);
#ifdef PARAM_SUPPORT_SELINUX
    // selinux forbid
    ret = test.TestServiceCtrl("server2", 0772);
    EXPECT_NE(ret, 0);
#endif
    ret = 0;
}

HWTEST_F(ParamServiceUnitTest, TestPowerCtrl, TestSize.Level0)
{
    ParamServiceUnitTest test;
    int ret = test.TestPowerCtrl("reboot,shutdown", 0770);
    EXPECT_NE(ret, 0);

#ifdef PARAM_SUPPORT_SELINUX
    ret = test.TestPowerCtrl("reboot,shutdown", 0772);
    // selinux forbid
    EXPECT_NE(ret, 0);
#endif
    ret = test.TestPowerCtrl("reboot,updater", 0770);
    EXPECT_NE(ret, 0);
#ifdef PARAM_SUPPORT_SELINUX
    ret = test.TestPowerCtrl("reboot,updater", 0772);
    // selinux forbid
    EXPECT_NE(ret, 0);
#endif
    ret = test.TestPowerCtrl("reboot,flashd", 0770);
    EXPECT_NE(ret, 0);
#ifdef PARAM_SUPPORT_SELINUX
    ret = test.TestPowerCtrl("reboot,flashd", 0772);
    // selinux forbid
    EXPECT_NE(ret, 0);
#endif
    ret = test.TestPowerCtrl("reboot", 0770);
    EXPECT_NE(ret, 0);
#ifdef PARAM_SUPPORT_SELINUX
    ret = test.TestPowerCtrl("reboot", 0772);
    // selinux forbid
    EXPECT_NE(ret, 0);
#endif
}
}  // namespace init_ut
