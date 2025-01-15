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

#include "bootstage.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_param.h"
#include "init_utils.h"
#include "init_hashmap.h"
#include "loop_event.h"
#include "param_manager.h"
#include "param_stub.h"
#include "param_utils.h"
#include "securec.h"
#include "trigger_checker.h"
#include "trigger_manager.h"

using namespace testing::ext;
using namespace std;

static const int triggerBuffer = 512;
static uint32_t g_execCmdId = 0;
static int g_matchTrigger = 0;
static char g_matchTriggerName[triggerBuffer] = { 0 };
static void BootStateChange(int start, const char *content)
{
    UNUSED(content);
    return;
}

static int TestCmdExec(const TriggerNode *trigger, const char *content, uint32_t size)
{
    PARAM_CHECK(trigger != nullptr, return -1, "Invalid trigger");
    PARAM_LOGI("DoTriggerExecute_ trigger type: %d %s", trigger->type, GetTriggerName(trigger));
    PARAM_CHECK(trigger->type <= TRIGGER_UNKNOW, return -1, "Invalid trigger type %d", trigger->type);
    CommandNode *cmd = GetNextCmdNode(reinterpret_cast<const JobNode *>(trigger), nullptr);
    while (cmd != nullptr) {
        g_execCmdId = cmd->cmdKeyIndex;
        cmd = GetNextCmdNode(reinterpret_cast<const JobNode *>(trigger), cmd);
    }
    return 0;
}

static int TestTriggerExecute(TriggerNode *trigger, const char *content, uint32_t size)
{
    JobNode *node = reinterpret_cast<JobNode *>(trigger);
    int ret = memcpy_s(g_matchTriggerName, (int)sizeof(g_matchTriggerName) - 1, node->name, strlen(node->name));
    EXPECT_EQ(ret, 0);
    g_matchTriggerName[strlen(node->name)] = '\0';
    g_matchTrigger++;
    return 0;
}

static void Test_JobParseHook(JOB_PARSE_CTX *jobParseCtx)
{
    return;
}

class TriggerUnitTest : public ::testing::Test {
public:
    TriggerUnitTest() {}
    virtual ~TriggerUnitTest() {}

    void SetUp()
    {
        SetTestPermissionResult(0);
    }
    void TearDown() {}
    void TestBody() {}

    int ParseInitCfg(const char *configFile)
    {
        char *fileBuf = ReadFileToBuf(configFile);
        INIT_ERROR_CHECK(fileBuf != nullptr, return -1, "Failed to read file content %s", configFile);
        cJSON *fileRoot = cJSON_Parse(fileBuf);
        INIT_ERROR_CHECK(fileRoot != nullptr, return -1, "Failed to parse json file %s", configFile);
        ParseTriggerConfig(fileRoot, nullptr, nullptr);
        cJSON_Delete(fileRoot);
        free(fileBuf);
        fileBuf = nullptr;
        return 0;
    }

    int TestLoadTrigger()
    {
        RegisterBootStateChange(BootStateChange);
        InitAddJobParseHook(Test_JobParseHook);

        int cmdKeyIndex = 0;
        const char *matchCmd = GetMatchCmd("setparam aaaa aaaa", &cmdKeyIndex);
        printf("cmd %d \n", matchCmd != nullptr);
        EXPECT_NE(matchCmd, nullptr);

        ReadConfig();
        ParseInitCfg(STARTUP_INIT_UT_PATH "/trigger_test.cfg");
        // trigger
        PostTrigger(EVENT_TRIGGER_BOOT, "pre-init", strlen("pre-init"));
        PostTrigger(EVENT_TRIGGER_BOOT, "init", strlen("init"));
        PostTrigger(EVENT_TRIGGER_BOOT, "post-init", strlen("post-init"));
        LE_DoAsyncEvent(LE_GetDefaultLoop(), GetTriggerWorkSpace()->eventHandle);
        return 0;
    }

    TriggerHeader *GetTriggerHeader(int type)
    {
        return &GetTriggerWorkSpace()->triggerHead[type];
    }

    JobNode *AddTrigger(int type, const char *name, const char *condition, uint32_t size)
    {
        return UpdateJobTrigger(GetTriggerWorkSpace(), type, condition, name);
    }

    int TestAddTriggerForBoot()
    {
        JobNode *node = AddTrigger(TRIGGER_BOOT, "init-later", "", 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), "init-later");
        EXPECT_EQ(node, trigger);
        if (trigger == nullptr) {
            return -1;
        }
        EXPECT_EQ(strcmp(trigger->name, "init-later"), 0);

