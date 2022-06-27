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

#ifndef OHOS_HOOK_MANAGER_H__
#define OHOS_HOOK_MANAGER_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief A Hook is a means of executing custom code (function) for existing running code.
 *
 * For a running process, it may consists of several stages.
 * HookManager can help to add hooks to different stages with different priorities.
 * The relationships for HookManager, HookStage and HookItem are shown as below:
 * 
 * |￣￣￣￣￣￣|
 * |  HookMgr |
 * |__________|
 *      |
 *      |    |▔▔▔▔▔▔▔▔▔▔▔|    |▔▔▔▔▔▔▔▔▔▔▔| 
 *      └--->| HookStage |--->| HookStage | ...
 *           |___________|    |___________|
 *               |
 *               |    |▔▔▔▔▔▔▔▔▔▔|    |▔▔▔▔▔▔▔▔▔▔| 
 *               └--->| HookItem |--->| HookItem | ...
 *                    |__________|    |__________|
 *
 * Usage example:
 * 
 * For an existing module with several stages as below:
 *      ExistingStage1(...);
 *      ExistingStage2(...);
 *      ExistingStage3(...);
 * We can add hooks capability to it as below:
 *      HookMgrExecute(hookMgr, PRE_STAGE1, ...);
 *      ExistingStage1(...);
 *      HookMgrExecute(hookMgr, PRE_STAGE2, ...);
 *      ExistingStage2(...);
 *      HookMgrExecute(hookMgr, PRE_STAGE3, ...);
 *      ExistingStage3(...);
 *      HookMgrExecute(hookMgr, POST_STAGE3, ...);
 * 
 * For extending modules, we can add hooks without changing the existing module as below:
 *      int sampleHook() {
 *          ...
 *      }
 *      HookMgrAdd(hookMgr, PRE_STAGE1, priority, sampleHook);
 */

/* Forward declaration for HookManager */
typedef struct tagHOOK_MGR HOOK_MGR;

/* Forward declaration for HOOK_INFO */
typedef struct tagHOOK_INFO HOOK_INFO;

/**
 * @brief Hook function prototype
 *
 * @param hookInfo hook information
 * @param executionContext input arguments for running the hook execution context
 * @return return 0 if succeed; other values if failed.
 */
typedef int (*OhosHook)(const HOOK_INFO *hookInfo, void *executionContext);

struct tagHOOK_INFO {
    int stage;          /* hook stage */
    int prio;           /* hook priority */
    OhosHook hook;      /* hook function */
    void *hookCookie;   /* hook function cookie, for current hook only */
};

/**
 * @brief Add a hook function
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param stage hook stage
 * @param prio hook priority
 * @param hook hook function pointer
 * @return return 0 if succeed; other values if failed.
 */
int HookMgrAdd(HOOK_MGR *hookMgr, int stage, int prio, OhosHook hook);

/**
 * @brief Add a hook function with full hook information
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param hookInfo full hook information
 * @return return 0 if succeed; other values if failed.
 */
int HookMgrAddEx(HOOK_MGR *hookMgr, const HOOK_INFO *hookInfo);

/**
 * @brief Delete hook function
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param stage hook stage
 * @param hook hook function pointer
 *                If hook is NULL, it will delete all hooks in the stage
 * @return None
 */
void HookMgrDel(HOOK_MGR *hookMgr, int stage, OhosHook hook);

/**
 * @brief preHook function prototype for HookMgrExecute each hook
 *
 * @param hookInfo HOOK_INFO for the each hook.
 * @param executionContext input arguments for running the hook execution context.
 * @return None
 */
typedef void (*OhosHookPreExecution)(const HOOK_INFO *hookInfo, void *executionContext);

/**
 * @brief postHook function prototype for HookMgrExecute each hook
 *
 * @param hookInfo HOOK_INFO for the each hook.
 * @param executionContext input arguments for running the hook execution context.
 * @param executionRetVal return value for running the hook.
 * @return None
 */
typedef void (*OhosHookPostExecution)(const HOOK_INFO *hookInfo, void *executionContext, int executionRetVal);

/* Executing hooks in descending priority order */
#define HOOK_EXEC_REVERSE_ORDER     0x01
/* Stop executing hooks when error returned for each hook */
#define HOOK_EXEC_EXIT_WHEN_ERROR   0x02

/**
 * @brief Extra execution arguments for HookMgrExecute
 */
typedef struct tagHOOK_EXEC_OPTIONS {
    /* Executing flags */
    int flags;
    /* preHook for before executing each hook */
    OhosHookPreExecution preHook;
    /* postHook for before executing each hook */
    OhosHookPostExecution postHook;
} HOOK_EXEC_OPTIONS;

/**
 * @brief Executing each hooks in specified stages
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param stage hook stage
 * @param extraArgs HOOK_EXEC_ARGS for executing each hook.
 * @return return 0 if succeed; other values if failed.
 */
int HookMgrExecute(HOOK_MGR *hookMgr, int stage, void *executionContext, const HOOK_EXEC_OPTIONS *extraArgs);

/**
 * @brief Create a HookManager handle
 *
 * @param name HookManager name.
 * @return return HookManager handle; NULL if failed.
 */
HOOK_MGR *HookMgrCreate(const char *name);

/**
 * @brief Destroy HookManager
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @return None.
 */
void HookMgrDestroy(HOOK_MGR *hookMgr);

/**
 * @brief Hook traversal function prototype
 *
 * @param hookInfo HOOK_INFO for traversing each hook.
 * @return None
 */
typedef void (*OhosHookTraversal)(const HOOK_INFO *hookInfo, void *traversalCookie);

/**
 * @brief Traversing all hooks in the HookManager
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param traversalCookie traversal cookie.
 * @param traversal traversal function.
 * @return None.
 */
void HookMgrTraversal(HOOK_MGR *hookMgr, void *traversalCookie, OhosHookTraversal traversal);

/**
 * @brief Get number of hooks in specified stage
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param stage hook stage.
 * @return number of hooks, return 0 if none
 */
int HookMgrGetHooksCnt(HOOK_MGR *hookMgr, int stage);

/**
 * @brief Get number of stages in the HookManager
 *
 * @param hookMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @return number of stages, return 0 if none
 */
int HookMgrGetStagesCnt(HOOK_MGR *hookMgr);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
