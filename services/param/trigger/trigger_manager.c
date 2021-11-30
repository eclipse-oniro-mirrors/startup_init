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

#include "trigger_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_cmds.h"
#include "param_manager.h"
#include "trigger_checker.h"

int AddCommand(TriggerNode *trigger, uint32_t cmdKeyIndex, const char *content)
{
    PARAM_CHECK(trigger != NULL && trigger->triggerHead != NULL, return -1, "trigger is null");
    uint32_t size = sizeof(CommandNode);
    size += (content == NULL) ? 1 : (strlen(content) + 1);
    size = PARAM_ALIGN(size);

    CommandNode *node = (CommandNode *)calloc(1, size);
    PARAM_CHECK(node != NULL, return -1, "Failed to alloc memory for command");
    node->cmdKeyIndex = cmdKeyIndex;
    node->next = NULL;
    node->content[0] = '\0';
    if (content != NULL && strlen(content) != 0) {
        int ret = memcpy_s(node->content, size, content, strlen(content));
        node->content[strlen(content)] = '\0';
        PARAM_CHECK(ret == EOK, free(node);
            return 0, "Failed to copy command");
    }
    // 插入队列
    if (trigger->firstCmd == NULL) {
        trigger->firstCmd = node;
        trigger->lastCmd = node;
    } else {
        PARAM_CHECK(trigger->lastCmd != NULL, free(node);
            return 0, "Invalid last cmd");
        trigger->lastCmd->next = node;
        trigger->lastCmd = node;
    }
    trigger->triggerHead->cmdNodeCount++;
    return 0;
}

CommandNode *GetNextCmdNode(const TriggerNode *trigger, const CommandNode *curr)
{
    PARAM_CHECK(trigger != NULL, return NULL, "trigger is null");
    if (curr == NULL) {
        return trigger->firstCmd;
    }
    return curr->next;
}

TriggerNode *AddTrigger(TriggerHeader *triggerHead, const char *name, const char *condition, uint16_t extDataSize)
{
    PARAM_CHECK(triggerHead != NULL && name != NULL, return NULL, "triggerHead is null");
    PARAM_CHECK(extDataSize <= PARAM_CONST_VALUE_LEN_MAX, return NULL, "extDataSize is longest %d", extDataSize);
    uint32_t nameLen = strlen(name);
    uint32_t triggerNodeLen = PARAM_ALIGN(nameLen + 1) + sizeof(TriggerNode);
    uint32_t conditionLen = 0;
    if (condition != NULL && strlen(condition) != 0) {
        conditionLen = PARAM_ALIGN(strlen(condition) + 1) + CONDITION_EXTEND_LEN;
    }

    TriggerNode *node = (TriggerNode *)calloc(1, triggerNodeLen + conditionLen + extDataSize);
    PARAM_CHECK(node != NULL, return NULL, "Failed to alloc memory for trigger");
    int ret = memcpy_s(node->name, triggerNodeLen - sizeof(TriggerNode), name, nameLen);
    PARAM_CHECK(ret == EOK, free(node);
        return NULL, "Failed to memcpy_s for trigger");
    node->name[nameLen] = '\0';
    node->condition = NULL;
    if (conditionLen != 0) {
        char *cond = node->name + PARAM_ALIGN(nameLen + 1);
        ret = ConvertInfixToPrefix(condition, cond, conditionLen);
        PARAM_CHECK(ret == 0, free(node);
            return NULL, "Failed to convert condition for trigger");
        node->condition = cond;
    }
    node->flags = 0;
    node->firstCmd = NULL;
    node->lastCmd = NULL;
    node->triggerHead = triggerHead;
    ListInit(&node->node);
    node->extDataSize = extDataSize;
    node->extDataOffset = triggerNodeLen + conditionLen;
    ListAddTail(&triggerHead->triggerList, &node->node);
    triggerHead->triggerCount++;
    return node;
}

void ClearTrigger(TriggerHeader *head)
{
    PARAM_CHECK(head != NULL, return, "head is null");
    ListNode *node = head->triggerList.next;
    while (node != &head->triggerList && node != NULL) {
        ListRemove(node);
        ListInit(node);
        TriggerNode *trigger = ListEntry(node, TriggerNode, node);
        FreeTrigger(trigger);
        node = head->triggerList.next;
    }
    ListInit(&head->triggerList);
}