        // add command
        int cmdIndex = 0;
        GetMatchCmd("reboot ", &cmdIndex);
        int ret = AddCommand(trigger, cmdIndex, nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = AddCommand(trigger, cmdIndex, "update: aaaaaaa", nullptr);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestAddTriggerForParm()
    {
        const char *triggerName = "param:test_param.000";
        int id = 0;
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, "test_param.000=1", 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        GetTriggerHeader(TRIGGER_PARAM)->compareData(reinterpret_cast<struct tagTriggerNode_ *>(trigger), &id);
        EXPECT_EQ(trigger, node);
        if (trigger == nullptr) {
            return -1;
        }
        EXPECT_EQ(strcmp(trigger->name, triggerName), 0);

        // add command
        int cmdIndex = 0;
        GetMatchCmd("reboot ", &cmdIndex);
        int ret = AddCommand(trigger, cmdIndex, nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = AddCommand(trigger, cmdIndex, "update: aaaaaaa", nullptr);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestParamEvent()
    {
        PostParamTrigger(EVENT_TRIGGER_PARAM, "net.tcp.default_init_rwnd", "60");
        const char *sysctrl = "ohos.startup.powerctrl=reboot, shutdown";
        PostTrigger(EVENT_TRIGGER_PARAM, sysctrl, strlen(sysctrl));
        PostParamTrigger(EVENT_TRIGGER_PARAM, "ohos.startup.powerctrl", "reboot, shutdown");

        const char *startCmd = "ohos.ctl.start=hdc -t";
        PostTrigger(EVENT_TRIGGER_PARAM, startCmd, strlen(startCmd));
        PostParamTrigger(EVENT_TRIGGER_PARAM, "ohos.ctl.start", "hdc -t");

        const char *stopCmd = "ohos.ctl.stop=hdc -t";
        PostTrigger(EVENT_TRIGGER_PARAM, stopCmd, strlen(stopCmd));
        PostParamTrigger(EVENT_TRIGGER_PARAM, "ohos.ctl.stop", "hdc -t");
        return 0;
    }

    int TestBootEvent(const char *boot)
    {
        PostTrigger(EVENT_TRIGGER_BOOT, boot, strlen(boot));
        return 0;
    }

    int TestCheckParamTrigger1()
    {
        const char *triggerName = "param:test_param.111";
        const char *param = "test_param.aaa.111.2222";
        const char *value = "1";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, value);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);

        g_matchTrigger = 0;
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, "2");
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(0, g_matchTrigger);

        g_matchTrigger = 0;
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, value);
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));

        // check for bug
        g_matchTrigger = 0;
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", "2222", value);
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(0, g_matchTrigger);

        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM_WATCH, buffer, strlen(buffer), TestTriggerExecute);
        return 0;
    }

    int TestCheckParamTrigger2()
    {
        const char *triggerName = "param:test_param.222";
        const char *param = "test_param.aaa.222.2222";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=*", param);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);

        g_matchTrigger = 0;
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, "2");
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));

        g_matchTrigger = 0;
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, param, strlen(param), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));
        return 0;
    }

    int TestCheckParamTrigger3()
    {
        const char *triggerName = "param:test_param.333";
        const char *param1 = "test_param.aaa.333.2222=1";
        const char *param2 = "test_param.aaa.333.3333=2";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s || %s", param1, param2);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);

        g_matchTrigger = 0;
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, param1, strlen(param1), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));

        g_matchTrigger = 0;
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, param2, strlen(param2), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));
        return 0;
    }

    int TestCheckParamTrigger4()
    {
        const char *triggerName = "param:test_param.444";
        const char *param1 = "test_param.aaa.444.2222";
        const char *param2 = "test_param.aaa.444.3333";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=%s && %s=%s", param1, "1", param2, "2");
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);
        DoJobNow(triggerName);
        ClearTrigger(nullptr, 0);
        g_matchTrigger = 0;
        SystemWriteParam(param1, "1");
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param1, "1");
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(0, g_matchTrigger);

        SystemWriteParam(param2, "2");
        g_matchTrigger = 0;
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param2, "2");
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));
        return 0;
    }

    // test for trigger aaaa:test_param.aaa 被加入unknown执行
    int TestCheckParamTrigger5()
    {
        const char *triggerName = "aaaa:test_param.aaa";
        const char *param1 = "test_param.aaa.aaa.2222";
        const char *param2 = "test_param.aaa.aaa.3333";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "aaaa && %s=%s && %s=%s", param1, "1", param2, "2");
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_UNKNOW, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);

        g_matchTrigger = 0;
        SystemWriteParam(param1, "1");
        ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param1, "1");
        EXPECT_GE(ret, 0);
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_PARAM, buffer, strlen(buffer), TestTriggerExecute);
        EXPECT_EQ(0, g_matchTrigger);

        SystemWriteParam(param2, "2");
        g_matchTrigger = 0;
        CheckTrigger(GetTriggerWorkSpace(), TRIGGER_UNKNOW, "aaaa", strlen("aaaa"), TestTriggerExecute);
        EXPECT_EQ(1, g_matchTrigger);
        EXPECT_EQ(0, strcmp(triggerName, g_matchTriggerName));
        return 0;
    }

    int TestComputeCondition(const char *condition)
    {
        u_int32_t size = strlen(condition) + CONDITION_EXTEND_LEN;
        char *prefix = reinterpret_cast<char *>(malloc(size));
        if (prefix == nullptr) {
            printf("prefix is null.\n");
            return -1;
        }
        ConvertInfixToPrefix(condition, prefix, size);
        printf("prefix %s \n", prefix);
        free(prefix);
        return 0;
    }

    // 普通的属性trigger
    int TestExecuteParamTrigger1()
    {
        const char *triggerName = "aaaa:test_param.eee";
        const char *param = "test_param.eee.aaa.1111";
        const char *value = "eee";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, value);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);

        const uint32_t cmdIndex = 100;
        ret = AddCommand(trigger, cmdIndex, value, nullptr);
        EXPECT_EQ(ret, 0);
        // 修改命令为测试执行
        RegisterTriggerExec(TRIGGER_PARAM, TestCmdExec);
        SystemWriteParam(param, value);
        LE_DoAsyncEvent(LE_GetDefaultLoop(), GetTriggerWorkSpace()->eventHandle);
        EXPECT_EQ(g_execCmdId, cmdIndex);
        return 0;
    }

    int TestExecuteParamTrigger2()
    {
        const char *triggerName = "param:test_param.dddd";
        const char *param = "test_param.dddd.aaa.2222";
        const char *value = "2222";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, value);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);
        const uint32_t cmdIndex = 102;
        ret = AddCommand(trigger, cmdIndex, value, nullptr);
        EXPECT_EQ(ret, 0);
        RegisterTriggerExec(TRIGGER_PARAM, TestCmdExec);
        SystemWriteParam(param, value);
        LE_DoAsyncEvent(LE_GetDefaultLoop(), GetTriggerWorkSpace()->eventHandle);
        EXPECT_EQ(g_execCmdId, cmdIndex);
        return 0;
    }

    // 测试执行后立刻删除
    int TestExecuteParamTrigger3()
    {
        const char *triggerName = "param:test_param.3333";
        const char *param = "test_param.dddd.aaa.3333";
        const char *value = "3333";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, value);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);
        if (trigger == nullptr) {
            return -1;
        }
        const uint32_t cmdIndex = 103;
        ret = AddCommand(trigger, cmdIndex, value, nullptr);
        EXPECT_EQ(ret, 0);
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_ONCE);
        SystemWriteParam(param, value);

        RegisterTriggerExec(TRIGGER_PARAM, TestCmdExec);
        LE_DoAsyncEvent(LE_GetDefaultLoop(), GetTriggerWorkSpace()->eventHandle);
        EXPECT_EQ(g_execCmdId, cmdIndex);
        trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        if (trigger != nullptr) {
            EXPECT_EQ(1, 0);
        }
        return 0;
    }

    // 测试删除队列中的trigger
    int TestExecuteParamTrigger4()
    {
        const char *triggerName = "param:test_param.4444";
        const char *param = "test_param.dddd.aaa.4444";
        const char *value = "4444";
        char buffer[triggerBuffer];
        int ret = sprintf_s(buffer, sizeof(buffer), "%s=%s", param, value);
        EXPECT_GE(ret, 0);
        JobNode *node = AddTrigger(TRIGGER_PARAM, triggerName, buffer, 0);
        JobNode *trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        EXPECT_EQ(trigger, node);
        if (trigger == nullptr) {
            return -1;
        }
        const uint32_t cmdIndex = 105;
        ret = AddCommand(trigger, cmdIndex, value, nullptr);
        EXPECT_EQ(ret, 0);
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_ONCE);
        SystemWriteParam(param, value);

        RegisterTriggerExec(TRIGGER_PARAM, TestCmdExec);
        FreeTrigger(GetTriggerWorkSpace(), reinterpret_cast<TriggerNode *>(trigger));
        LE_DoAsyncEvent(LE_GetDefaultLoop(), GetTriggerWorkSpace()->eventHandle);
        EXPECT_NE(g_execCmdId, cmdIndex);
        trigger = GetTriggerByName(GetTriggerWorkSpace(), triggerName);
        if (trigger != nullptr) {
            EXPECT_EQ(1, 0);
        }
        return 0;
    }

    // 测试执行后检查子trigger执行
    int TestExecuteParamTrigger5()
    {
        const char *boot = "boot2";
        const char *triggerName = "boot2:test_param.5555";
        const char *param = "test_param.dddd.aaa.5555";
        const char *value = "5555";
        JobNode *trigger = AddTrigger(TRIGGER_BOOT, boot, nullptr, 0);
        const int testCmdIndex = 1105;
        int ret = AddCommand(trigger, testCmdIndex, value, nullptr);
        EXPECT_EQ(ret, 0);
        if (trigger == nullptr) {
            return -1;
        }
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_SUBTRIGGER);

        char buffer[triggerBuffer];
        ret = sprintf_s(buffer, sizeof(buffer), "boot2 && %s=%s", param, value);
        EXPECT_GE(ret, 0);
        trigger = AddTrigger(TRIGGER_UNKNOW, triggerName, buffer, 0);
        const int testCmdIndex2 = 105;
        ret = AddCommand(trigger, testCmdIndex2, value, nullptr);

        RegisterTriggerExec(TRIGGER_UNKNOW, TestCmdExec);
        SystemWriteParam(param, value);

        TestBootEvent(boot);
        LE_DoAsyncEvent(LE_GetDefaultLoop(), GetTriggerWorkSpace()->eventHandle);
        EXPECT_EQ(g_execCmdId, (uint32_t)testCmdIndex2);
        return 0;
    }

    int TestDumpTrigger()
    {
        (void)AddCompleteJob("param:ohos.servicectrl.display", "ohos.servicectrl.display=*", "display system");
        DoTriggerExec("param:ohos.servicectrl.display");
        return 0;
    }
};

