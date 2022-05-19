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
#include "hookmgr.h"

// Forward declaration
typedef struct tagHOOK_STAGE HOOK_STAGE;

/*
 * Internal HOOK Item with priorities
 */
typedef struct tagHOOK_ITEM {
    ListNode node;
    int prio;
    OhosHook hook;
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
    if (hookMgr != NULL) {
        return hookMgr;
    }
    // Use default HOOK_MGR if possible
    if (defaultHookMgr != NULL) {
        return defaultHookMgr;
    }

    if (!autoCreate) {
        return NULL;
    }

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

    if (node == NULL) {
        return;
    }

    stage = (HOOK_STAGE *)node;
    ListRemoveAll(&(stage->hooks), NULL);
    free((void *)stage);
}

// Get HOOK_STAGE if found, otherwise create it
static HOOK_STAGE *getHookStage(HOOK_MGR *hookMgr, int stage, int createIfNotFound)
{
    HOOK_STAGE *stageItem;

    stageItem = (HOOK_STAGE *)ListFind(&(hookMgr->stages), (void *)(&stage), hookStageCompare);
    if (stageItem != NULL) {
        return stageItem;
    }

    if (!createIfNotFound) {
        return NULL;
    }

    // Not found, create it
    stageItem = (HOOK_STAGE *)malloc(sizeof(HOOK_STAGE));
    if (stageItem == NULL) {
        return NULL;
    }
    stageItem->stage = stage;
    ListInit(&(stageItem->hooks));
    ListAddTail(&(hookMgr->stages), (ListNode *)stageItem);
    return stageItem;
}

static int hookItemCompare(ListNode *node, ListNode *newNode)
{
    const HOOK_ITEM *hookItem;
    const HOOK_ITEM *newItem;

    hookItem = (const HOOK_ITEM *)node;
    newItem = (const HOOK_ITEM *)newNode;
    return (hookItem->prio - newItem->prio);
}

struct HOOKITEM_COMPARE_VAL {
    int prio;
    OhosHook hook;
};

static int hookItemCompareValue(ListNode *node, void *data)
{
    const HOOK_ITEM *hookItem;
    struct HOOKITEM_COMPARE_VAL *compareVal = (struct HOOKITEM_COMPARE_VAL *)data;

    hookItem = (const HOOK_ITEM *)node;
    if (hookItem->prio != compareVal->prio) {
        return (hookItem->prio - compareVal->prio);
    }
    if (hookItem->hook == compareVal->hook) {
        return 0;
    }
    return -1;
}

// Add hook to stage list with prio ordered
static int addHookToStage(HOOK_STAGE *hookStage, int prio, OhosHook hook)
{
    HOOK_ITEM *hookItem;
    struct HOOKITEM_COMPARE_VAL compareVal;

    // Check if exists
    compareVal.prio = prio;
    compareVal.hook = hook;
    hookItem = (HOOK_ITEM *)ListFind(&(hookStage->hooks), (void *)(&compareVal), hookItemCompareValue);
    if (hookItem != NULL) {
        return 0;
    }

    // Create new item
    hookItem = (HOOK_ITEM *)malloc(sizeof(HOOK_ITEM));
    if (hookItem == NULL) {
        return -1;
    }
    hookItem->prio = prio;
    hookItem->hook = hook;
    hookItem->stage = hookStage;

    // Insert with order
    ListAddWithOrder(&(hookStage->hooks), (ListNode *)hookItem, hookItemCompare);
    return 0;
}

int HookMgrAdd(HOOK_MGR *hookMgr, int stage, int prio, OhosHook hook)
{
    HOOK_STAGE *stageItem;
    if (hook == NULL) {
        return -1;
    }

    // Get HOOK_MGR
    hookMgr = getHookMgr(hookMgr, true);
    if (hookMgr == NULL) {
        return -1;
    }

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, true);
    if (stageItem == NULL) {
        return -1;
    }

    // Add hook to stage
    return addHookToStage(stageItem, prio, hook);
}

static int hookTraversalDelProc(ListNode *node, void *cookie)
{
    HOOK_ITEM *hookItem = (HOOK_ITEM *)node;

    // Not equal, just return
    if ((void *)hookItem->hook != cookie) {
        return 0;
    }

    // Remove from the list
    ListRemove(node);
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
    if (hookMgr == NULL) {
        return;
    }

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, false);
    if (stageItem == NULL) {
        return;
    }

    if (hook != NULL) {
        ListTraversal(&(stageItem->hooks), hook, hookTraversalDelProc, 0);
        return;
    }

    // Remove from list
    ListRemove((ListNode *)stageItem);

    // Destroy stage item
    hookStageDestroy((ListNode *)stageItem);
}