void FreeTrigger(TriggerNode *trigger)
{
    PARAM_CHECK(trigger != NULL, return, "trigger is null");
    PARAM_LOGD("FreeTrigger %s", trigger->name);
    TriggerHeader *triggerHead = trigger->triggerHead;
    CommandNode *cmd = trigger->firstCmd;
    while (cmd != NULL) {
        CommandNode *next = cmd->next;
        free(cmd);
        if (triggerHead != NULL) {
            triggerHead->cmdNodeCount--;
        }
        cmd = next;
    }
    trigger->lastCmd = NULL;
    trigger->firstCmd = NULL;
    ListRemove(&trigger->node);
    if (triggerHead != NULL) {
        triggerHead->triggerCount--;
    }

    // 如果在执行队列，从队列中移走
    if (!TRIGGER_IN_QUEUE(trigger)) {
        free(trigger);
        return;
    }
    TriggerExecuteQueue *executeQueue = &GetTriggerWorkSpace()->executeQueue;
    for (uint32_t i = executeQueue->startIndex; i < executeQueue->endIndex; i++) {
        if (executeQueue->executeQueue[i] == trigger) {
            executeQueue->executeQueue[i] = NULL;
            break;
        }
    }
    free(trigger);
}

static TriggerNode *GetNextTrigger(const TriggerHeader *triggerHead, const TriggerNode *curr)
{
    PARAM_CHECK(triggerHead != NULL, return NULL, "Invalid triggerHead");
    ListNode *node = NULL;
    if (curr != NULL) {
        node = curr->node.next;
    } else {
        node = triggerHead->triggerList.next;
    }
    if (node != &triggerHead->triggerList) {
        return ListEntry(node, TriggerNode, node);
    }
    return NULL;
}

static const char *GetTriggerCondition(const TriggerWorkSpace *workSpace, const TriggerNode *trigger)
{
    UNUSED(workSpace);
    return (trigger == NULL || trigger->condition == NULL) ? "" : trigger->condition;
}

TriggerNode *GetTriggerByName(const TriggerWorkSpace *workSpace, const char *triggerName)
{
    PARAM_CHECK(workSpace != NULL && triggerName != NULL, return NULL, "Invalid param");
    for (size_t i = 0; i < sizeof(workSpace->triggerHead) / sizeof(workSpace->triggerHead[0]); i++) {
        TriggerNode *trigger = GetNextTrigger(&workSpace->triggerHead[i], NULL);
        while (trigger != NULL) {
            if (strcmp(triggerName, trigger->name) == 0) {
                return trigger;
            }
            trigger = GetNextTrigger(&workSpace->triggerHead[i], trigger);
        }
    }
    return NULL;
}

int ExecuteQueuePush(TriggerWorkSpace *workSpace, const TriggerNode *trigger)
{
    PARAM_CHECK(workSpace != NULL, return -1, "Invalid workSpace");
    uint32_t index = workSpace->executeQueue.endIndex++ % workSpace->executeQueue.queueCount;
    workSpace->executeQueue.executeQueue[index] = (TriggerNode *)trigger;
    return 0;
}

TriggerNode *ExecuteQueuePop(TriggerWorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return NULL, "Invalid workSpace");
    TriggerNode *trigger = NULL;
    do {
        if (workSpace->executeQueue.endIndex <= workSpace->executeQueue.startIndex) {
            return NULL;
        }
        uint32_t currIndex = workSpace->executeQueue.startIndex % workSpace->executeQueue.queueCount;
        trigger = workSpace->executeQueue.executeQueue[currIndex];
        workSpace->executeQueue.executeQueue[currIndex] = NULL;
        workSpace->executeQueue.startIndex++;
    } while (trigger == NULL);
    return trigger;
}

