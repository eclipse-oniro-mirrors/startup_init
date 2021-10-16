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
#include <unistd.h>
#include <time.h>
#include "init_cmds.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "param_manager.h"
#include "param_utils.h"
#include "trigger_checker.h"
#include "trigger_manager.h"

#define MAX_TRIGGER_COUNT_RUN_ONCE 20
static TriggerWorkSpace g_triggerWorkSpace = {};

#ifdef PARAM_TEST
#define MAX_COUNT 1000
#define READ_DURATION 100000
#define TEST_PARAM_NAME "test.random.read.001"
static void AddTestTrigger(const TriggerWorkSpace *workSpace)
{
    TriggerHeader *header = (TriggerHeader *)&workSpace->triggerHead[TRIGGER_PARAM];
    TriggerNode *trigger = AddTrigger(header, "param:test.randrom.test.start=1", "test.randrom.test.start=1", 0);
    PARAM_CHECK(trigger != NULL, return, "Failed to create test trigger");
    int cmdKeyIndex = 0;
    GetMatchCmd("setparam ", &cmdKeyIndex);
    int ret = AddCommand(trigger, (uint32_t)CMD_INDEX_FOR_PARA_TEST, NULL);
    PARAM_CHECK(ret == 0, return, "Failed to add command");
}

static void TestTimerCallback(ParamTaskPtr timer, void *context)
{
    (void)srand((unsigned)time(NULL));
    char buffer[32] = { 0 };
    static int index = 0;
    index++;
    if (index >= MAX_COUNT) {
        ParamTaskClose(timer);
    }
    static uint32_t value = 0;
    int count = 0;
    while (count < MAX_COUNT) {
        int wait = rand() / READ_DURATION + READ_DURATION; // 100ms
        (void)sprintf_s(buffer, sizeof(buffer), "%u", value);
        PARAM_LOGI("set param name: %s, value %s", TEST_PARAM_NAME, buffer);
        SystemWriteParam(TEST_PARAM_NAME, buffer);
        usleep(wait);
        count++;
        value++;
    }
}
#endif

void DoTriggerExec(const char *triggerName)
{
    PARAM_CHECK(triggerName != NULL, return, "Invalid param");
    TriggerNode *trigger = GetTriggerByName(&g_triggerWorkSpace, triggerName);
    if (trigger != NULL && !TRIGGER_IN_QUEUE(trigger)) { // 不在队列中
        PARAM_LOGI("DoTriggerExec trigger %s", trigger->name);
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_QUEUE);
        ExecuteQueuePush(&g_triggerWorkSpace, trigger);
    }
}

static void DoCmdExec(const TriggerNode *trigger, const CommandNode *cmd, const char *content, uint32_t size)
{
    if (cmd->cmdKeyIndex == CMD_INDEX_FOR_PARA_WAIT || cmd->cmdKeyIndex == CMD_INDEX_FOR_PARA_WATCH) {
        TriggerExtData *extData = TRIGGER_GET_EXT_DATA(trigger, TriggerExtData);
        if (extData != NULL && extData->excuteCmd != NULL) {
            extData->excuteCmd(extData, cmd->cmdKeyIndex, content);
        }
        return;
#ifdef PARAM_TEST
    } else if (cmd->cmdKeyIndex == CMD_INDEX_FOR_PARA_TEST) {
        ParamTaskPtr timer = NULL;
        ParamTimerCreate(&timer, TestTimerCallback, NULL);
        ParamTimerStart(timer, READ_DURATION, MAX_COUNT);
        return;
#endif
    }
#ifndef STARTUP_INIT_TEST
    DoCmdByIndex(cmd->cmdKeyIndex, cmd->content);
#endif
}

static int ExecuteTrigger(const TriggerWorkSpace *workSpace, const TriggerNode *trigger)
{
    PARAM_CHECK(workSpace != NULL && trigger != NULL, return -1, "Invalid trigger");
    PARAM_CHECK(workSpace->cmdExec != NULL, return -1, "Invalid cmdExec");
    PARAM_LOGI("ExecuteTrigger trigger %s", trigger->name);
    CommandNode *cmd = GetNextCmdNode(trigger, NULL);
    while (cmd != NULL) {
        workSpace->cmdExec(trigger, cmd, NULL, 0);
        cmd = GetNextCmdNode(trigger, cmd);
    }
    return 0;
}

static int DoTiggerCheckResult(TriggerNode *trigger, const char *content, uint32_t size)
{
    UNUSED(content);
    UNUSED(size);
    if (TRIGGER_IN_QUEUE(trigger)) {
        PARAM_LOGI("DoTiggerExecute trigger %s has been waiting execute", trigger->name);
        return 0;
    }
    TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_QUEUE);
    PARAM_LOGI("Add trigger %s to execute queue", trigger->name);
    ExecuteQueuePush(&g_triggerWorkSpace, trigger);
    return 0;
}

