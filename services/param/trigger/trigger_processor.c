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
#include <ctype.h>
#include <unistd.h>

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

static int DoTriggerExecute_(const TriggerNode *trigger, const char *content, uint32_t size)
{
    PARAM_CHECK(trigger != NULL, return -1, "Invalid trigger");
    PARAM_LOGI("DoTriggerExecute_ trigger %s type: %d", GetTriggerName(trigger), trigger->type);
    PARAM_CHECK(trigger->type <= TRIGGER_UNKNOW, return -1, "Invalid trigger type %d", trigger->type);
    CommandNode *cmd = GetNextCmdNode((JobNode *)trigger, NULL);
    while (cmd != NULL) {
#ifndef STARTUP_INIT_TEST
        DoCmdByIndex(cmd->cmdKeyIndex, cmd->content);
#endif
        cmd = GetNextCmdNode((JobNode *)trigger, cmd);
    }
    return 0;
}

static int DoTiggerCheckResult(TriggerNode *trigger, const char *content, uint32_t size)
{
    UNUSED(content);
    UNUSED(size);
    if (TRIGGER_IN_QUEUE(trigger)) {
        PARAM_LOGI("DoTiggerExecute trigger %s has been waiting execute", GetTriggerName(trigger));
        return 0;
    }
    TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_QUEUE);
    PARAM_LOGI("Add trigger %s to execute queue", GetTriggerName(trigger));
    ExecuteQueuePush(&g_triggerWorkSpace, trigger);
    return 0;
}

static int ExecuteTiggerImmediately(TriggerNode *trigger, const char *content, uint32_t size)
{
    PARAM_CHECK(trigger != NULL, return -1, "Invalid trigger");
    PARAM_LOGV("ExecuteTiggerImmediately trigger %s", GetTriggerName(trigger));
    TriggerHeader *triggerHead = GetTriggerHeader(&g_triggerWorkSpace, trigger->type);
    if (triggerHead != NULL) {
        triggerHead->executeTrigger(trigger, content, size);
        TRIGGER_CLEAR_FLAG(trigger, TRIGGER_FLAGS_QUEUE);

        if (TRIGGER_TEST_FLAG(trigger, TRIGGER_FLAGS_ONCE)) {
            FreeTrigger(&g_triggerWorkSpace, trigger);
        }
    }
    return 0;
}

static void StartTiggerExecute_(TriggerNode *trigger, const char *content, uint32_t size)
{
    TriggerHeader *triggerHead = GetTriggerHeader(&g_triggerWorkSpace, trigger->type);
    if (triggerHead != NULL) {
        PARAM_LOGV("StartTiggerExecute_ trigger %s flags:0x%04x",
            GetTriggerName(trigger), trigger->flags);
        triggerHead->executeTrigger(trigger, content, size);
        TRIGGER_CLEAR_FLAG(trigger, TRIGGER_FLAGS_QUEUE);
        if (TRIGGER_TEST_FLAG(trigger, TRIGGER_FLAGS_SUBTRIGGER)) { // boot && xxx=xxx trigger
            const char *condition = triggerHead->getCondition(trigger);
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_UNKNOW, condition, strlen(condition), ExecuteTiggerImmediately);
        }
        if (TRIGGER_TEST_FLAG(trigger, TRIGGER_FLAGS_ONCE)) {
            FreeTrigger(&g_triggerWorkSpace, trigger);
        }
    }
}

static void ExecuteQueueWork(uint32_t maxCount)
{
    uint32_t executeCount = 0;
    TriggerNode *trigger = ExecuteQueuePop(&g_triggerWorkSpace);
    while (trigger != NULL) {
        StartTiggerExecute_(trigger, NULL, 0);
        executeCount++;
        if (executeCount > maxCount) {
            break;
        }
        trigger = ExecuteQueuePop(&g_triggerWorkSpace);
    }
}

