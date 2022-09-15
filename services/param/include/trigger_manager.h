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
#include "init_hashmap.h"
#include "list.h"
#include "param_message.h"
#include "param_utils.h"
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

typedef enum {
    TRIGGER_BOOT = 0,
    TRIGGER_PARAM,
    TRIGGER_UNKNOW,
    TRIGGER_PARAM_WAIT,
    TRIGGER_PARAM_WATCH,
    TRIGGER_MAX,
} TriggerType;

#define PARAM_TRIGGER_FOR_WAIT 0
#define PARAM_TRIGGER_FOR_WATCH 1

struct tagTriggerNode_;
struct TriggerWorkSpace_;
typedef struct TriggerExtInfo_ {
    int8_t type;
    ParamTaskPtr stream;
    union {
        char *name;
        struct {
            uint32_t watchId;
        } watchInfo;
        struct {
            uint32_t timeout;
            uint32_t waitId;
        } waitInfo;
    } info;
    int32_t (*addNode)(struct tagTriggerNode_ *, const struct TriggerExtInfo_ *);
} TriggerExtInfo;

typedef struct TriggerHeader_ {
    ListNode triggerList;
    uint32_t triggerCount;
    uint32_t cmdNodeCount;
    struct tagTriggerNode_ *(*addTrigger)(const struct TriggerWorkSpace_ *workSpace,
        const char *condition, const TriggerExtInfo *extInfo);
    struct tagTriggerNode_ *(*nextTrigger)(const struct TriggerHeader_ *, const struct tagTriggerNode_ *);
    int32_t (*executeTrigger)(const struct tagTriggerNode_ *trigger, const char *content, uint32_t size);

    int32_t (*checkAndMarkTrigger)(const struct TriggerWorkSpace_ *workSpace, int type, const char *name);
    int32_t (*checkTriggerMatch)(const struct TriggerWorkSpace_ *workSpace, int type,
        LogicCalculator *calculator, const char *content, uint32_t contentSize);
    int32_t (*checkCondition)(LogicCalculator *calculator,
        const char *condition, const char *content, uint32_t contentSize);

    const char *(*getTriggerName)(const struct tagTriggerNode_ *trigger);
    const char *(*getCondition)(const struct tagTriggerNode_ *trigger);
    void (*delTrigger)(const struct TriggerWorkSpace_ *workSpace, struct tagTriggerNode_ *trigger);
    void (*dumpTrigger)(const struct TriggerWorkSpace_ *workSpace,
        const struct tagTriggerNode_ *trigger);
    int32_t (*compareData)(const struct tagTriggerNode_ *trigger, const void *data);
} TriggerHeader;

typedef struct CommandNode_ {
    struct CommandNode_ *next;
    uint32_t cmdKeyIndex;
    char content[0];
} CommandNode;

#define NODE_BASE \
    ListNode node; \
    uint32_t flags : 24; \
    uint32_t type : 4; \
    char *condition

typedef struct tagTriggerNode_ {
    NODE_BASE;
} TriggerNode;

typedef struct tagTriggerJobNode_ {
    NODE_BASE;
    CommandNode *firstCmd;
    CommandNode *lastCmd;
    HashNode hashNode;
    char name[0];
} JobNode;

typedef struct WatchNode_ {
    NODE_BASE;
    ListNode item;
    uint32_t watchId;
} WatchNode;

typedef struct WaitNode_ {
    NODE_BASE;
    ListNode item;
    uint32_t timeout;
    uint32_t waitId;
    ParamTaskPtr stream;
} WaitNode;

typedef struct {
    uint32_t queueCount;
    uint32_t startIndex;
    uint32_t endIndex;
    TriggerNode **executeQueue;
} TriggerExecuteQueue;

typedef struct {
    ListHead triggerHead;
    ParamTaskPtr stream;
} ParamWatcher;

typedef struct TriggerWorkSpace_ {
    TriggerExecuteQueue executeQueue;
    TriggerHeader triggerHead[TRIGGER_MAX];
    HashMapHandle hashMap;
    ParamTaskPtr eventHandle;
    void (*bootStateChange)(int start, const char *);
    char cache[PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX];
} TriggerWorkSpace;

int InitTriggerWorkSpace(void);
void CloseTriggerWorkSpace(void);
TriggerWorkSpace *GetTriggerWorkSpace(void);
char *GetTriggerCache(uint32_t *size);
TriggerHeader *GetTriggerHeader(const TriggerWorkSpace *workSpace, int type);
void InitTriggerHead(const TriggerWorkSpace *workSpace);

int CheckTrigger(TriggerWorkSpace *workSpace, int type,
    const char *content, uint32_t contentSize, PARAM_CHECK_DONE triggerCheckDone);
int CheckAndMarkTrigger(int type, const char *name);

TriggerNode *ExecuteQueuePop(TriggerWorkSpace *workSpace);
int ExecuteQueuePush(TriggerWorkSpace *workSpace, const TriggerNode *trigger);

JobNode *UpdateJobTrigger(const TriggerWorkSpace *workSpace,
    int type, const char *condition, const char *name);
JobNode *GetTriggerByName(const TriggerWorkSpace *workSpace, const char *triggerName);
void FreeTrigger(const TriggerWorkSpace *workSpace, TriggerNode *trigger);
void ClearTrigger(const TriggerWorkSpace *workSpace, int8_t type);
int AddCommand(JobNode *trigger, uint32_t cmdIndex, const char *content);
CommandNode *GetNextCmdNode(const JobNode *trigger, const CommandNode *curr);

void PostParamTrigger(int type, const char *name, const char *value);

void ClearWatchTrigger(ParamWatcher *watcher, int type);
void DelWatchTrigger(int type, const void *data);
int CheckWatchTriggerTimeout(void);

const char *GetTriggerName(const TriggerNode *trigger);
void RegisterTriggerExec(int type, int32_t (*executeTrigger)(const TriggerNode *, const char *, uint32_t));

#ifdef STARTUP_INIT_TEST
void ProcessBeforeEvent(const ParamTaskPtr stream, uint64_t eventId, const uint8_t *content, uint32_t size);
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // STARTUP_TRIGER_MANAGER_H
