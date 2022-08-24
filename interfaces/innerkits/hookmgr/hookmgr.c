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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "beget_ext.h"
#include "hookmgr.h"

// Forward declaration
typedef struct tagHOOK_STAGE HOOK_STAGE;

/*
 * Internal HOOK Item with priorities
 */
typedef struct tagHOOK_ITEM {
    ListNode node;
    HOOK_INFO info;
    HOOK_STAGE *stage;
} HOOK_ITEM;

/*
 * Internal HOOK Stage in the same stage
 */
struct tagHOOK_STAGE {
    ListNode node;
    int stage;
    ListNode hooks;
};

/*
 * HookManager is consist of different hook stages
 */
struct tagHOOK_MGR {
    const char *name;
    ListNode stages;
};

/*
 * Default HookManager is created automatically for HookMgrAddHook
 */
static HOOK_MGR *defaultHookMgr = NULL;

static HOOK_MGR *getHookMgr(HOOK_MGR *hookMgr, int autoCreate)
{
    BEGET_CHECK(hookMgr == NULL, return hookMgr);
    // Use default HOOK_MGR if possible
    BEGET_CHECK(defaultHookMgr == NULL, return defaultHookMgr);

    BEGET_CHECK(autoCreate, return NULL);

    // Create default HOOK_MGR if not created
    defaultHookMgr = HookMgrCreate("default");
    return defaultHookMgr;
}

static int hookStageCompare(ListNode *node, void *data)
{
    const HOOK_STAGE *stage;
    int compareStage = *((int *)data);

    stage = (const HOOK_STAGE *)node;
    return (stage->stage - compareStage);
}

static void hookStageDestroy(ListNode *node)
{
    HOOK_STAGE *stage;

    BEGET_CHECK(node != NULL, return);

    stage = (HOOK_STAGE *)node;
    OH_ListRemoveAll(&(stage->hooks), NULL);
    free((void *)stage);
}

// Get HOOK_STAGE if found, otherwise create it
static HOOK_STAGE *getHookStage(HOOK_MGR *hookMgr, int stage, int createIfNotFound)
{
    HOOK_STAGE *stageItem;

    stageItem = (HOOK_STAGE *)OH_ListFind(&(hookMgr->stages), (void *)(&stage), hookStageCompare);
    BEGET_CHECK(stageItem == NULL, return stageItem);

    BEGET_CHECK(createIfNotFound, return NULL);

    // Not found, create it
    stageItem = (HOOK_STAGE *)malloc(sizeof(HOOK_STAGE));
    BEGET_CHECK(stageItem != NULL, return NULL);
    stageItem->stage = stage;
    OH_ListInit(&(stageItem->hooks));
    OH_ListAddTail(&(hookMgr->stages), (ListNode *)stageItem);
    return stageItem;
}

static int hookItemCompare(ListNode *node, ListNode *newNode)
{
    const HOOK_ITEM *hookItem;
    const HOOK_ITEM *newItem;

    hookItem = (const HOOK_ITEM *)node;
    newItem = (const HOOK_ITEM *)newNode;
    return (hookItem->info.prio - newItem->info.prio);
}

struct HOOKITEM_COMPARE_VAL {
    int prio;
    OhosHook hook;
    void *hookCookie;
};

static int hookItemCompareValue(ListNode *node, void *data)
{
    const HOOK_ITEM *hookItem;
    struct HOOKITEM_COMPARE_VAL *compareVal = (struct HOOKITEM_COMPARE_VAL *)data;

    hookItem = (const HOOK_ITEM *)node;
    BEGET_CHECK(hookItem->info.prio == compareVal->prio, return (hookItem->info.prio - compareVal->prio));
    if (hookItem->info.hook == compareVal->hook && hookItem->info.hookCookie == compareVal->hookCookie) {
        return 0;
    }
    return -1;
}