static int CheckBootTriggerMatch(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize)
{
    UNUSED(workSpace);
    UNUSED(calculator);
    UNUSED(content);
    UNUSED(contentSize);
    PARAM_CHECK(trigger != NULL, return -1, "Invalid trigger");
    if (strncmp(trigger->name, content, contentSize) == 0) {
        return 1;
    }
    return 0;
}

static int CheckWatcherTriggerMatch(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize)
{
    UNUSED(workSpace);
    UNUSED(calculator);
    UNUSED(content);
    UNUSED(contentSize);
    PARAM_CHECK(trigger != NULL, return -1, "Invalid trigger");
    if (strncmp(trigger->name, content, strlen(trigger->name)) == 0) {
        return 1;
    }
    return 0;
}

static int CheckParamTriggerMatch(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize)
{
    UNUSED(content);
    UNUSED(contentSize);
    const char *condition = GetTriggerCondition(workSpace, trigger);
    if (calculator->inputName != NULL) { // 存在input数据时，先过滤非input的
        if (!CheckMatchSubCondition(condition, calculator->inputName, strlen(calculator->inputName))) {
            return 0;
        }
    }
    return ComputeCondition(calculator, condition);
}

static int CheckOtherTriggerMatch(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize)
{
    UNUSED(content);
    UNUSED(contentSize);
    const char *condition = GetTriggerCondition(workSpace, trigger);
    return ComputeCondition(calculator, condition);
}

static int CheckParamWaitMatch(TriggerWorkSpace *workSpace, int type,
    LogicCalculator *calculator, const char *content, uint32_t contentSize)
{
    UNUSED(type);
    ParamWatcher *watcher = GetNextParamWatcher(workSpace, NULL);
    while (watcher != NULL) {
        TriggerNode *trigger = GetNextTrigger(&watcher->triggerHead, NULL);
        while (trigger != NULL) {
            TriggerNode *next = GetNextTrigger(&watcher->triggerHead, trigger);
            if (CheckParamTriggerMatch(workSpace, calculator, trigger, content, contentSize) == 1) {
                calculator->triggerExecuter(trigger, content, contentSize);
            }
            trigger = next;
        }
        watcher = GetNextParamWatcher(workSpace, watcher);
    }
    return 0;
}

static int CheckParamWatcherMatch(TriggerWorkSpace *workSpace, int type,
    LogicCalculator *calculator, const char *content, uint32_t contentSize)
{
    UNUSED(type);
    TriggerNode *trigger = GetNextTrigger(&workSpace->watcher.triggerHead, NULL);
    while (trigger != NULL) {
        TriggerNode *next = GetNextTrigger(&workSpace->watcher.triggerHead, trigger);
        if (CheckWatcherTriggerMatch(workSpace, calculator, trigger, content, contentSize) == 1) {
            calculator->triggerExecuter(trigger, content, contentSize);
        }
        trigger = next;
    }
    return 0;
}

static int CheckTrigger_(TriggerWorkSpace *workSpace,
    LogicCalculator *calculator, int type, const char *content, uint32_t contentSize)
{
    static TRIGGER_MATCH triggerCheckMatch[TRIGGER_MAX] = {
        CheckBootTriggerMatch, CheckParamTriggerMatch, CheckOtherTriggerMatch
    };
    PARAM_CHECK(calculator != NULL, return -1, "Failed to check calculator");
    PARAM_CHECK(type < TRIGGER_MAX, return -1, "Invalid type %d", type);
    PARAM_CHECK((uint32_t)type < sizeof(triggerCheckMatch) / sizeof(triggerCheckMatch[0]),
        return -1, "Failed to get check function");
    PARAM_CHECK(triggerCheckMatch[type] != NULL, return -1, "Failed to get check function");

    TriggerNode *trigger = GetNextTrigger(&workSpace->triggerHead[type], NULL);
    while (trigger != NULL) {
        if (triggerCheckMatch[type](workSpace, calculator, trigger, content, contentSize) == 1) { // 等于1 则认为匹配
            calculator->triggerExecuter(trigger, content, contentSize);
        }
        trigger = GetNextTrigger(&workSpace->triggerHead[type], trigger);
    }
    return 0;
}

