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

#include <string.h>
#include <sys/types.h>

#include "init_cmds.h"
#include "param_manager.h"
#include "trigger_checker.h"
#include "securec.h"

static DUMP_PRINTF g_printf = printf;

int AddCommand(JobNode *trigger, uint32_t cmdKeyIndex, const char *content)
{
    PARAM_CHECK(trigger != NULL, return -1, "trigger is null");
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

    if (trigger->firstCmd == NULL) {
        trigger->firstCmd = node;
        trigger->lastCmd = node;
    } else {
        PARAM_CHECK(trigger->lastCmd != NULL, free(node);
            return 0, "Invalid last cmd");
        trigger->lastCmd->next = node;
        trigger->lastCmd = node;
    }
    return 0;
}

CommandNode *GetNextCmdNode(const JobNode *trigger, const CommandNode *curr)
{
    PARAM_CHECK(trigger != NULL, return NULL, "trigger is null");
    if (curr == NULL) {
        return trigger->firstCmd;
    }
    return curr->next;
}

static int CopyCondition(TriggerNode *node, const char *condition)
{
    if (condition == NULL || strlen(condition) == 0) {
        return 0;
    }
    uint32_t buffSize = 0;
    char *cond = GetTriggerCache(&buffSize);
    int ret = ConvertInfixToPrefix(condition, cond, buffSize);
    PARAM_CHECK(ret == 0, return -1, "Failed to convert condition for trigger");
    node->condition = strdup(cond);
    PARAM_CHECK(node->condition != NULL, return -1, "Failed to dup conditition");
    return 0;
}

static TriggerNode *AddTriggerNode_(TriggerHeader *triggerHead,
    uint32_t type, const char *condition, uint32_t dataSize)
{
    TriggerNode *node = (TriggerNode *)calloc(1, dataSize);
    PARAM_CHECK(node != NULL, return NULL, "Failed to alloc memory for trigger");
    node->condition = NULL;
    int ret = CopyCondition(node, condition);
    PARAM_CHECK(ret == 0, free(node);
            return NULL, "Failed to copy conditition");
    node->type = type;
    node->flags = 0;
    OH_ListInit(&node->node);
    OH_ListAddTail(&triggerHead->triggerList, &node->node);
    triggerHead->triggerCount++;
    return node;
}

static int32_t AddJobNode_(TriggerNode *trigger, const TriggerExtInfo *extInfo)
{
    JobNode *node = (JobNode *)trigger;
    int ret = strcpy_s(node->name, strlen(extInfo->info.name) + 1, extInfo->info.name);
    PARAM_CHECK(ret == EOK, return -1, "Failed to copy name for trigger");
    node->firstCmd = NULL;
    node->lastCmd = NULL;
    ret = OH_HashMapAdd(GetTriggerWorkSpace()->hashMap, &node->hashNode);
    PARAM_CHECK(ret == 0, return -1, "Failed to add hash node");
    return 0;
}

static TriggerNode *AddJobTrigger_(const TriggerWorkSpace *workSpace,
    const char *condition, const TriggerExtInfo *extInfo)
{
    PARAM_CHECK(workSpace != NULL, return NULL, "workSpace is null");
    PARAM_CHECK(extInfo != NULL && extInfo->addNode != NULL, return NULL, "extInfo is null");
    PARAM_CHECK(extInfo->type <= TRIGGER_UNKNOW, return NULL, "Invalid type");
    TriggerHeader *triggerHead = GetTriggerHeader(workSpace, extInfo->type);
    PARAM_CHECK(triggerHead != NULL, return NULL, "Failed to get header %d", extInfo->type);
    uint32_t nameLen = strlen(extInfo->info.name);
    uint32_t triggerNodeLen = PARAM_ALIGN(nameLen + 1) + sizeof(JobNode);
    TriggerNode *node = (TriggerNode *)AddTriggerNode_(triggerHead, extInfo->type, condition, triggerNodeLen);
    PARAM_CHECK(node != NULL, return NULL, "Failed to alloc jobnode");
    int ret = extInfo->addNode(node, extInfo);
    PARAM_CHECK(ret == 0, FreeTrigger(workSpace, node);
        return NULL, "Failed to add hash node");
    if (extInfo->type == TRIGGER_BOOT) {
        TRIGGER_SET_FLAG(node, TRIGGER_FLAGS_ONCE);
        if (strncmp("boot-service:", extInfo->info.name, strlen("boot-service:")) != 0) {
            TRIGGER_SET_FLAG(node, TRIGGER_FLAGS_SUBTRIGGER);
        }
    }
    return node;
}