// Add hook to stage list with prio ordered
static int addHookToStage(HOOK_STAGE *hookStage, int prio, OhosHook hook, void *hookCookie)
{
    HOOK_ITEM *hookItem;
    struct HOOKITEM_COMPARE_VAL compareVal;

    // Check if exists
    compareVal.prio = prio;
    compareVal.hook = hook;
    compareVal.hookCookie = hookCookie;
    hookItem = (HOOK_ITEM *)OH_ListFind(&(hookStage->hooks), (void *)(&compareVal), hookItemCompareValue);
    BEGET_CHECK(hookItem == NULL, return 0);

    // Create new item
    hookItem = (HOOK_ITEM *)malloc(sizeof(HOOK_ITEM));
    BEGET_CHECK(hookItem != NULL, return -1);
    hookItem->info.stage = hookStage->stage;
    hookItem->info.prio = prio;
    hookItem->info.hook = hook;
    hookItem->info.hookCookie = hookCookie;
    hookItem->stage = hookStage;

    // Insert with order
    OH_ListAddWithOrder(&(hookStage->hooks), (ListNode *)hookItem, hookItemCompare);
    return 0;
}

int HookMgrAddEx(HOOK_MGR *hookMgr, const HOOK_INFO *hookInfo)
{
    HOOK_STAGE *stageItem;
    BEGET_CHECK(hookInfo != NULL, return -1);
    BEGET_CHECK(hookInfo->hook != NULL, return -1);

    // Get HOOK_MGR
    hookMgr = getHookMgr(hookMgr, true);
    BEGET_CHECK(hookMgr != NULL, return -1);

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, hookInfo->stage, true);
    BEGET_CHECK(stageItem != NULL, return -1);

    // Add hook to stage
    return addHookToStage(stageItem, hookInfo->prio, hookInfo->hook, hookInfo->hookCookie);
}

int HookMgrAdd(HOOK_MGR *hookMgr, int stage, int prio, OhosHook hook)
{
    HOOK_INFO info;
    info.stage = stage;
    info.prio = prio;
    info.hook = hook;
    info.hookCookie = NULL;
    return HookMgrAddEx(hookMgr, &info);
}

static int hookTraversalDelProc(ListNode *node, void *cookie)
{
    HOOK_ITEM *hookItem = (HOOK_ITEM *)node;

    // Not equal, just return
    BEGET_CHECK((void *)hookItem->info.hook == cookie, return 0);

    // Remove from the list
    OH_ListRemove(node);
    // Destroy myself
    free((void *)node);

    return 0;
}

/*
 * 删除钩子函数
 * hook为NULL，表示删除该stage上的所有hooks
 */
void HookMgrDel(HOOK_MGR *hookMgr, int stage, OhosHook hook)
{
    HOOK_STAGE *stageItem;

    // Get HOOK_MGR
    hookMgr = getHookMgr(hookMgr, 0);
    BEGET_CHECK(hookMgr != NULL, return);

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, false);
    BEGET_CHECK(stageItem != NULL, return);

    if (hook != NULL) {
        OH_ListTraversal(&(stageItem->hooks), hook, hookTraversalDelProc, 0);
        return;
    }

    // Remove from list
    OH_ListRemove((ListNode *)stageItem);

    // Destroy stage item
    hookStageDestroy((ListNode *)stageItem);
}

typedef struct tagHOOK_EXECUTION_ARGS {
    void *executionContext;
    const HOOK_EXEC_OPTIONS *options;
} HOOK_EXECUTION_ARGS;

static int hookExecutionProc(ListNode *node, void *cookie)
{
    int ret;
    HOOK_ITEM *hookItem = (HOOK_ITEM *)node;
    HOOK_EXECUTION_ARGS *args = (HOOK_EXECUTION_ARGS *)cookie;

    if ((args->options != NULL) && (args->options->preHook != NULL)) {
        args->options->preHook(&hookItem->info, args->executionContext);
    }
    ret = hookItem->info.hook(&hookItem->info, args->executionContext);
    if ((args->options != NULL) && (args->options->postHook != NULL)) {
        args->options->postHook(&hookItem->info, args->executionContext, ret);
    }

    return ret;
}

/*
 * 执行钩子函数
 */