int CheckTrigger(TriggerWorkSpace *workSpace, int type,
    const char *content, uint32_t contentSize, PARAM_CHECK_DONE triggerExecuter)
{
    PARAM_CHECK(workSpace != NULL && content != NULL && triggerExecuter != NULL,
        return -1, "Failed arg for trigger");
    PARAM_LOGD("CheckTrigger type: %d content: %s ", type, content);
    int ret;
    LogicCalculator calculator;
    CalculatorInit(&calculator, MAX_CONDITION_NUMBER, sizeof(LogicData), 1);
    calculator.triggerExecuter = triggerExecuter;
    if (type == TRIGGER_PARAM || type == TRIGGER_PARAM_WAIT) {
        ret = GetValueFromContent(content, contentSize,
            0, calculator.inputName, SUPPORT_DATA_BUFFER_MAX);
        PARAM_CHECK(ret == 0, CalculatorFree(&calculator);
            return -1, "Failed parse content name");
        ret = GetValueFromContent(content, contentSize,
            strlen(calculator.inputName) + 1, calculator.inputContent, SUPPORT_DATA_BUFFER_MAX);
        PARAM_CHECK(ret == 0, CalculatorFree(&calculator);
            return -1, "Failed parse content value");
    } else if (type == TRIGGER_UNKNOW) {
        ret = memcpy_s(calculator.triggerContent, sizeof(calculator.triggerContent), content, contentSize);
        PARAM_CHECK(ret == EOK, CalculatorFree(&calculator);
            return -1, "Failed to memcpy");
        calculator.inputName = NULL;
        calculator.inputContent = NULL;
    }
    if (type == TRIGGER_PARAM_WAIT) {
        CheckParamWaitMatch(workSpace, PARAM_TRIGGER_FOR_WAIT, &calculator, content, contentSize);
    } else if (type == TRIGGER_PARAM_WATCH) {
        CheckParamWatcherMatch(workSpace, PARAM_TRIGGER_FOR_WATCH, &calculator, content, contentSize);
    } else {
        CheckTrigger_(workSpace, &calculator, type, content, contentSize);
    }
    CalculatorFree(&calculator);
    return 0;
}

int MarkTriggerToParam(const TriggerWorkSpace *workSpace, const TriggerHeader *triggerHead, const char *name)
{
    PARAM_CHECK(name != NULL, return 0, "name is null");
    int ret = 0;
    TriggerNode *trigger = GetNextTrigger(triggerHead, NULL);
    while (trigger != NULL) {
        if (GetTriggerCondition(workSpace, trigger) == NULL) {
            trigger = GetNextTrigger(triggerHead, trigger);
            continue;
        }
        const char *tmp = strstr(GetTriggerCondition(workSpace, trigger), name);
        if (tmp != NULL && strncmp(tmp + strlen(name), "=", 1) == 0) {
            TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_RELATED);
            ret = 1;
        }
        trigger = GetNextTrigger(triggerHead, trigger);
    }
    return ret;
}


ParamWatcher *GetNextParamWatcher(const TriggerWorkSpace *workSpace, const ParamWatcher *curr)
{
    PARAM_CHECK(workSpace != NULL, return NULL, "Invalid workSpace");
    ListNode *node = NULL;
    if (curr != NULL) {
        node = curr->node.next;
    } else {
        node = workSpace->waitList.next;
    }
    if (node == &workSpace->waitList) {
        return NULL;
    }
    return ListEntry(node, ParamWatcher, node);
}

TriggerNode *AddWatcherTrigger(ParamWatcher *watcher,
    int triggerType, const char *name, const char *condition, const TriggerExtData *extData)
{
    PARAM_CHECK(name != NULL, return NULL, "Invalid name");
    PARAM_CHECK(watcher != NULL && extData != NULL, return NULL, "Invalid watcher for %s", name);

    TriggerNode *trigger = AddTrigger(&watcher->triggerHead, name, condition, sizeof(TriggerExtData));
    PARAM_CHECK(trigger != NULL, return NULL, "Failed to create trigger for %s", name);
    // add command and arg is "name"
    int cmd = CMD_INDEX_FOR_PARA_WATCH;
    if (triggerType == TRIGGER_PARAM_WAIT) { // wait 回复后立刻删除
        TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_ONCE);
        cmd = CMD_INDEX_FOR_PARA_WAIT;
    }
    int ret = AddCommand(trigger, cmd, name);
    PARAM_CHECK(ret == 0, FreeTrigger(trigger);
        return NULL, "Failed to add command for %s", name);
    TriggerExtData *localData = TRIGGER_GET_EXT_DATA(trigger, TriggerExtData);
    PARAM_CHECK(localData != NULL, return NULL, "Failed to get trigger ext data");
    localData->excuteCmd = extData->excuteCmd;
    localData->watcherId = extData->watcherId;
    localData->watcher = watcher;
    return trigger;
}

