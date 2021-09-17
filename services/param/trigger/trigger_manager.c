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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_cmds.h"
#include "param_manager.h"
#include "trigger_checker.h"

#define LABEL "Trigger"
int AddCommand(TriggerNode *trigger, uint32_t cmdKeyIndex, const char *content)
{
    PARAM_CHECK(trigger != NULL, return -1, "trigger is null");
    uint32_t size = sizeof(CommandNode);
    size += (content == NULL || strlen(content) == 0) ? 1 : strlen(content) + 1;
    size = PARAM_ALIGN(size);

    CommandNode *node = (CommandNode *)malloc(size);
    PARAM_CHECK(node != NULL, return -1, "Failed to alloc memory for command");
    node->cmdKeyIndex = cmdKeyIndex;
    node->next = NULL;
    node->content[0] = '\0';
    if (content != NULL && strlen(content) != 0) {
        int ret = memcpy_s(node->content, size, content, strlen(content));
        node->content[strlen(content)] = '\0';
        PARAM_CHECK(ret == EOK, return 0, "Failed to copy command");
    }
    // 插入队列
    if (trigger->firstCmd == NULL) {
        trigger->firstCmd = node;
        trigger->lastCmd = node;
    } else {
        trigger->lastCmd->next = node;
        trigger->lastCmd = node;
    }
    trigger->triggerHead->cmdNodeCount++;
    return 0;
}

CommandNode *GetNextCmdNode(TriggerNode *trigger, CommandNode *curr)
{
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

    TriggerNode *node = (TriggerNode *)malloc(triggerNodeLen + conditionLen + extDataSize);
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
    ListNode *node = head->triggerList.next;
    while (node != &head->triggerList) {
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
        triggerHead->cmdNodeCount--;
        cmd = next;
    }
    trigger->lastCmd = NULL;
    trigger->firstCmd = NULL;
    ListRemove(&trigger->node);
    triggerHead->triggerCount--;

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

static TriggerNode *GetNextTrigger(TriggerHeader *triggerHead, TriggerNode *curr)
{
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

static const char *GetTriggerCondition(TriggerWorkSpace *workSpace, TriggerNode *trigger)
{
    return trigger->condition == NULL ? "" : trigger->condition;
}

TriggerNode *GetTriggerByName(TriggerWorkSpace *workSpace, const char *triggerName)
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

int ExecuteQueuePush(TriggerWorkSpace *workSpace, TriggerNode *trigger)
{
    PARAM_CHECK(workSpace != NULL, return -1, "Invalid area");
    uint32_t index = workSpace->executeQueue.endIndex++ % workSpace->executeQueue.queueCount;
    workSpace->executeQueue.executeQueue[index] = trigger;
    return 0;
}

TriggerNode *ExecuteQueuePop(TriggerWorkSpace *workSpace)
{
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

int ExecuteQueueSize(TriggerWorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return 0, "Invalid param");
    return workSpace->executeQueue.endIndex - workSpace->executeQueue.startIndex;
}

static int CheckBootTriggerMatch(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize)
{
    if (strncmp(trigger->name, (char *)content, contentSize) == 0) {
        return 1;
    }
    return 0;
}

static int CheckWatcherTriggerMatch(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize)
{
    if (strncmp(trigger->name, (char *)content, strlen(trigger->name)) == 0) {
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
    int ret = 0;
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

int MarkTriggerToParam(TriggerWorkSpace *workSpace, TriggerHeader *triggerHead, const char *name)
{
    int ret = 0;
    TriggerNode *trigger = GetNextTrigger(triggerHead, NULL);
    while (trigger != NULL) {
        const char *tmp = strstr(GetTriggerCondition(workSpace, trigger), name);
        if (tmp != NULL && strncmp(tmp + strlen(name), "=", 1) == 0) {
            TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_RELATED);
            ret = 1;
        }
        trigger = GetNextTrigger(triggerHead, trigger);
    }
    return ret;
}


ParamWatcher *GetNextParamWatcher(TriggerWorkSpace *workSpace, ParamWatcher *curr)
{
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

void DelWatcherTrigger(ParamWatcher *watcher, uint32_t watcherId)
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

void ClearWatcherTrigger(ParamWatcher *watcher)
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

#define DUMP_DEBUG PARAM_LOGD
static void DumpTriggerQueue(TriggerWorkSpace *workSpace, int index)
{
    TriggerNode *trigger = GetNextTrigger(&workSpace->triggerHead[index], NULL);
    while (trigger != NULL) {
        DUMP_DEBUG("trigger 0x%08x", trigger->flags);
        DUMP_DEBUG("trigger name %s ", trigger->name);
        DUMP_DEBUG("trigger condition %s ", trigger->condition);

        CommandNode *cmd = GetNextCmdNode(trigger, NULL);
        while (cmd != NULL) {
            DUMP_DEBUG("\t command name %s", GetCmdKey(cmd->cmdKeyIndex));
            DUMP_DEBUG("\t command args %s", cmd->content);
            cmd = GetNextCmdNode(trigger, cmd);
        }
        trigger = GetNextTrigger(&workSpace->triggerHead[index], trigger);
    }
}

void DumpTrigger(TriggerWorkSpace *workSpace)
{
    DUMP_DEBUG("Ready to dump all trigger memory");
    DUMP_DEBUG("workspace queue BOOT info:");
    DumpTriggerQueue(workSpace, TRIGGER_BOOT);
    DUMP_DEBUG("workspace queue parameter info:");
    DumpTriggerQueue(workSpace, TRIGGER_PARAM);
    DUMP_DEBUG("workspace queue other info:");
    DumpTriggerQueue(workSpace, TRIGGER_UNKNOW);

    DUMP_DEBUG("workspace queue execute info:");
    DUMP_DEBUG("queue info count: %u start: %u end: %u",
        workSpace->executeQueue.queueCount, workSpace->executeQueue.startIndex, workSpace->executeQueue.endIndex);
    for (uint32_t index = workSpace->executeQueue.startIndex; index < workSpace->executeQueue.endIndex; index++) {
        TriggerNode *trigger = workSpace->executeQueue.executeQueue[index % workSpace->executeQueue.queueCount];
        if (trigger != 0) {
            DUMP_DEBUG("queue node trigger name: %s ", trigger->name);
        }
    }
}
