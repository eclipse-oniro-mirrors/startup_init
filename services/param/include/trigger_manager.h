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

#ifndef STARTUP_TRIGER_MANAGER_H
#define STARTUP_TRIGER_MANAGER_H
#include <stdint.h>

#include "cJSON.h"
#include "list.h"
#include "param_message.h"
#include "param_utils.h"
#include "securec.h"
#include "trigger_checker.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define TRIGGER_CMD "trigger "
#define TRIGGER_ARR_NAME_IN_JSON "jobs"
#define CMDS_ARR_NAME_IN_JSON "cmds"
#define TRIGGER_MAX_CMD 4096

#define TRIGGER_EXECUTE_QUEUE 64
#define MAX_CONDITION_NUMBER 64

#define TRIGGER_FLAGS_QUEUE 0x01
#define TRIGGER_FLAGS_RELATED 0x02
#define TRIGGER_FLAGS_ONCE 0x04       // 执行完成后释放
#define TRIGGER_FLAGS_SUBTRIGGER 0x08 // 对init执行后，需要执行的init:xxx=aaa的trigger

#define CMD_INDEX_FOR_PARA_WAIT 0xfffE
#define CMD_INDEX_FOR_PARA_WATCH 0xffff
#define CMD_INDEX_FOR_PARA_TEST 0x10000

#define TRIGGER_IN_QUEUE(trigger) (((trigger)->flags & TRIGGER_FLAGS_QUEUE) == TRIGGER_FLAGS_QUEUE)
#define TRIGGER_SET_FLAG(trigger, flag) ((trigger)->flags |= (flag))
#define TRIGGER_CLEAR_FLAG(trigger, flag) ((trigger)->flags &= ~(flag))
#define TRIGGER_TEST_FLAG(trigger, flag) (((trigger)->flags & (flag)) == (flag))
#define TRIGGER_GET_EXT_DATA(trigger, TYPE) \
    (trigger)->extDataSize == 0 ? NULL : (TYPE *)(((char *)(trigger)) + (trigger)->extDataOffset)

typedef enum {
    TRIGGER_BOOT = 0,
    TRIGGER_PARAM,
    TRIGGER_UNKNOW,
    TRIGGER_MAX,
    TRIGGER_PARAM_WAIT,
    TRIGGER_PARAM_WATCH
} TriggerType;

#define PARAM_TRIGGER_FOR_WAIT 0
#define PARAM_TRIGGER_FOR_WATCH 1

typedef struct {
    ListNode triggerList;
    uint32_t triggerCount;
    uint32_t cmdNodeCount;
} TriggerHeader;

#define PARAM_TRIGGER_HEAD_INIT(head) \
    do {                              \
        ListInit(&(head).triggerList);  \
        (head).triggerCount = 0;        \
        (head).cmdNodeCount = 0;        \
    } while (0)

// Command对象列表，主要存储每个triger需要执行那些Command操作。
typedef struct CommandNode_ {
    struct CommandNode_ *next;
    uint32_t cmdKeyIndex;
    char content[0];
} CommandNode;

typedef struct tagTriggerNode_ {
    ListNode node;
    uint32_t flags : 24;
    uint32_t type : 8;
    TriggerHeader *triggerHead;
    CommandNode *firstCmd;
    CommandNode *lastCmd;
    uint16_t extDataOffset;
    uint16_t extDataSize;
    char *condition;
    char name[0];
} TriggerNode;

typedef struct {
    uint32_t queueCount;
    uint32_t startIndex;
    uint32_t endIndex;
    TriggerNode **executeQueue;
} TriggerExecuteQueue;

typedef struct {
    TriggerHeader triggerHead;
    ListNode node;
    uint32_t timeout;
    ParamTaskPtr stream;
} ParamWatcher;

typedef struct TriggerExtData_ {
    int (*excuteCmd)(const struct TriggerExtData_ *trigger, int cmd, const char *content);
    uint32_t watcherId;
    ParamWatcher *watcher;
} TriggerExtData;

typedef struct TriggerWorkSpace {
    void (*cmdExec)(const TriggerNode *trigger,
        const CommandNode *cmd, const char *content, uint32_t size);
    ParamTaskPtr eventHandle;
    char buffer[PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX];
    TriggerExecuteQueue executeQueue;
    TriggerHeader triggerHead[TRIGGER_MAX];
    ParamWatcher watcher;
    ListNode waitList;
} TriggerWorkSpace;

int InitTriggerWorkSpace(void);
void CloseTriggerWorkSpace(void);

typedef int (*TRIGGER_MATCH)(TriggerWorkSpace *workSpace, LogicCalculator *calculator,
    TriggerNode *trigger, const char *content, uint32_t contentSize);
int CheckTrigger(TriggerWorkSpace *workSpace, int type,
    const char *content, uint32_t contentSize, PARAM_CHECK_DONE triggerExecuter);
int MarkTriggerToParam(const TriggerWorkSpace *workSpace, const TriggerHeader *triggerHead, const char *name);
int CheckAndMarkTrigger(int type, const char *name);

TriggerNode *ExecuteQueuePop(TriggerWorkSpace *workSpace);
int ExecuteQueuePush(TriggerWorkSpace *workSpace, const TriggerNode *trigger);

TriggerNode *AddTrigger(TriggerHeader *triggerHead, const char *name, const char *condition, uint16_t extDataSize);
TriggerNode *GetTriggerByName(const TriggerWorkSpace *workSpace, const char *triggerName);
void FreeTrigger(TriggerNode *trigger);
void ClearTrigger(TriggerHeader *head);

int AddCommand(TriggerNode *trigger, uint32_t cmdIndex, const char *content);
CommandNode *GetNextCmdNode(const TriggerNode *trigger, const CommandNode *curr);

void DumpTrigger(const TriggerWorkSpace *workSpace);
void PostParamTrigger(int type, const char *name, const char *value);

ParamWatcher *GetParamWatcher(const ParamTaskPtr worker);
ParamWatcher *GetNextParamWatcher(const TriggerWorkSpace *workSpace, const ParamWatcher *curr);
TriggerNode *AddWatcherTrigger(ParamWatcher *watcher,
    int triggerType, const char *name, const char *condition, const TriggerExtData *extData);
void DelWatcherTrigger(const ParamWatcher *watcher, uint32_t watcherId);
void ClearWatcherTrigger(const ParamWatcher *watcher);

TriggerWorkSpace *GetTriggerWorkSpace(void);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // STARTUP_TRIGER_MANAGER_H