int HookMgrExecute(HOOK_MGR *hookMgr, int stage, void *executionContext, const HOOK_EXEC_OPTIONS *options)
{
    unsigned int flags;
    HOOK_STAGE *stageItem;
    HOOK_EXECUTION_ARGS args;

    // Get HOOK_MGR
    hookMgr = getHookMgr(hookMgr, 0);
    BEGET_CHECK(hookMgr != NULL, return -1)

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, false);
    BEGET_CHECK(stageItem != NULL, return -1);

    flags = 0;
    if (options != NULL) {
        flags = (unsigned int)(options->flags);
    }

    args.executionContext = executionContext;
    args.options = options;

    // Traversal all hooks in the specified stage
    return OH_ListTraversal(&(stageItem->hooks), (void *)(&args), hookExecutionProc, flags);
}

HOOK_MGR *HookMgrCreate(const char *name)
{
    HOOK_MGR *ret;

    BEGET_CHECK(name != NULL, return NULL);
    ret = (HOOK_MGR *)malloc(sizeof(HOOK_MGR));
    BEGET_CHECK(ret != NULL, return NULL);

    ret->name = strdup(name);
    if (ret->name == NULL) {
        free((void *)ret);
        return NULL;
    }
    OH_ListInit(&(ret->stages));
    return ret;
}

void HookMgrDestroy(HOOK_MGR *hookMgr)
{
    hookMgr = getHookMgr(hookMgr, 0);
    BEGET_CHECK(hookMgr != NULL, return);

    OH_ListRemoveAll(&(hookMgr->stages), hookStageDestroy);

    if (hookMgr == defaultHookMgr) {
        defaultHookMgr = NULL;
    }
    if (hookMgr->name != NULL) {
        free((void *)hookMgr->name);
    }
    free((void *)hookMgr);
}

typedef struct tagHOOK_TRAVERSAL_ARGS {
    void *traversalCookie;
    OhosHookTraversal traversal;
} HOOK_TRAVERSAL_ARGS;

static int hookItemTraversal(ListNode *node, void *data)
{
    HOOK_ITEM *hookItem;
    HOOK_TRAVERSAL_ARGS *stageArgs;

    hookItem = (HOOK_ITEM *)node;
    stageArgs = (HOOK_TRAVERSAL_ARGS *)data;

    stageArgs->traversal(&(hookItem->info), stageArgs->traversalCookie);
    return 0;
}

static int hookStageTraversal(ListNode *node, void *data)
{
    HOOK_STAGE *stageItem = (HOOK_STAGE *)node;
    OH_ListTraversal(&(stageItem->hooks), data, hookItemTraversal, 0);
    return 0;
}

/*
 * 遍历所有的hooks
 */
void HookMgrTraversal(HOOK_MGR *hookMgr, void *traversalCookie, OhosHookTraversal traversal)
{
    HOOK_TRAVERSAL_ARGS stageArgs;

    BEGET_CHECK(traversal != NULL, return);

    hookMgr = getHookMgr(hookMgr, 0);
    BEGET_CHECK(hookMgr != NULL, return);

    // Prepare common args
    stageArgs.traversalCookie = traversalCookie;
    stageArgs.traversal = traversal;
    OH_ListTraversal(&(hookMgr->stages), (void *)(&stageArgs), hookStageTraversal, 0);
}

/*
 * 获取指定stage中hooks的个数
 */
int HookMgrGetHooksCnt(HOOK_MGR *hookMgr, int stage)
{
    HOOK_STAGE *stageItem;

    hookMgr = getHookMgr(hookMgr, 0);
    BEGET_CHECK(hookMgr != NULL, return 0);

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, false);
    BEGET_CHECK(stageItem != NULL, return 0);

    return OH_ListGetCnt(&(stageItem->hooks));
}

/*
 * 获取指定stage中hooks的个数
 */
int HookMgrGetStagesCnt(HOOK_MGR *hookMgr)
{
    hookMgr = getHookMgr(hookMgr, 0);
    BEGET_CHECK(hookMgr != NULL, return 0);

    return OH_ListGetCnt(&(hookMgr->stages));
}