static int ExecuteTiggerImmediately(TriggerNode *trigger, const char *content, uint32_t size)
{
    PARAM_CHECK(trigger != NULL, return -1, "Invalid trigger");
    PARAM_CHECK(g_triggerWorkSpace.cmdExec != NULL, return -1, "Invalid cmdExec");
    PARAM_LOGI("ExecuteTiggerImmediately trigger %s", trigger->name);
    CommandNode *cmd = GetNextCmdNode(trigger, NULL);
    while (cmd != NULL) {
        g_triggerWorkSpace.cmdExec(trigger, cmd, content, size);
        cmd = GetNextCmdNode(trigger, cmd);
    }
    if (TRIGGER_TEST_FLAG(trigger, TRIGGER_FLAGS_ONCE)) {
        FreeTrigger(trigger);
    }
    return 0;
}

static void ExecuteQueueWork(uint32_t maxCount)
{
    uint32_t executeCount = 0;
    TriggerNode *trigger = ExecuteQueuePop(&g_triggerWorkSpace);
    while (trigger != NULL) {
        ExecuteTrigger(&g_triggerWorkSpace, trigger);
        TRIGGER_CLEAR_FLAG(trigger, TRIGGER_FLAGS_QUEUE);
        if (TRIGGER_TEST_FLAG(trigger, TRIGGER_FLAGS_SUBTRIGGER)) { // 检查boot:xxx=xxx 的trigger
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_UNKNOW,
                trigger->name, strlen(trigger->name), ExecuteTiggerImmediately);
        }
        if (TRIGGER_TEST_FLAG(trigger, TRIGGER_FLAGS_ONCE)) {
            FreeTrigger(trigger);
        }
        executeCount++;
        if (executeCount > maxCount) {
            break;
        }
        trigger = ExecuteQueuePop(&g_triggerWorkSpace);
    }
}

static void ProcessParamEvent(uint64_t eventId, const char *content, uint32_t size)
{
    UNUSED(eventId);
    UNUSED(content);
    UNUSED(size);
    ExecuteQueueWork(MAX_TRIGGER_COUNT_RUN_ONCE);
}

static void ProcessBeforeEvent(uint64_t eventId, const char *content, uint32_t size)
{
    switch (eventId) {
        case EVENT_TRIGGER_PARAM: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_PARAM, content, size, DoTiggerCheckResult);
            break;
        }
        case EVENT_TRIGGER_BOOT: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_BOOT, content, size, DoTiggerCheckResult);
            break;
        }
        case EVENT_TRIGGER_PARAM_WAIT: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_PARAM_WAIT, content, size, ExecuteTiggerImmediately);
            break;
        }
        case EVENT_TRIGGER_PARAM_WATCH: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_PARAM_WATCH, content, size, ExecuteTiggerImmediately);
            break;
        }
        default:
            break;
    }
}

static const char *GetCmdInfo(const char *content, uint32_t contentSize, char **cmdParam)
{
    static const char *ctrlCmds[][2] = {
        {"reboot", "reboot "}
    };
    int index = 0;
    for (size_t i = 0; i < sizeof(ctrlCmds) / sizeof(ctrlCmds[0]); i++) {
        if (strncmp(content, ctrlCmds[i][0], strlen(ctrlCmds[i][0])) == 0) {
            *cmdParam = (char *)content;
            return GetMatchCmd(ctrlCmds[i][1], &index);
        }
    }
    return NULL;
}

static void SendTriggerEvent(int type, const char *content, uint32_t contentLen)
{
    PARAM_CHECK(content != NULL, return, "Invalid param");
    PARAM_LOGD("SendTriggerEvent type %d content %s", type, content);
    if (type == EVENT_TRIGGER_PARAM) {
        int ctrlSize = strlen(SYS_POWER_CTRL);
        if (strncmp(content, SYS_POWER_CTRL, ctrlSize) == 0) {
            char *cmdParam = NULL;
            const char *matchCmd = GetCmdInfo(content + ctrlSize, contentLen - ctrlSize, &cmdParam);
            PARAM_LOGD("SendTriggerEvent matchCmd %s", matchCmd);
            if (matchCmd != NULL) {
#ifndef STARTUP_INIT_TEST
                DoCmdByName(matchCmd, cmdParam);
#endif
            } else {
                PARAM_LOGE("SendTriggerEvent cmd %s not found", content);
            }
        } else if (strncmp(content, OHOS_CTRL_START, strlen(OHOS_CTRL_START)) == 0) {
            StartServiceByName(content + strlen(OHOS_CTRL_START), false);
        } else if (strncmp(content, OHOS_CTRL_STOP, strlen(OHOS_CTRL_STOP)) == 0) {
            StopServiceByName(content + strlen(OHOS_CTRL_STOP));
        } else {
            ParamEventSend(g_triggerWorkSpace.eventHandle, (uint64_t)type, content, contentLen);
        }
    } else {
        ParamEventSend(g_triggerWorkSpace.eventHandle, (uint64_t)type, content, contentLen);
    }
}

