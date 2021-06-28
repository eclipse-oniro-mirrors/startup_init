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

#include "trigger_processor.h"
#include <pthread.h>
#include <unistd.h>

#include "init_cmds.h"
#include "property_manager.h"
#include "trigger_checker.h"
#include "uv.h"

#define SYS_POWER_CTRL "sys.powerctrl."
static int g_triggerServiceStart = 0;
static TriggerWorkSpace g_triggerWorkSpace = {};

static int DoCmdExecute(TriggerNode *trigger, const char *cmdName, const char *command)
{
    TRIGGER_CHECK(trigger != NULL && cmdName != NULL && command != NULL, return -1, "Invalid param");
    if (strncmp(cmdName, TRIGGER_CMD, strlen(TRIGGER_CMD)) == 0) {
        u_int32_t triggerIndex = 0;
        TriggerNode *node = GetTriggerByName(&g_triggerWorkSpace, command, &triggerIndex);
        if (node != NULL && !TRIGGER_NODE_IN_QUEUE(node)) { // 不在队列中
            TRIGGER_LOGI("DoCmdExecute trigger %s", node->name);
            TRIGGER_NODE_SET_QUEUE_FLAG(node);
            ExecuteQueuePush(&g_triggerWorkSpace, node, triggerIndex);
        }
        return 0;
    }
    TRIGGER_LOGI("DoCmdExecute trigger %s cmd %s", trigger->name, cmdName);
    DoCmdByName(cmdName, command);
    return 0;
}

static int DoTiggerCheckResult(TriggerNode *trigger, u_int32_t triggerIndex)
{
    // 直接执行，不需要添加队列
    if (!g_triggerServiceStart) {
        TRIGGER_LOGI("DoTiggerExecute trigger %s", trigger->name);
        ExecuteTrigger(&g_triggerWorkSpace, trigger, DoCmdExecute);
        TRIGGER_NODE_CLEAR_QUEUE_FLAG(trigger);
    }

    // 已经在队列中了，则不执行 TODO
    if (TRIGGER_NODE_IN_QUEUE(trigger)) {
        TRIGGER_LOGI("DoTiggerExecute trigger %s has been waiting execute", trigger->name);
        return 0;
    }

    if (trigger->type == TRIGGER_BOOT) { // 立刻执行
        TRIGGER_LOGI("DoTiggerExecute trigger %s", trigger->name);
        ExecuteTrigger(&g_triggerWorkSpace, trigger, DoCmdExecute);
        TRIGGER_NODE_CLEAR_QUEUE_FLAG(trigger);
    } else {
        TRIGGER_NODE_SET_QUEUE_FLAG(trigger);
        TRIGGER_LOGI("DoTiggerExecute trigger %s", trigger->name);
        ExecuteQueuePush(&g_triggerWorkSpace, trigger, triggerIndex);
    }
    return 0;
}

void ExecuteQueueWork()
{
    int executeCount = 0;
    TriggerNode *trigger = ExecuteQueuePop(&g_triggerWorkSpace);
    while (trigger != NULL) {
        ExecuteTrigger(&g_triggerWorkSpace, trigger, DoCmdExecute);
        TRIGGER_NODE_CLEAR_QUEUE_FLAG(trigger);
        executeCount++;
        if (executeCount > 5) {
            break;
        }
        trigger = ExecuteQueuePop(&g_triggerWorkSpace);
    }
}

static void CheckTriggers(int type, void *content, u_int32_t contentLen)
{
    switch (type) {
        case EVENT_PROPERTY: {
            CheckPropertyTrigger(&g_triggerWorkSpace, content, contentLen, DoTiggerCheckResult);
            break;
        }
        case EVENT_BOOT: {
            CheckTrigger(&g_triggerWorkSpace, TRIGGER_BOOT, content, contentLen, DoTiggerCheckResult);
            break;
        }
        default:
            TRIGGER_LOGI("CheckTriggers: %d", type);
            break;
    }
}

static void ProcessAfterEvent(uv_work_t *req, int status)
{
    free(req);
    ExecuteQueueWork();
}

static void ProcessEvent(uv_work_t *req)
{
    TriggerDataEvent *event = (TriggerDataEvent *)req;
    CheckTriggers(event->type, event->content, event->contentSize);
}

static const char *GetCmdInfo(const char *content, u_int32_t contentSize, char **cmdParam)
{
    char cmd[MAX_CMD_NAME_LEN + 1] = { 0 };
    int ret = GetValueFromContent(content, contentSize, 0, cmd, sizeof(cmd));
    TRIGGER_CHECK(ret == 0, return NULL, "Failed parse cmd");
    u_int32_t cmdLen = strlen(cmd);
    TRIGGER_CHECK(cmdLen < MAX_CMD_NAME_LEN, return NULL, "Failed parse cmd");
    cmd[cmdLen] = ' ';
    cmd[cmdLen + 1] = '\0';
    *cmdParam = content + cmdLen + 1;
    return GetMatchCmd(cmd);
}

