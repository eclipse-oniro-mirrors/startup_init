/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "param_persist.h"

#include <time.h>

#include "init_param.h"
#include "init_utils.h"
#include "param_manager.h"
#include "param_trie.h"
#include "param_osadp.h"
#include "securec.h"

static ParamPersistWorkSpace g_persistWorkSpace = {0, 0, NULL, 0, {0}};
static int IsNeedToSave(const char *name)
{
#if defined(__LITEOS_M__) || defined(__LITEOS_A__)
    return IS_READY_ONLY(name) ? 0 : 1;
#else
    return (strncmp(name, PARAM_PERSIST_PREFIX, strlen(PARAM_PERSIST_PREFIX)) == 0) ? 1 : 0;
#endif
}

static long long GetPersistCommitId(void)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return 0, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return 0, "Invalid space");
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space == NULL || space->area == NULL) {
        return 0;
    }
    PARAMSPACE_AREA_RD_LOCK(space);
    long long globalCommitId =  ATOMIC_LOAD_EXPLICIT(&space->area->commitPersistId, memory_order_acquire);
    PARAMSPACE_AREA_RW_UNLOCK(space);
    return globalCommitId;
}

static void UpdatePersistCommitId(void)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return, "Invalid space");
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space == NULL || space->area == NULL) {
        return;
    }
    PARAMSPACE_AREA_RW_LOCK(space);
    long long globalCommitId = ATOMIC_LOAD_EXPLICIT(&space->area->commitPersistId, memory_order_relaxed);
    ATOMIC_STORE_EXPLICIT(&space->area->commitPersistId, ++globalCommitId, memory_order_release);
    PARAMSPACE_AREA_RW_UNLOCK(space);
}

static int SavePersistParam(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie)
{
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL || current->dataIndex == 0) {
        return 0;
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
    if (entry == NULL) {
        return 0;
    }

    if (!IsNeedToSave(entry->data)) {
        return 0;
    }
    static char name[PARAM_NAME_LEN_MAX] = {0};
    int ret = memcpy_s(name, PARAM_NAME_LEN_MAX - 1, entry->data, entry->keyLength);
    PARAM_CHECK(ret == EOK, return -1, "Failed to read param name %s", entry->data);
    name[entry->keyLength] = '\0';
    ret = g_persistWorkSpace.persistParamOps.batchSave(
        (PERSIST_SAVE_HANDLE)cookie, name, entry->data + entry->keyLength + 1);
    PARAM_CHECK(ret == 0, return -1, "Failed to write param %s", current->key);
    return ret;
}

static int BatchSavePersistParam(void)
{
    PARAM_LOGV("BatchSavePersistParam");
    if (g_persistWorkSpace.persistParamOps.batchSaveBegin == NULL ||
        g_persistWorkSpace.persistParamOps.batchSave == NULL ||
        g_persistWorkSpace.persistParamOps.batchSaveEnd == NULL) {
        return 0;
    }

    PERSIST_SAVE_HANDLE handle;
    int ret = g_persistWorkSpace.persistParamOps.batchSaveBegin(&handle);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to save persist");
#if defined(__LITEOS_M__) || defined(__LITEOS_A__)
    const char *prefix = "";
#else
    const char *prefix = PARAM_PERSIST_PREFIX;
#endif
    // walk and save persist param
    WorkSpace *workSpace = GetFirstWorkSpace();
    while (workSpace != NULL) {
        WorkSpace *next = GetNextWorkSpace(workSpace);
        ParamTrieNode *root = FindTrieNode(workSpace, prefix, strlen(prefix), NULL);
        PARAMSPACE_AREA_RD_LOCK(workSpace);
        TraversalTrieNode(workSpace, root, SavePersistParam, (void *)handle);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
        workSpace = next;
    }
    g_persistWorkSpace.persistParamOps.batchSaveEnd(handle);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Save persist param fail");

    PARAM_CLEAR_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE);
    (void)time(&g_persistWorkSpace.lastSaveTimer);
    return ret;
}

INIT_LOCAL_API int InitPersistParamWorkSpace(void)
{
    if (PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    (void)time(&g_persistWorkSpace.lastSaveTimer);
    RegisterPersistParamOps(&g_persistWorkSpace.persistParamOps);
    PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT);
    return 0;
}