PARAM_STATIC void ProcessBeforeEvent(const ParamTaskPtr stream,
    uint64_t eventId, const uint8_t *content, uint32_t size)
{
    PARAM_LOGV("ProcessBeforeEvent %s ", (char *)content);
    switch (eventId) {
        case EVENT_TRIGGER_PARAM: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_PARAM,
                (const char *)content, size, DoTiggerCheckResult);
            ExecuteQueueWork(MAX_TRIGGER_COUNT_RUN_ONCE);
            break;
        }
        case EVENT_TRIGGER_BOOT: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_BOOT,
                (const char *)content, size, DoTiggerCheckResult);
            ExecuteQueueWork(MAX_TRIGGER_COUNT_RUN_ONCE);
            if (g_triggerWorkSpace.bootStateChange != NULL) {
                g_triggerWorkSpace.bootStateChange((const char *)content);
            }
            break;
        }
        case EVENT_TRIGGER_PARAM_WAIT: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_PARAM_WAIT,
                (const char *)content, size, ExecuteTiggerImmediately);
            break;
        }
        case EVENT_TRIGGER_PARAM_WATCH: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_PARAM_WATCH,
                (const char *)content, size, ExecuteTiggerImmediately);
            break;
        }
        default:
            break;
    }
}

static const char *GetCmdInfo(const char *content, uint32_t contentSize)
{
    static char buffer[PARAM_NAME_LEN_MAX] = {0};
    uint32_t index = 0;
    while (index < contentSize && index < PARAM_NAME_LEN_MAX) {
        if (*(content + index) == '=' || *(content + index) == ',') {
            break;
        }
        buffer[index] = *(content + index);
        index++;
    }
    if (index >= (PARAM_NAME_LEN_MAX - 1)) {
        return NULL;
    }
    buffer[index] = ' ';
    buffer[index + 1] = '\0';
    int cmdIndex = 0;
    return GetMatchCmd(buffer, &cmdIndex);
}

static void DoServiceCtrlTrigger(const char *cmdStart, uint32_t len, int onlyValue)
{
    char *cmdParam = (char *)cmdStart;
    const char *matchCmd = GetCmdInfo(cmdStart, len);
    if (matchCmd != NULL) {
        size_t cmdLen = strlen(matchCmd);
        if (onlyValue != 0 && cmdParam != NULL && strlen(cmdParam) > cmdLen) {
            cmdParam += cmdLen + 1;
        }
        PARAM_LOGV("DoServiceCtrlTrigger matchCmd %s cmdParam %s", matchCmd, cmdParam);
#ifndef STARTUP_INIT_TEST
        DoCmdByName(matchCmd, cmdParam);
#endif
    } else {
        PARAM_LOGE("DoServiceCtrlTrigger cmd %s not found", cmdStart);
    }
}