static void SendTriggerEvent(TriggerDataEvent *event)
{
    int ctrlSize = strlen(SYS_POWER_CTRL);
    if (strncmp(event->content, SYS_POWER_CTRL, ctrlSize) == 0) {
        char *cmdParam = NULL;
        const char *matchCmd = GetCmdInfo(event->content + ctrlSize, event->contentSize - ctrlSize, &cmdParam);
        if (matchCmd != NULL) {
            DoCmdByName(matchCmd, cmdParam);
        } else {
            TRIGGER_LOGE("SendTriggerEvent cmd %s not found", event->content);
        }
        free(event);
        return;
    }
    if (event->type == EVENT_BOOT || g_triggerServiceStart == 0) {
        CheckTriggers(EVENT_PROPERTY, event->content, event->contentSize);
        free(event);
    } else {
        uv_queue_work(uv_default_loop(), &event->request, ProcessEvent, ProcessAfterEvent);
    }
}

void PostPropertyTrigger(const char *name, const char *value)
{
    TRIGGER_CHECK(name != NULL && value != NULL, return, "Invalid param");
    TRIGGER_LOGI("PostPropertyTrigger %s ", name);
    int contentLen = strlen(name) + strlen(value) + 2;
    TriggerDataEvent *event = (TriggerDataEvent *)malloc(sizeof(TriggerDataEvent) + contentLen);
    TRIGGER_CHECK(event != NULL, return, "Failed to alloc memory");
    event->type = EVENT_PROPERTY;
    event->request.data = (char*)event + sizeof(uv_work_t);
    event->contentSize = BuildPropertyContent(event->content, contentLen, name, value);
    TRIGGER_CHECK(event->contentSize > 0, return, "Failed to copy porperty");
    SendTriggerEvent(event);
    TRIGGER_LOGI("PostPropertyTrigger %s success", name);
}

void PostTrigger(EventType type, void *content, u_int32_t contentLen)
{
    TRIGGER_LOGI("PostTrigger %d", type);
    TRIGGER_CHECK(content != NULL && contentLen > 0, return, "Invalid param");
    if (type == EVENT_BOOT || g_triggerServiceStart == 0) {
        CheckTriggers(type, content, contentLen);
    } else {
        TriggerDataEvent *event = (TriggerDataEvent *)malloc(sizeof(TriggerDataEvent) + contentLen + 1);
        TRIGGER_CHECK(event != NULL, return, "Failed to alloc memory");
        event->type = type;
        event->request.data = (char*)event + sizeof(uv_work_t);
        event->contentSize = contentLen;
        memcpy_s(event->content, contentLen, content, contentLen);
        event->content[contentLen] = '\0';
        SendTriggerEvent(event);
    }
    TRIGGER_LOGI("PostTrigger %d success", type);
}

void StartTriggerService()
{
    TRIGGER_LOGI("StartTriggerService ");
    g_triggerServiceStart = 1;
}

int ParseTriggerConfig(cJSON *fileRoot)
{
    TRIGGER_CHECK(fileRoot != NULL, return -1, "Invalid file");
    int ret = InitTriggerWorkSpace(&g_triggerWorkSpace);
    TRIGGER_CHECK(ret == 0, return -1, "Failed to init trigger");

    cJSON *triggers = cJSON_GetObjectItemCaseSensitive(fileRoot, TRIGGER_ARR_NAME_IN_JSON);
    TRIGGER_CHECK(cJSON_IsArray(triggers), return -1, "Trigger item must array");

    int size = cJSON_GetArraySize(triggers);
    TRIGGER_CHECK(size > 0, return -1, "Trigger array size must positive");

    for (int i = 0; i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(triggers, i);
        ParseTrigger(&g_triggerWorkSpace, item);
    }
    return 0;
}

void DoTriggerExec(const char *content)
{
    TRIGGER_CHECK(content != NULL, return, "Invalid trigger content");
    u_int32_t triggerIndex = 0;
    TriggerNode *trigger = GetTriggerByName(&g_triggerWorkSpace, content, &triggerIndex);
    if (trigger != NULL) {
        ExecuteTrigger(&g_triggerWorkSpace, trigger, DoCmdExecute);
    }
}

TriggerWorkSpace *GetTriggerWorkSpace()
{
    return &g_triggerWorkSpace;
}