INIT_LOCAL_API void ClosePersistParamWorkSpace(void)
{
    if (g_persistWorkSpace.saveTimer != NULL) {
        ParamTimerClose(g_persistWorkSpace.saveTimer);
    }
    g_persistWorkSpace.flags = 0;
}

void CheckAndSavePersistParam(void)
{
    // check commit
    long long commit = GetPersistCommitId();
    PARAM_LOGV("CheckAndSavePersistParam commit %lld %lld", commit, g_persistWorkSpace.commitId);
    if (g_persistWorkSpace.commitId == commit) {
        return;
    }
    g_persistWorkSpace.commitId = commit;
    (void)BatchSavePersistParam();
}

PARAM_STATIC void TimerCallbackForSave(const ParamTaskPtr timer, void *context)
{
    UNUSED(context);
    UNUSED(timer);
    PARAM_LOGV("TimerCallbackForSave ");
    // for liteos-a must cycle check
#if (!defined(PARAM_SUPPORT_CYCLE_CHECK) || defined(PARAM_SUPPORT_REAL_CHECK))
    ParamTimerClose(g_persistWorkSpace.saveTimer);
    g_persistWorkSpace.saveTimer = NULL;
    if (!PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE)) {
        return;
    }
#endif
    CheckAndSavePersistParam();
}

INIT_LOCAL_API int WritePersistParam(const char *name, const char *value)
{
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    if (!IsNeedToSave(name)) {
        return 0;
    }
    PARAM_LOGV("WritePersistParam name %s ", name);
    if (g_persistWorkSpace.persistParamOps.save != NULL) {
        g_persistWorkSpace.persistParamOps.save(name, value);
    }
    // update commit for check
    UpdatePersistCommitId();
    // for liteos-m, start task to check and save parameter
    // for linux, start timer after set persist parameter
    // for liteos-a, start timer in init to check and save parameter
#ifdef PARAM_SUPPORT_REAL_CHECK
    if (!PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED)) {
        PARAM_LOGE("Can not save persist param before load %s ", name);
        return 0;
    }
    if (g_persistWorkSpace.persistParamOps.batchSave == NULL) {
        return 0;
    }

    // check timer for save all
    time_t currTimer;
    (void)time(&currTimer);
    uint32_t diff = Difftime(currTimer, g_persistWorkSpace.lastSaveTimer);
    if (diff > PARAM_MUST_SAVE_PARAM_DIFF) {
        if (g_persistWorkSpace.saveTimer != NULL) {
            ParamTimerClose(g_persistWorkSpace.saveTimer);
            g_persistWorkSpace.saveTimer = NULL;
        }
        return BatchSavePersistParam();
    }

    PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE);
    if (g_persistWorkSpace.saveTimer == NULL) {
        ParamTimerCreate(&g_persistWorkSpace.saveTimer, TimerCallbackForSave, NULL);
        ParamTimerStart(g_persistWorkSpace.saveTimer, PARAM_MUST_SAVE_PARAM_DIFF * MS_UNIT, MS_UNIT);
    }
#endif
    return 0;
}

int LoadPersistParams(void)
{
    // get persist parameter
    int ret = InitPersistParamWorkSpace();
    PARAM_CHECK(ret == 0, return ret, "Failed to init persist param");
#ifndef STARTUP_INIT_TEST
    if (PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED)) {
        PARAM_LOGE("Persist param has been loaded");
        return 0;
    }
#endif
    if (g_persistWorkSpace.persistParamOps.load != NULL) {
        (void)g_persistWorkSpace.persistParamOps.load();
        PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED);
    }
    // save new persist param
    ret = BatchSavePersistParam();
    PARAM_CHECK(ret == 0, return ret, "Failed to load persist param");
    // for liteos-a, start time to check in init
#ifdef PARAM_SUPPORT_CYCLE_CHECK
    PARAM_LOGV("LoadPersistParams start check time ");
    if (g_persistWorkSpace.saveTimer == NULL) {
        ParamTimerCreate(&g_persistWorkSpace.saveTimer, TimerCallbackForSave, NULL);
        ParamTimerStart(g_persistWorkSpace.saveTimer, PARAM_MUST_SAVE_PARAM_DIFF * MS_UNIT, MS_UNIT);
    }
#endif
    return 0;
}