void PostParamTrigger(int type, const char *name, const char *value)
{
    PARAM_CHECK(name != NULL && value != NULL, return, "Invalid param");
    uint32_t bufferSize = strlen(name) + strlen(value) + 1 + 1 + 1;
    PARAM_CHECK(bufferSize < (PARAM_CONST_VALUE_LEN_MAX + PARAM_NAME_LEN_MAX + 1 + 1 + 1),
        return, "bufferSize is longest %d", bufferSize);
    char *buffer = (char *)malloc(bufferSize);
    PARAM_CHECK(buffer != NULL, return, "Failed to alloc memory for  param %s", name);
    int ret = sprintf_s(buffer, bufferSize - 1, "%s=%s", name, value);
    PARAM_CHECK(ret > EOK, free(buffer);
        return, "Failed to copy param");
    SendTriggerEvent(type, buffer, strlen(buffer));
    free(buffer);
}

void PostTrigger(EventType type, const char *content, uint32_t contentLen)
{
    PARAM_CHECK(content != NULL && contentLen > 0, return, "Invalid param");
    SendTriggerEvent(type, content, contentLen);
}

static int GetTriggerType(const char *type)
{
    if (strncmp("param:", type, strlen("param:")) == 0) {
        return TRIGGER_PARAM;
    }
    const char *triggerTypeStr[] = {
        "pre-init", "boot", "early-init", "init", "early-init", "late-init", "post-init",
        "early-fs", "post-fs", "late-fs", "post-fs-data"
    };
    for (size_t i = 0; i < ARRAY_LENGTH(triggerTypeStr); i++) {
        if (strcmp(triggerTypeStr[i], type) == 0) {
            return TRIGGER_BOOT;
        }
    }
    return TRIGGER_UNKNOW;
}

static int ParseTrigger_(const TriggerWorkSpace *workSpace, const cJSON *triggerItem)
{
    PARAM_CHECK(triggerItem != NULL, return -1, "Invalid file");
    PARAM_CHECK(workSpace != NULL, return -1, "Failed to create trigger list");
    char *name = cJSON_GetStringValue(cJSON_GetObjectItem(triggerItem, "name"));
    PARAM_CHECK(name != NULL, return -1, "Can not get name from cfg");
    char *condition = cJSON_GetStringValue(cJSON_GetObjectItem(triggerItem, "condition"));
    int type = GetTriggerType(name);
    PARAM_CHECK(type < TRIGGER_MAX, return -1, "Failed to get trigger index");
    TriggerHeader *header = (TriggerHeader *)&workSpace->triggerHead[type];
    TriggerNode *trigger = GetTriggerByName(workSpace, name);
    if (trigger == NULL) {
        trigger = AddTrigger(header, name, condition, 0);
        PARAM_CHECK(trigger != NULL, return -1, "Failed to create trigger %s", name);
    }
    if (type == TRIGGER_BOOT) { // 设置trigger立刻删除，如果是boot
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_ONCE);
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_SUBTRIGGER);
    }
    PARAM_LOGD("ParseTrigger %s type %d count %d", name, type, header->triggerCount);

    // 添加命令行
    cJSON *cmdItems = cJSON_GetObjectItem(triggerItem, CMDS_ARR_NAME_IN_JSON);
    PARAM_CHECK(cJSON_IsArray(cmdItems), return -1, "Command item must be array");
    int cmdLinesCnt = cJSON_GetArraySize(cmdItems);
    PARAM_CHECK(cmdLinesCnt > 0, return -1, "Command array size must positive %s", name);

    int ret;
    int cmdKeyIndex = 0;
    for (int i = 0; (i < cmdLinesCnt) && (i < TRIGGER_MAX_CMD); ++i) {
        char *cmdLineStr = cJSON_GetStringValue(cJSON_GetArrayItem(cmdItems, i));
        PARAM_CHECK(cmdLineStr != NULL, continue, "Command is null");

        size_t cmdLineLen = strlen(cmdLineStr);
        const char *matchCmd = GetMatchCmd(cmdLineStr, &cmdKeyIndex);
        PARAM_CHECK(matchCmd != NULL, continue, "Command not support %s", cmdLineStr);
        size_t matchLen = strlen(matchCmd);
        if (matchLen == cmdLineLen) {
            ret = AddCommand(trigger, (uint32_t)cmdKeyIndex, NULL);
        } else {
            ret = AddCommand(trigger, (uint32_t)cmdKeyIndex, cmdLineStr + matchLen);
        }
        PARAM_CHECK(ret == 0, continue, "Failed to add command %s", cmdLineStr);
    }
    return 0;
}