static void DelJobTrigger_(const TriggerWorkSpace *workSpace, TriggerNode *trigger)
{
    PARAM_CHECK(workSpace != NULL, return, "Param is null");
    PARAM_CHECK(trigger != NULL, return, "Trigger is null");
    JobNode *jobNode = (JobNode *)trigger;
    TriggerHeader *triggerHead = GetTriggerHeader(workSpace, trigger->type);
    PARAM_CHECK(triggerHead != NULL, return, "Failed to get header %d", trigger->type);
    CommandNode *cmd = jobNode->firstCmd;
    while (cmd != NULL) {
        CommandNode *next = cmd->next;
        free(cmd);
        triggerHead->cmdNodeCount--;
        cmd = next;
    }
    if (jobNode->condition != NULL) {
        free(jobNode->condition);
        jobNode->condition = NULL;
    }
    jobNode->lastCmd = NULL;
    jobNode->firstCmd = NULL;
    OH_ListRemove(&trigger->node);
    triggerHead->triggerCount--;
    OH_HashMapRemove(workSpace->hashMap, jobNode->name);

    if (!TRIGGER_IN_QUEUE(trigger)) {
        free(jobNode);
        return;
    }
    TriggerExecuteQueue *executeQueue = (TriggerExecuteQueue *)&workSpace->executeQueue;
    for (uint32_t i = executeQueue->startIndex; i < executeQueue->endIndex; i++) {
        if (executeQueue->executeQueue[i] == trigger) {
            executeQueue->executeQueue[i] = NULL;
            break;
        }
    }
    free(jobNode);
}

static TriggerNode *AddWatchTrigger_(const TriggerWorkSpace *workSpace,
    const char *condition, const TriggerExtInfo *extInfo)
{
    PARAM_CHECK(workSpace != NULL, return NULL, "workSpace is null");
    PARAM_CHECK(extInfo != NULL && extInfo->addNode != NULL, return NULL, "extInfo is null");
    TriggerHeader *triggerHead = GetTriggerHeader(workSpace, extInfo->type);
    PARAM_CHECK(triggerHead != NULL, return NULL, "Failed to get header %d", extInfo->type);
    uint32_t size = 0;
    if (extInfo->type == TRIGGER_PARAM_WATCH) {
        size = sizeof(WatchNode);
    } else if (extInfo->type == TRIGGER_PARAM_WAIT) {
        size = sizeof(WaitNode);
    } else {
        PARAM_LOGE("Invalid trigger type %d", extInfo->type);
        return NULL;
    }
    TriggerNode *node = AddTriggerNode_(triggerHead, extInfo->type, condition, size);
    PARAM_CHECK(node != NULL, return NULL, "Failed to alloc memory for trigger");
    int ret = extInfo->addNode(node, extInfo);
    PARAM_CHECK(ret == 0, FreeTrigger(workSpace, node);
        return NULL, "Failed to add node");
    if (extInfo->type == TRIGGER_PARAM_WAIT) {
        TRIGGER_SET_FLAG(node, TRIGGER_FLAGS_ONCE);
    }
    return node;
}