void DelWatcherTrigger(const ParamWatcher *watcher, uint32_t watcherId)
{
    PARAM_CHECK(watcher != NULL, return, "Failed to add watcher ");
    TriggerNode *trigger = GetNextTrigger(&watcher->triggerHead, NULL);
    while (trigger != NULL) {
        TriggerExtData *extData = TRIGGER_GET_EXT_DATA(trigger, TriggerExtData);
        PARAM_CHECK(extData != NULL, continue, "Failed to get trigger ext data");
        TriggerNode *next = GetNextTrigger(&watcher->triggerHead, trigger);
        if (extData->watcherId == watcherId) {
            FreeTrigger(trigger);
            break;
        }
        trigger = next;
    }
}

void ClearWatcherTrigger(const ParamWatcher *watcher)
{
    PARAM_CHECK(watcher != NULL, return, "Invalid watcher ");
    TriggerNode *trigger = GetNextTrigger(&watcher->triggerHead, NULL);
    while (trigger != NULL) {
        TriggerExtData *extData = TRIGGER_GET_EXT_DATA(trigger, TriggerExtData);
        PARAM_CHECK(extData != NULL, continue, "Failed to get trigger ext data");
        FreeTrigger(trigger);
        trigger = GetNextTrigger(&watcher->triggerHead, NULL);
    }
}

static void DumpTriggerQueue(const TriggerWorkSpace *workSpace, int index)
{
    const int maxCmd = 1024 * 64;
    PARAM_CHECK(workSpace != NULL, return, "Invalid workSpace ");
    TriggerNode *trigger = GetNextTrigger(&workSpace->triggerHead[index], NULL);
    while (trigger != NULL) {
        printf("trigger 0x%08x \n", trigger->flags);
        printf("trigger name %s \n", trigger->name);
        if (trigger->condition != NULL) {
            printf("trigger condition %s \n",  trigger->condition);
        }

        int count = 0;
        CommandNode *cmd = GetNextCmdNode(trigger, NULL);
        while (cmd != NULL && count < maxCmd) {
            printf("\t command name %s \n", GetCmdKey(cmd->cmdKeyIndex));
            printf("\t command args %s \n", cmd->content);
            cmd = GetNextCmdNode(trigger, cmd);
            count++;
        }
        trigger = GetNextTrigger(&workSpace->triggerHead[index], trigger);
    }
}

void DumpTrigger(const TriggerWorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return, "Invalid workSpace ");
    printf("Ready to dump all trigger memory \n");
    printf("workspace queue BOOT info:\n");
    DumpTriggerQueue(workSpace, TRIGGER_BOOT);
    printf("workspace queue parameter info:\n");
    DumpTriggerQueue(workSpace, TRIGGER_PARAM);
    printf("workspace queue other info:\n");
    DumpTriggerQueue(workSpace, TRIGGER_UNKNOW);

    printf("workspace queue execute info:\n");
    printf("queue info count: %u start: %u end: %u\n",
        workSpace->executeQueue.queueCount, workSpace->executeQueue.startIndex, workSpace->executeQueue.endIndex);
    for (uint32_t index = workSpace->executeQueue.startIndex; index < workSpace->executeQueue.endIndex; index++) {
        TriggerNode *trigger = workSpace->executeQueue.executeQueue[index % workSpace->executeQueue.queueCount];
        if (trigger != 0) {
            printf("queue node trigger name: %s \n", trigger->name);
        }
    }
}