static int hookTraversalProc(ListNode *node, void *cookie)
{
    HOOK_ITEM *hookItem = (HOOK_ITEM *)node;
    HOOK_INFO hookInfo;
    const HOOK_EXEC_ARGS *args = (const HOOK_EXEC_ARGS *)cookie;

    hookInfo.stage = hookItem->stage->stage;
    hookInfo.prio = hookItem->prio;
    hookInfo.hook = hookItem->hook;
    hookInfo.cookie = NULL;
    hookInfo.retVal = 0;

    if (args != NULL) {
        hookInfo.cookie = args->cookie;
    }

    if ((args != NULL) && (args->preHook != NULL)) {
        args->preHook(&hookInfo);
    }
    hookInfo.retVal = hookItem->hook(hookInfo.stage, hookItem->prio, hookInfo.cookie);
    if ((args != NULL) && (args->postHook != NULL)) {
        args->postHook(&hookInfo);
    }

    return hookInfo.retVal;
}

/*
 * 执行钩子函数
 */
int HookMgrExecute(HOOK_MGR *hookMgr, int stage, const HOOK_EXEC_ARGS *args)
{
    int flags;
    HOOK_STAGE *stageItem;

    // Get HOOK_MGR
    hookMgr = getHookMgr(hookMgr, 0);
    if (hookMgr == NULL) {
        return -1;
    }

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, false);
    if (stageItem == NULL) {
        return -1;
    }

    flags = 0;
    if (args != NULL) {
        flags = args->flags;
    }

    // Traversal all hooks in the specified stage
    return ListTraversal(&(stageItem->hooks), (void *)args,
                         hookTraversalProc, flags);
}

HOOK_MGR *HookMgrCreate(const char *name)
{
    HOOK_MGR *ret;

    if (name == NULL) {
        return NULL;
    }
    ret = (HOOK_MGR *)malloc(sizeof(HOOK_MGR));
    if (ret == NULL) {
        return NULL;
    }

    ret->name = strdup(name);
    if (ret->name == NULL) {
        free((void *)ret);
        return NULL;
    }
    ListInit(&(ret->stages));
    return ret;
}

void HookMgrDestroy(HOOK_MGR *hookMgr)
{
    hookMgr = getHookMgr(hookMgr, 0);
    if (hookMgr == NULL) {
        return;
    }

    ListRemoveAll(&(hookMgr->stages), hookStageDestroy);

    if (hookMgr == defaultHookMgr) {
        defaultHookMgr = NULL;
    }
    if (hookMgr->name != NULL) {
        free((void *)hookMgr->name);
    }
    free((void *)hookMgr);
}

typedef struct tagHOOK_TRAVERSAL_ARGS {
    HOOK_INFO hookInfo;
    OhosHookTraversal traversal;
} HOOK_TRAVERSAL_ARGS;

static int hookItemTraversal(ListNode *node, void *data)
{
    HOOK_ITEM *hookItem;
    HOOK_TRAVERSAL_ARGS *stageArgs;

    hookItem = (HOOK_ITEM *)node;
    stageArgs = (HOOK_TRAVERSAL_ARGS *)data;

    stageArgs->hookInfo.prio = hookItem->prio;
    stageArgs->hookInfo.hook = hookItem->hook;

    stageArgs->traversal(&(stageArgs->hookInfo));
    return 0;
}

static int hookStageTraversal(ListNode *node, void *data)
{
    HOOK_STAGE *stageItem;
    HOOK_TRAVERSAL_ARGS *stageArgs;

    stageItem = (HOOK_STAGE *)node;
    stageArgs = (HOOK_TRAVERSAL_ARGS *)data;

    stageArgs->hookInfo.stage = stageItem->stage;

    ListTraversal(&(stageItem->hooks), data, hookItemTraversal, 0);

    return 0;
}

/*
 * 遍历所有的hooks
 */
void HookMgrTraversal(HOOK_MGR *hookMgr, void *cookie, OhosHookTraversal traversal)
{
    HOOK_TRAVERSAL_ARGS stageArgs;

    if (traversal == NULL) {
        return;
    }

    hookMgr = getHookMgr(hookMgr, 0);
    if (hookMgr == NULL) {
        return;
    }

    // Prepare common args
    stageArgs.hookInfo.cookie = cookie;
    stageArgs.hookInfo.retVal = 0;
    stageArgs.traversal = traversal;
    ListTraversal(&(hookMgr->stages), (void *)(&stageArgs), hookStageTraversal, 0);
}

/*
 * 获取指定stage中hooks的个数
 */
int HookMgrGetHooksCnt(HOOK_MGR *hookMgr, int stage)
{
    HOOK_STAGE *stageItem;

    hookMgr = getHookMgr(hookMgr, 0);
    if (hookMgr == NULL) {
        return 0;
    }

    // Get HOOK_STAGE list
    stageItem = getHookStage(hookMgr, stage, false);
    if (stageItem == NULL) {
        return 0;
    }

    return ListGetCnt(&(stageItem->hooks));
}

/*
 * 获取指定stage中hooks的个数
 */
int HookMgrGetStagesCnt(HOOK_MGR *hookMgr)
{
    hookMgr = getHookMgr(hookMgr, 0);
    if (hookMgr == NULL) {
        return 0;
    }

    return ListGetCnt(&(hookMgr->stages));
}