static void DelWatchTrigger_(const TriggerWorkSpace *workSpace, TriggerNode *trigger)
{
    PARAM_CHECK(workSpace != NULL, return, "Param is null");
    TriggerHeader *triggerHead = GetTriggerHeader(workSpace, trigger->type);
    PARAM_CHECK(triggerHead != NULL, return, "Failed to get header %d", trigger->type);
    OH_ListRemove(&trigger->node);
    if (trigger->type == TRIGGER_PARAM_WAIT) {
        WaitNode *node = (WaitNode *)trigger;
        OH_ListRemove(&node->item);
    } else if (trigger->type == TRIGGER_PARAM_WATCH) {
        WatchNode *node = (WatchNode *)trigger;
        OH_ListRemove(&node->item);
    }
    PARAM_LOGV("DelWatchTrigger_ %s count %d", GetTriggerName(trigger), triggerHead->triggerCount);
    triggerHead->triggerCount--;
    free(trigger);
}

static TriggerNode *GetNextTrigger_(const TriggerHeader *triggerHead, const TriggerNode *curr)
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

static const char *GetTriggerCondition_(const TriggerNode *trigger)
{
    return (trigger == NULL || trigger->condition == NULL) ? "" : trigger->condition;
}

static const char *GetBootCondition_(const TriggerNode *trigger)
{
    PARAM_CHECK(trigger != NULL, return "", "Invalid trigger");
    PARAM_CHECK(trigger->type == TRIGGER_BOOT, return "", "Invalid type");
    const JobNode *node = (const JobNode *)trigger;
    return node->name;
}

static const char *GetJobName_(const TriggerNode *trigger)
{
    PARAM_CHECK(trigger != NULL, return "", "Invalid trigger");
    PARAM_CHECK(trigger->type <= TRIGGER_UNKNOW, return "", "Invalid type");
    const JobNode *node = (const JobNode *)trigger;
    return node->name;
}

static const char *GetWatchName_(const TriggerNode *trigger)
{
    PARAM_CHECK(trigger != NULL, return "", "Invalid trigger");
    PARAM_CHECK(trigger->type < TRIGGER_MAX && trigger->type > TRIGGER_UNKNOW,
        return "", "Invalid type");
    return trigger->condition;
}

JobNode *UpdateJobTrigger(const TriggerWorkSpace *workSpace,
    int type, const char *condition, const char *name)
{
    PARAM_CHECK(workSpace != NULL && name != NULL, return NULL, "name is null");
    PARAM_CHECK(type <= TRIGGER_UNKNOW, return NULL, "Invalid type");
    TriggerHeader *triggerHead = GetTriggerHeader(workSpace, type);
    PARAM_CHECK(triggerHead != NULL, return NULL, "Failed to get header %d", type);
    JobNode *jobNode = GetTriggerByName(workSpace, name);
    if (jobNode == NULL) {
        TriggerExtInfo extInfo = {};
        extInfo.info.name = (char *)name;
        extInfo.type = type;
        extInfo.addNode = AddJobNode_;
        return (JobNode *)triggerHead->addTrigger(workSpace, condition, &extInfo);
    } else if (jobNode->condition == NULL && condition != NULL) {
        int ret = CopyCondition((TriggerNode *)jobNode, condition);
        PARAM_CHECK(ret == 0, FreeTrigger(workSpace, (TriggerNode*)jobNode);
            return NULL, "Failed to copy conditition");
    }
    return jobNode;
}

JobNode *GetTriggerByName(const TriggerWorkSpace *workSpace, const char *triggerName)
{
    PARAM_CHECK(workSpace != NULL && triggerName != NULL, return NULL, "Invalid param");
    HashNode *node = OH_HashMapGet(workSpace->hashMap, triggerName);
    if (node == NULL) {
        return NULL;
    }
    JobNode *trigger = HASHMAP_ENTRY(node, JobNode, hashNode);
    return trigger;
}

void FreeTrigger(const TriggerWorkSpace *workSpace, TriggerNode *trigger)
{
    PARAM_CHECK(workSpace != NULL && trigger != NULL, return, "Invalid param");
    TriggerHeader *head = GetTriggerHeader(workSpace, trigger->type);
    if (head != NULL) {
        head->delTrigger(workSpace, trigger);
    }
}