static void SendTriggerEvent(int type, const char *content, uint32_t contentLen)
{
    PARAM_CHECK(content != NULL, return, "Invalid param");
    PARAM_LOGV("SendTriggerEvent type %d content %s", type, content);
    if (type == EVENT_TRIGGER_PARAM) {
        int ctrlSize = strlen(SYS_POWER_CTRL);
        int prefixSize = strlen(OHOS_SERVICE_CTRL_PREFIX);
        if (strncmp(content, SYS_POWER_CTRL, ctrlSize) == 0) {
            DoServiceCtrlTrigger(content + ctrlSize, contentLen - ctrlSize, 0);
        } else if (strncmp(content, OHOS_SERVICE_CTRL_PREFIX, prefixSize) == 0) {
            DoServiceCtrlTrigger(content + prefixSize, contentLen - prefixSize, 1);
        } else if (strncmp(content, OHOS_CTRL_START, strlen(OHOS_CTRL_START)) == 0) {
            StartServiceByName(content + strlen(OHOS_CTRL_START));
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
        "fs", "early-fs", "post-fs", "late-fs", "early-boot", "post-fs-data", "reboot", "suspend"
    };
    for (size_t i = 0; i < ARRAY_LENGTH(triggerTypeStr); i++) {
        if (strcmp(triggerTypeStr[i], type) == 0) {
            return TRIGGER_BOOT;
        }
    }
    return TRIGGER_UNKNOW;
}

static int GetCommandInfo(const char *cmdLine, int *cmdKeyIndex, char **content)
{
    const char *matchCmd = GetMatchCmd(cmdLine, cmdKeyIndex);
    PARAM_CHECK(matchCmd != NULL, return -1, "Command not support %s", cmdLine);
    char *str = strstr(cmdLine, matchCmd);
    if (str != NULL) {
        str += strlen(matchCmd);
    }
    while (str != NULL && isspace(*str)) {
        str++;
    }
    *content = str;
    return 0;
}

static int ParseTrigger_(const TriggerWorkSpace *workSpace,
    const cJSON *triggerItem, int (*checkJobValid)(const char *jobName))
{
    PARAM_CHECK(triggerItem != NULL, return -1, "Invalid file");
    PARAM_CHECK(workSpace != NULL, return -1, "Failed to create trigger list");
    char *name = cJSON_GetStringValue(cJSON_GetObjectItem(triggerItem, "name"));
    PARAM_CHECK(name != NULL, return -1, "Can not get name from cfg");
    char *condition = cJSON_GetStringValue(cJSON_GetObjectItem(triggerItem, "condition"));
    int type = GetTriggerType(name);
    PARAM_CHECK(type <= TRIGGER_UNKNOW, return -1, "Failed to get trigger index");
    if (type != TRIGGER_BOOT && checkJobValid != NULL && checkJobValid(name) != 0) {
        PARAM_LOGI("Trigger %s not exist in group", name);
        return 0;
    }

    TriggerHeader *header = GetTriggerHeader(workSpace, type);
    PARAM_CHECK(header != NULL, return -1, "Failed to get header %d", type);
    JobNode *trigger = UpdateJobTrigger(workSpace, type, condition, name);
    PARAM_CHECK(trigger != NULL, return -1, "Failed to create trigger %s", name);
    PARAM_LOGV("ParseTrigger %s type %d count %d", name, type, header->triggerCount);
    cJSON *cmdItems = cJSON_GetObjectItem(triggerItem, CMDS_ARR_NAME_IN_JSON);
    PARAM_CHECK(cJSON_IsArray(cmdItems), return -1, "Command item must be array");
    int cmdLinesCnt = cJSON_GetArraySize(cmdItems);
    PARAM_CHECK(cmdLinesCnt > 0, return -1, "Command array size must positive %s", name);

    int ret;
    int cmdKeyIndex = 0;
    for (int i = 0; (i < cmdLinesCnt) && (i < TRIGGER_MAX_CMD); ++i) {
        char *cmdLineStr = cJSON_GetStringValue(cJSON_GetArrayItem(cmdItems, i));
        PARAM_CHECK(cmdLineStr != NULL, continue, "Command is null");

        char *content = NULL;
        ret = GetCommandInfo(cmdLineStr, &cmdKeyIndex, &content);
        PARAM_CHECK(ret == 0, continue, "Command not support %s", cmdLineStr);
        ret = AddCommand(trigger, (uint32_t)cmdKeyIndex, content);
        PARAM_CHECK(ret == 0, continue, "Failed to add command %s", cmdLineStr);
        header->cmdNodeCount++;
    }
    return 0;
}

int ParseTriggerConfig(const cJSON *fileRoot, int (*checkJobValid)(const char *jobName))
{
    PARAM_CHECK(fileRoot != NULL, return -1, "Invalid file");
    cJSON *triggers = cJSON_GetObjectItemCaseSensitive(fileRoot, TRIGGER_ARR_NAME_IN_JSON);
    if (triggers == NULL) {
        return 0;
    }
    PARAM_CHECK(cJSON_IsArray(triggers), return -1, "Trigger item must array");

    int size = cJSON_GetArraySize(triggers);
    PARAM_CHECK(size > 0, return -1, "Trigger array size must positive");

    for (int i = 0; i < size && i < TRIGGER_MAX_CMD; ++i) {
        cJSON *item = cJSON_GetArrayItem(triggers, i);
        ParseTrigger_(&g_triggerWorkSpace, item, checkJobValid);
    }
    return 0;
}

int CheckAndMarkTrigger(int type, const char *name)
{
    TriggerHeader *triggerHead = GetTriggerHeader(&g_triggerWorkSpace, type);
    if (triggerHead) {
        return triggerHead->checkAndMarkTrigger(&g_triggerWorkSpace, type, name);
    }
    return 0;
}

int InitTriggerWorkSpace(void)
{
    if (g_triggerWorkSpace.eventHandle != NULL) {
        return 0;
    }
    g_triggerWorkSpace.bootStateChange = NULL;
    ParamEventTaskCreate(&g_triggerWorkSpace.eventHandle, ProcessBeforeEvent);
    PARAM_CHECK(g_triggerWorkSpace.eventHandle != NULL, return -1, "Failed to event handle");

    // executeQueue
    g_triggerWorkSpace.executeQueue.executeQueue = calloc(1, TRIGGER_EXECUTE_QUEUE * sizeof(TriggerNode *));
    PARAM_CHECK(g_triggerWorkSpace.executeQueue.executeQueue != NULL,
        return -1, "Failed to alloc memory for executeQueue");
    g_triggerWorkSpace.executeQueue.queueCount = TRIGGER_EXECUTE_QUEUE;
    g_triggerWorkSpace.executeQueue.startIndex = 0;
    g_triggerWorkSpace.executeQueue.endIndex = 0;
    InitTriggerHead(&g_triggerWorkSpace);
    RegisterTriggerExec(TRIGGER_BOOT, DoTriggerExecute_);
    RegisterTriggerExec(TRIGGER_PARAM, DoTriggerExecute_);
    RegisterTriggerExec(TRIGGER_UNKNOW, DoTriggerExecute_);
    PARAM_LOGV("InitTriggerWorkSpace success");
    return 0;
}

void CloseTriggerWorkSpace(void)
{
    for (size_t i = 0; i < sizeof(g_triggerWorkSpace.triggerHead) / sizeof(g_triggerWorkSpace.triggerHead[0]); i++) {
        ClearTrigger(&g_triggerWorkSpace, i);
    }
    free(g_triggerWorkSpace.executeQueue.executeQueue);
    g_triggerWorkSpace.executeQueue.executeQueue = NULL;
    ParamTaskClose(g_triggerWorkSpace.eventHandle);
    g_triggerWorkSpace.eventHandle = NULL;
}

TriggerWorkSpace *GetTriggerWorkSpace(void)
{
    return &g_triggerWorkSpace;
}

void RegisterTriggerExec(int type,
    int32_t (*executeTrigger)(const struct tagTriggerNode_ *, const char *, uint32_t))
{
    TriggerHeader *triggerHead = GetTriggerHeader(&g_triggerWorkSpace, type);
    if (triggerHead != NULL) {
        triggerHead->executeTrigger = executeTrigger;
    }
}

void DoTriggerExec(const char *triggerName)
{
    PARAM_CHECK(triggerName != NULL, return, "Invalid param");
    JobNode *trigger = GetTriggerByName(&g_triggerWorkSpace, triggerName);
    if (trigger != NULL && !TRIGGER_IN_QUEUE((TriggerNode *)trigger)) {
        PARAM_LOGI("DoTriggerExec trigger %s", trigger->name);
        TRIGGER_SET_FLAG((TriggerNode *)trigger, TRIGGER_FLAGS_QUEUE);
        ExecuteQueuePush(&g_triggerWorkSpace, (TriggerNode *)trigger);
    } else {
        PARAM_LOGE("Can not find trigger %s", triggerName);
    }
}

void DoJobExecNow(const char *triggerName)
{
    PARAM_CHECK(triggerName != NULL, return, "Invalid param");
    JobNode *trigger = GetTriggerByName(&g_triggerWorkSpace, triggerName);
    if (trigger != NULL) {
        StartTiggerExecute_((TriggerNode *)trigger, NULL, 0);
    }
}

int AddCompleteJob(const char *name, const char *condition, const char *cmdContent)
{
    PARAM_CHECK(name != NULL, return -1, "Invalid name");
    PARAM_CHECK(condition != NULL, return -1, "Invalid condition");
    PARAM_CHECK(cmdContent != NULL, return -1, "Invalid cmdContent");
    int type = GetTriggerType(name);
    PARAM_CHECK(type <= TRIGGER_UNKNOW, return -1, "Failed to get trigger index");
    TriggerHeader *header = GetTriggerHeader(&g_triggerWorkSpace, type);
    PARAM_CHECK(header != NULL, return -1, "Failed to get header %d", type);

    JobNode *trigger = UpdateJobTrigger(&g_triggerWorkSpace, type, condition, name);
    PARAM_CHECK(trigger != NULL, return -1, "Failed to create trigger");
    char *content = NULL;
    int cmdKeyIndex = 0;
    int ret = GetCommandInfo(cmdContent, &cmdKeyIndex, &content);
    PARAM_CHECK(ret == 0, return -1, "Command not support %s", cmdContent);
    ret = AddCommand(trigger, (uint32_t)cmdKeyIndex, content);
    PARAM_CHECK(ret == 0, return -1, "Failed to add command %s", cmdContent);
    header->cmdNodeCount++;
    PARAM_LOGV("AddCompleteJob %s type %d count %d", name, type, header->triggerCount);
    return 0;
}

void RegisterBootStateChange(void (*bootStateChange)(const char *))
{
    if (bootStateChange != NULL) {
        g_triggerWorkSpace.bootStateChange = bootStateChange;
    }
}