HWTEST_F(TriggerUnitTest, Init_TestLoadTrigger_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestLoadTrigger();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestBootEvent_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestBootEvent("pre-init");
    EXPECT_EQ(ret, 0);
    ret = test.TestBootEvent("init");
    EXPECT_EQ(ret, 0);
    ret = test.TestBootEvent("post-init");
    EXPECT_EQ(ret, 0);
    ret = test.TestBootEvent("early-init");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestAddTriggerForBoot_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestAddTriggerForBoot();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestAddTriggerForParm_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestAddTriggerForParm();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestCheckParamTrigger_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestCheckParamTrigger1();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestCheckParamTrigger_002, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestCheckParamTrigger2();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestCheckParamTrigger_003, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestCheckParamTrigger3();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestCheckParamTrigger_004, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestCheckParamTrigger4();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestCheckParamTrigger_005, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestCheckParamTrigger5();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestParamEvent_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestParamEvent();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestComputerCondition_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestComputeCondition("aaa=111||aaa=222||aaa=333");
    EXPECT_EQ(ret, 0);
    ret = test.TestComputeCondition("aaa=111||aaa=222&&aaa=333");
    EXPECT_EQ(ret, 0);
    ret = test.TestComputeCondition("(aaa=111||aaa=222)&&aaa=333");
    EXPECT_EQ(ret, 0);
    ret = test.TestComputeCondition("aaa=111||(aaa=222&&aaa=333)");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestExecuteParamTrigger_001, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestExecuteParamTrigger1();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestExecuteParamTrigger_002, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestExecuteParamTrigger2();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestExecuteParamTrigger_003, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestExecuteParamTrigger3();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestExecuteParamTrigger_004, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestExecuteParamTrigger4();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestExecuteParamTrigger_005, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestExecuteParamTrigger5();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TriggerUnitTest, Init_TestExecuteParamTrigger_006, TestSize.Level0)
{
    TriggerUnitTest test;
    int ret = test.TestDumpTrigger();
    EXPECT_EQ(ret, 0);
}