void ClearTrigger(const TriggerWorkSpace *workSpace, int8_t type)
{
    PARAM_CHECK(workSpace != NULL, return, "head is null");
    TriggerHeader *head = GetTriggerHeader(workSpace, type);
    PARAM_CHECK(head != NULL, return, "Failed to get header %d", type);
    TriggerNode *trigger = head->nextTrigger(head, NULL);
    while (trigger != NULL) {
        TriggerNode *next = head->nextTrigger(head, trigger);
        FreeTrigger(workSpace, trigger);
        trigger = next;
    }
    OH_ListInit(&head->triggerList);
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

static int CheckBootCondition_(LogicCalculator *calculator,
    const char *condition, const char *content, uint32_t contentSize)
{
    UNUSED(calculator);
    if (strncmp(condition, content, contentSize) == 0) {
        return 1;
    }
    return 0;
}

static int CheckWatchCondition_(LogicCalculator *calculator,
    const char *condition, const char *content, uint32_t contentSize)
{
    UNUSED(calculator);
    UNUSED(contentSize);
    if (strncmp(condition, content, strlen(condition)) == 0) {
        return 1;
    }
    return 0;
}

static int CheckParamCondition_(LogicCalculator *calculator,
    const char *condition, const char *content, uint32_t contentSize)
{
    UNUSED(content);
    UNUSED(contentSize);
    if (calculator->inputName != NULL) {
        if (!CheckMatchSubCondition(condition, calculator->inputName, strlen(calculator->inputName))) {
            return 0;
        }
    }
    return ComputeCondition(calculator, condition);
}

static int CheckUnknowCondition_(LogicCalculator *calculator,
    const char *condition, const char *content, uint32_t contentSize)
{
    if (condition != NULL && content != NULL && strcmp(content, condition) == 0) {
        return 1;
    }
    return ComputeCondition(calculator, condition);
}

static int ExecTriggerMatch_(const TriggerWorkSpace *workSpace,
    int type, LogicCalculator *calculator, const char *content, uint32_t contentSize)
{
    TriggerHeader *head = GetTriggerHeader(workSpace, type);
    PARAM_CHECK(head != NULL, return 0, "Failed to get header %d", type);
    TriggerNode *trigger = head->nextTrigger(head, NULL);
    while (trigger != NULL) {
        TriggerNode *next = head->nextTrigger(head, trigger);
        const char *condition = head->getCondition(trigger);
        if (head->checkCondition(calculator, condition, content, contentSize) == 1) {
            calculator->triggerCheckDone(trigger, content, contentSize);
        }
        trigger = next;
    }
    return 0;
}

static int CheckBootMatch_(const TriggerWorkSpace *workSpace,
    int type, LogicCalculator *calculator, const char *content, uint32_t contentSize)
{
    PARAM_CHECK(workSpace != NULL, return -1, "Invalid space");
    PARAM_CHECK((type == TRIGGER_BOOT) || (type == TRIGGER_PARAM_WATCH), return -1, "Invalid type");
    return ExecTriggerMatch_(workSpace, type, calculator, content, contentSize);
}

static int CheckParamMatch_(const TriggerWorkSpace *workSpace,
    int type, LogicCalculator *calculator, const char *content, uint32_t contentSize)
{
    PARAM_CHECK(workSpace != NULL, return -1, "Invalid space");
    PARAM_CHECK((type == TRIGGER_PARAM) || (type == TRIGGER_PARAM_WAIT), return -1, "Invalid type");

    CalculatorInit(calculator, MAX_CONDITION_NUMBER, sizeof(LogicData), 1);
    int ret = GetValueFromContent(content, contentSize, 0, calculator->inputName, SUPPORT_DATA_BUFFER_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed parse content name");
    ret = GetValueFromContent(content, contentSize,
        strlen(calculator->inputName) + 1, calculator->inputContent, SUPPORT_DATA_BUFFER_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed parse content value");
    return ExecTriggerMatch_(workSpace, type, calculator, content, contentSize);
}

static int CheckUnknowMatch_(const TriggerWorkSpace *workSpace,
    int type, LogicCalculator *calculator, const char *content, uint32_t contentSize)
{
    PARAM_CHECK(workSpace != NULL && content != NULL, return -1, "Failed arg for trigger");
    PARAM_CHECK(type == TRIGGER_UNKNOW, return -1, "Invalid type");

    CalculatorInit(calculator, MAX_CONDITION_NUMBER, sizeof(LogicData), 1);
    int ret = memcpy_s(calculator->triggerContent, sizeof(calculator->triggerContent), content, contentSize);
    PARAM_CHECK(ret == EOK, return -1, "Failed to memcpy");
    calculator->inputName = NULL;
    calculator->inputContent = NULL;
    return ExecTriggerMatch_(workSpace, type, calculator, content, contentSize);
}

int32_t CheckAndMarkTrigger_(const TriggerWorkSpace *workSpace, int type, const char *name)
{
    PARAM_CHECK(workSpace != NULL && name != NULL, return 0, "Failed arg for trigger");
    TriggerHeader *head = GetTriggerHeader(workSpace, type);
    PARAM_CHECK(head != NULL, return 0, "Failed to get header %d", type);
    int ret = 0;
    TriggerNode *trigger = head->nextTrigger(head, NULL);
    while (trigger != NULL) {
        if (head->getCondition(trigger) == NULL) {
            trigger = head->nextTrigger(head, trigger);
            continue;
        }
        if (CheckMatchSubCondition(head->getCondition(trigger), name, strlen(name)) == 1) {
            TRIGGER_SET_FLAG(trigger, TRIGGER_FLAGS_RELATED);
            ret = 1;
        }
        trigger = head->nextTrigger(head, trigger);
    }
    return ret;
}

int CheckTrigger(TriggerWorkSpace *workSpace, int type,
    const char *content, uint32_t contentSize, PARAM_CHECK_DONE triggerCheckDone)
{
    PARAM_CHECK(workSpace != NULL && content != NULL && triggerCheckDone != NULL,
        return -1, "Failed arg for trigger");
    PARAM_LOGV("CheckTrigger_ type: %d content: %s ", type, content);
    TriggerHeader *triggerHead = GetTriggerHeader(workSpace, type);
    if (triggerHead != NULL) {
        LogicCalculator calculator = {{0}};
        calculator.triggerCheckDone = triggerCheckDone;
        int ret = triggerHead->checkTriggerMatch(workSpace, type, &calculator, content, contentSize);
        CalculatorFree(&calculator);
        return ret;
    }
    return 0;
}

static void DumpJobTrigger_(const TriggerWorkSpace *workSpace, const TriggerNode *trigger)
{
    const JobNode *node = (const JobNode *)trigger;
    PARAM_DUMP("trigger     flags: 0x%08x \n", trigger->flags);
    PARAM_DUMP("trigger      name: %s \n", node->name);
    PARAM_DUMP("trigger condition: %s \n", node->condition);
    const int maxCmd = 1024;
    int count = 0;
    CommandNode *cmd = GetNextCmdNode(node, NULL);
    while (cmd != NULL && count < maxCmd) {
        PARAM_DUMP("    command name: %s \n", GetCmdKey(cmd->cmdKeyIndex));
        PARAM_DUMP("    command args: %s \n", cmd->content);
        cmd = GetNextCmdNode(node, cmd);
        count++;
    }
}

static void DumpWatchTrigger_(const TriggerWorkSpace *workSpace, const TriggerNode *trigger)
{
    const WatchNode *node = (const WatchNode *)trigger;
    PARAM_DUMP("trigger     flags: 0x%08x \n", trigger->flags);
    PARAM_DUMP("trigger condition: %s \n", trigger->condition);
    PARAM_DUMP("trigger   watchId: %d \n", node->watchId);
}

static void DumpWaitTrigger_(const TriggerWorkSpace *workSpace, const TriggerNode *trigger)
{
    const WaitNode *node = (const WaitNode *)trigger;
    PARAM_DUMP("trigger     flags: 0x%08x \n", trigger->flags);
    PARAM_DUMP("trigger      name: %s \n", GetTriggerName(trigger));
    PARAM_DUMP("trigger condition: %s \n", trigger->condition);
    PARAM_DUMP("trigger    waitId: %d \n", node->waitId);
    PARAM_DUMP("trigger   timeout: %d \n", node->timeout);
}

static void DumpTrigger_(const TriggerWorkSpace *workSpace, int type)
{
    PARAM_CHECK(workSpace != NULL, return, "Invalid workSpace ");
    TriggerHeader *head = GetTriggerHeader(workSpace, type);
    PARAM_CHECK(head != NULL, return, "Failed to get header %d", type);
    TriggerNode *trigger = head->nextTrigger(head, NULL);
    while (trigger != NULL) {
        head->dumpTrigger(workSpace, trigger);
        trigger = head->nextTrigger(head, trigger);
    }
}

void SystemDumpTriggers(int verbose, int (*dump)(const char *fmt, ...))
{
    if (dump != NULL) {
        g_printf = dump;
    } else {
        g_printf = printf;
    }
    TriggerWorkSpace *workSpace = GetTriggerWorkSpace();
    PARAM_CHECK(workSpace != NULL, return, "Invalid workSpace ");
    PARAM_DUMP("workspace queue BOOT info:\n");
    DumpTrigger_(workSpace, TRIGGER_BOOT);
    PARAM_DUMP("workspace queue parameter info:\n");
    DumpTrigger_(workSpace, TRIGGER_PARAM);
    PARAM_DUMP("workspace queue other info:\n");
    DumpTrigger_(workSpace, TRIGGER_UNKNOW);
    PARAM_DUMP("workspace queue watch info:\n");
    DumpTrigger_(workSpace, TRIGGER_PARAM_WATCH);
    PARAM_DUMP("workspace queue wait info:\n");
    DumpTrigger_(workSpace, TRIGGER_PARAM_WAIT);

    PARAM_DUMP("workspace queue execute info:\n");
    PARAM_DUMP("queue info count: %u start: %u end: %u\n",
        workSpace->executeQueue.queueCount, workSpace->executeQueue.startIndex, workSpace->executeQueue.endIndex);
    for (uint32_t index = workSpace->executeQueue.startIndex; index < workSpace->executeQueue.endIndex; index++) {
        TriggerNode *trigger = workSpace->executeQueue.executeQueue[index % workSpace->executeQueue.queueCount];
        if (trigger != 0) {
            PARAM_DUMP("    queue node trigger name: %s \n", GetTriggerName(trigger));
        }
    }
}

static int32_t CompareData_(const struct tagTriggerNode_ *trigger, const void *data)
{
    PARAM_CHECK(trigger != NULL && data != NULL, return -1, "Invalid trigger");
    if (trigger->type == TRIGGER_PARAM_WAIT) {
        WaitNode *node = (WaitNode *)trigger;
        return node->waitId - *(uint32_t *)data;
    } else if (trigger->type == TRIGGER_PARAM_WATCH) {
        WatchNode *node = (WatchNode *)trigger;
        return node->watchId - *(uint32_t *)data;
    }
    return -1;
}

static void TriggerHeadSetDefault(TriggerHeader *head)
{
    OH_ListInit(&head->triggerList);
    head->triggerCount = 0;
    head->cmdNodeCount = 0;
    head->addTrigger = AddJobTrigger_;
    head->nextTrigger = GetNextTrigger_;
    head->delTrigger = DelJobTrigger_;
    head->executeTrigger = NULL;
    head->checkAndMarkTrigger = CheckAndMarkTrigger_;
    head->checkTriggerMatch = CheckBootMatch_;
    head->checkCondition = CheckBootCondition_;
    head->getCondition = GetBootCondition_;
    head->getTriggerName = GetJobName_;
    head->dumpTrigger = DumpJobTrigger_;
    head->compareData = CompareData_;
}

static int JobNodeNodeCompare(const HashNode *node1, const HashNode *node2)
{
    JobNode *jobNode1 = HASHMAP_ENTRY(node1, JobNode, hashNode);
    JobNode *jobNode2 = HASHMAP_ENTRY(node2, JobNode, hashNode);
    return strcmp(jobNode1->name, jobNode2->name);
}

static int JobNodeKeyCompare(const HashNode *node1, const void *key)
{
    JobNode *jobNode1 = HASHMAP_ENTRY(node1, JobNode, hashNode);
    return strcmp(jobNode1->name, (char *)key);
}

static int JobNodeGetNodeHasCode(const HashNode *node)
{
    JobNode *jobNode = HASHMAP_ENTRY(node, JobNode, hashNode);
    int code = 0;
    size_t nameLen = strlen(jobNode->name);
    for (size_t i = 0; i < nameLen; i++) {
        code += jobNode->name[i] - 'A';
    }
    return code;
}

static int JobNodeGetKeyHasCode(const void *key)
{
    int code = 0;
    const char *buff = (char *)key;
    size_t buffLen = strlen(buff);
    for (size_t i = 0; i < buffLen; i++) {
        code += buff[i] - 'A';
    }
    return code;
}

static void JobNodeFree(const HashNode *node)
{
    JobNode *jobNode = HASHMAP_ENTRY(node, JobNode, hashNode);
    FreeTrigger(GetTriggerWorkSpace(), (TriggerNode *)jobNode);
}

void InitTriggerHead(const TriggerWorkSpace *workSpace)
{
    HashInfo info = {
        JobNodeNodeCompare,
        JobNodeKeyCompare,
        JobNodeGetNodeHasCode,
        JobNodeGetKeyHasCode,
        JobNodeFree,
        64
    };
    PARAM_CHECK(workSpace != NULL, return, "Invalid workSpace");
    int ret = OH_HashMapCreate((HashMapHandle *)&workSpace->hashMap, &info);
    PARAM_CHECK(ret == 0, return, "Failed to create hash map");

    TriggerHeader *head = (TriggerHeader *)&workSpace->triggerHead[TRIGGER_BOOT];
    TriggerHeadSetDefault(head);
    // param trigger
    head = (TriggerHeader *)&workSpace->triggerHead[TRIGGER_PARAM];
    TriggerHeadSetDefault(head);
    head->checkTriggerMatch = CheckParamMatch_;
    head->checkCondition = CheckParamCondition_;
    head->getCondition = GetTriggerCondition_;
    // unknown trigger
    head = (TriggerHeader *)&workSpace->triggerHead[TRIGGER_UNKNOW];
    TriggerHeadSetDefault(head);
    head->checkTriggerMatch = CheckUnknowMatch_;
    head->checkCondition = CheckUnknowCondition_;
    head->getCondition = GetTriggerCondition_;
    // wait trigger
    head = (TriggerHeader *)&workSpace->triggerHead[TRIGGER_PARAM_WAIT];
    TriggerHeadSetDefault(head);
    head->addTrigger = AddWatchTrigger_;
    head->delTrigger = DelWatchTrigger_;
    head->checkTriggerMatch = CheckParamMatch_;
    head->checkCondition = CheckParamCondition_;
    head->getCondition = GetTriggerCondition_;
    head->dumpTrigger = DumpWaitTrigger_;
    head->getTriggerName = GetWatchName_;
    // watch trigger
    head = (TriggerHeader *)&workSpace->triggerHead[TRIGGER_PARAM_WATCH];
    TriggerHeadSetDefault(head);
    head->addTrigger = AddWatchTrigger_;
    head->delTrigger = DelWatchTrigger_;
    head->checkTriggerMatch = CheckBootMatch_;
    head->checkCondition = CheckWatchCondition_;
    head->getCondition = GetTriggerCondition_;
    head->dumpTrigger = DumpWatchTrigger_;
    head->getTriggerName = GetWatchName_;
}

void DelWatchTrigger(int type, const void *data)
{
    PARAM_CHECK(data != NULL, return, "Invalid data");
    TriggerHeader *head = GetTriggerHeader(GetTriggerWorkSpace(), type);
    PARAM_CHECK(head != NULL, return, "Failed to get header %d", type);
    PARAM_CHECK(head->compareData != NULL, return, "Invalid compareData");
    TriggerNode *trigger = head->nextTrigger(head, NULL);
    while (trigger != NULL) {
        if (head->compareData(trigger, data) == 0) {
            head->delTrigger(GetTriggerWorkSpace(), trigger);
            return;
        }
        trigger = head->nextTrigger(head, trigger);
    }
}

void ClearWatchTrigger(ParamWatcher *watcher, int type)
{
    PARAM_CHECK(watcher != NULL, return, "Invalid watcher");
    TriggerHeader *head = GetTriggerHeader(GetTriggerWorkSpace(), type);
    PARAM_CHECK(head != NULL, return, "Failed to get header %d", type);
    ListNode *node = watcher->triggerHead.next;
    while (node != &watcher->triggerHead) {
        TriggerNode *trigger = NULL;
        if (type == TRIGGER_PARAM_WAIT) {
            trigger = (TriggerNode *)ListEntry(node, WaitNode, item);
        } else if (type == TRIGGER_PARAM_WATCH) {
            trigger = (TriggerNode *)ListEntry(node, WatchNode, item);
        }
        if (trigger == NULL || type != trigger->type) {
            PARAM_LOGE("ClearWatchTrigger %s error type %d", GetTriggerName(trigger), type);
            return;
        }
        PARAM_LOGV("ClearWatchTrigger %s", GetTriggerName(trigger));
        ListNode *next = node->next;
        FreeTrigger(GetTriggerWorkSpace(), trigger);
        node = next;
    }
}

int CheckWatchTriggerTimeout(void)
{
    TriggerHeader *head = GetTriggerHeader(GetTriggerWorkSpace(), TRIGGER_PARAM_WAIT);
    PARAM_CHECK(head != NULL && head->nextTrigger != NULL, return 0, "Invalid header");
    int hasNode = 0;
    WaitNode *node = (WaitNode *)head->nextTrigger(head, NULL);
    while (node != NULL) {
        WaitNode *next = (WaitNode *)head->nextTrigger(head, (TriggerNode *)node);
        if (node->timeout > 0) {
            node->timeout--;
        } else {
            head->executeTrigger((TriggerNode*)node, NULL, 0);
            FreeTrigger(GetTriggerWorkSpace(), (TriggerNode *)node);
        }
        hasNode = 1;
        node = next;
    }
    return hasNode;
}

TriggerHeader *GetTriggerHeader(const TriggerWorkSpace *workSpace, int type)
{
    if (workSpace == NULL || type >= TRIGGER_MAX) {
        return NULL;
    }
    return (TriggerHeader *)&workSpace->triggerHead[type];
}

char *GetTriggerCache(uint32_t *size)
{
    TriggerWorkSpace *space = GetTriggerWorkSpace();
    if (space == NULL) {
        return NULL;
    }
    if (size != NULL) {
        *size = sizeof(space->cache) / sizeof(space->cache[0]);
    }
    return space->cache;
}

const char *GetTriggerName(const TriggerNode *trigger)
{
    PARAM_CHECK(trigger != NULL, return "", "Invalid trigger");
    TriggerHeader *triggerHead = GetTriggerHeader(GetTriggerWorkSpace(), trigger->type);
    if (triggerHead) {
        return triggerHead->getTriggerName(trigger);
    }
    return "";
}