int ParseTriggerConfig(const cJSON *fileRoot)
{
    PARAM_CHECK(fileRoot != NULL, return -1, "Invalid file");
    cJSON *triggers = cJSON_GetObjectItemCaseSensitive(fileRoot, TRIGGER_ARR_NAME_IN_JSON);
    PARAM_CHECK(cJSON_IsArray(triggers), return -1, "Trigger item must array");

    int size = cJSON_GetArraySize(triggers);
    PARAM_CHECK(size > 0, return -1, "Trigger array size must positive");

    for (int i = 0; i < size && i < TRIGGER_MAX_CMD; ++i) {
        cJSON *item = cJSON_GetArrayItem(triggers, i);
        ParseTrigger_(&g_triggerWorkSpace, item);
    }
#ifdef PARAM_TEST
    // for test
    AddTestTrigger(&g_triggerWorkSpace);
#endif
    return 0;
}

int InitTriggerWorkSpace(void)
{
    if (g_triggerWorkSpace.eventHandle != NULL) {
        return 0;
    }
    g_triggerWorkSpace.cmdExec = DoCmdExec;
    ParamEventTaskCreate(&g_triggerWorkSpace.eventHandle, ProcessParamEvent, ProcessBeforeEvent);
    PARAM_CHECK(g_triggerWorkSpace.eventHandle != NULL, return -1, "Failed to event handle");

    // executeQueue
    g_triggerWorkSpace.executeQueue.executeQueue = calloc(1, TRIGGER_EXECUTE_QUEUE * sizeof(TriggerNode *));
    PARAM_CHECK(g_triggerWorkSpace.executeQueue.executeQueue != NULL,
        return -1, "Failed to alloc memory for executeQueue");
    g_triggerWorkSpace.executeQueue.queueCount = TRIGGER_EXECUTE_QUEUE;
    g_triggerWorkSpace.executeQueue.startIndex = 0;
    g_triggerWorkSpace.executeQueue.endIndex = 0;

    // normal trigger
    for (size_t i = 0; i < sizeof(g_triggerWorkSpace.triggerHead) / sizeof(g_triggerWorkSpace.triggerHead[0]); i++) {
        PARAM_TRIGGER_HEAD_INIT(g_triggerWorkSpace.triggerHead[i]);
    }
    // for watcher trigger
    PARAM_TRIGGER_HEAD_INIT(g_triggerWorkSpace.watcher.triggerHead);
    ListInit(&g_triggerWorkSpace.waitList);
    return 0;
}

void CloseTriggerWorkSpace(void)
{
    // 释放trigger
    for (size_t i = 0; i < sizeof(g_triggerWorkSpace.triggerHead) / sizeof(g_triggerWorkSpace.triggerHead[0]); i++) {
        ClearTrigger(&g_triggerWorkSpace.triggerHead[i]);
    }
    free(g_triggerWorkSpace.executeQueue.executeQueue);
    g_triggerWorkSpace.executeQueue.executeQueue = NULL;
    ParamTaskClose(g_triggerWorkSpace.eventHandle);
    g_triggerWorkSpace.eventHandle = NULL;
}

int CheckAndMarkTrigger(int type, const char *name)
{
    if (type == TRIGGER_PARAM) {
        return MarkTriggerToParam(&g_triggerWorkSpace, &g_triggerWorkSpace.triggerHead[type], name);
    } else if (type != TRIGGER_PARAM_WAIT) {
        return 0;
    }

    ParamWatcher *watcher = GetNextParamWatcher(&g_triggerWorkSpace, NULL);
    while (watcher != NULL) {
        if (MarkTriggerToParam(&g_triggerWorkSpace, &watcher->triggerHead, name)) {
            return 1;
        }
        watcher = GetNextParamWatcher(&g_triggerWorkSpace, watcher);
    }
    return 0;
}

TriggerWorkSpace *GetTriggerWorkSpace(void)
{
    return &g_triggerWorkSpace;
}

ParamWatcher *GetParamWatcher(const ParamTaskPtr worker)
{
    if (worker != NULL) {
        return (ParamWatcher *)ParamGetTaskUserData(worker);
    }
    return &g_triggerWorkSpace.watcher;
}
