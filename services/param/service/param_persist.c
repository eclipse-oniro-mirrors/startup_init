/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "param_manager.h"
#include "param_service.h"
#include "param_trie.h"
#include "sys_param.h"

static ParamPersistWorkSpace g_persistWorkSpace = { 0, NULL, 0, { NULL, NULL, NULL, NULL, NULL } };

static int AddPersistParam(const char *name, const char *value, void *context)
{
    PARAM_CHECK(value != NULL && name != NULL && context != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid name or context");
    WorkSpace *workSpace = (WorkSpace *)context;
    uint32_t dataIndex = 0;
    int ret = WriteParam(workSpace, name, value, &dataIndex, 0);
    PARAM_CHECK(ret == 0, return ret, "Failed to write param %d name:%s %s", ret, name, value);
    return 0;
}

static int SavePersistParam(const WorkSpace *workSpace, const ParamTrieNode *node, void *cookie)
{
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL || current->dataIndex == 0) {
        return 0;
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
    if (entry == NULL) {
        return 0;
    }
    if (strncmp(entry->data, PARAM_PERSIST_PREFIX, strlen(PARAM_PERSIST_PREFIX)) != 0) {
        return 0;
    }
    static char name[PARAM_NAME_LEN_MAX] = { 0 };
    int ret = memcpy_s(name, PARAM_NAME_LEN_MAX - 1, entry->data, entry->keyLength);
    PARAM_CHECK(ret == EOK, return -1, "Failed to read param name %s", entry->data);
    name[entry->keyLength] = '\0';
    ret = g_persistWorkSpace.persistParamOps.batchSave(cookie, name, entry->data + entry->keyLength + 1);
    PARAM_CHECK(ret == 0, return -1, "Failed to write param %s", current->key);
    return ret;
}

static int BatchSavePersistParam(const WorkSpace *workSpace)
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
    ParamTrieNode *root = FindTrieNode(workSpace, PARAM_PERSIST_PREFIX, strlen(PARAM_PERSIST_PREFIX), NULL);
    ret = TraversalTrieNode(workSpace, root, SavePersistParam, handle);
    g_persistWorkSpace.persistParamOps.batchSaveEnd(handle);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Save persist param fail");

    PARAM_CLEAR_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE);
    (void)time(&g_persistWorkSpace.lastSaveTimer);
    return ret;
}

int InitPersistParamWorkSpace(const ParamWorkSpace *workSpace)
{
    UNUSED(workSpace);
    if (PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    (void)time(&g_persistWorkSpace.lastSaveTimer);
#ifdef PARAM_SUPPORT_SAVE_PERSIST
    RegisterPersistParamOps(&g_persistWorkSpace.persistParamOps);
#endif
    PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT);
    return 0;
}

void ClosePersistParamWorkSpace(void)
{
    if (g_persistWorkSpace.saveTimer != NULL) {
        ParamTaskClose(g_persistWorkSpace.saveTimer);
    }
    g_persistWorkSpace.flags = 0;
}

int LoadPersistParam(ParamWorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    int ret = InitPersistParamWorkSpace(workSpace);
    PARAM_CHECK(ret == 0, return ret, "Failed to init persist param");
#ifndef STARTUP_INIT_TEST
    if (PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED)) {
        PARAM_LOGE("Persist param has been loaded");
        return 0;
    }
#endif
    if (g_persistWorkSpace.persistParamOps.load != NULL) {
        ret = g_persistWorkSpace.persistParamOps.load(AddPersistParam, &workSpace->paramSpace);
        PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED);
    }
    // save new persist param
    ret = BatchSavePersistParam(&workSpace->paramSpace);
    PARAM_CHECK(ret == 0, return ret, "Failed to load persist param");
    return 0;
}

PARAM_STATIC void TimerCallbackForSave(const ParamTaskPtr timer, void *context)
{
    UNUSED(context);
    UNUSED(timer);
    ParamTaskClose(g_persistWorkSpace.saveTimer);
    g_persistWorkSpace.saveTimer = NULL;
    if (!PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE)) {
        return;
    }
    (void)BatchSavePersistParam((WorkSpace *)context);
}

int WritePersistParam(ParamWorkSpace *workSpace, const char *name, const char *value)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    if (strncmp(name, PARAM_PERSIST_PREFIX, strlen(PARAM_PERSIST_PREFIX)) != 0) {
        return 0;
    }
    if (!PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED)) {
        PARAM_LOGE("Can not save persist param before load %s ", name);
        return 0;
    }
    PARAM_LOGV("WritePersistParam name %s ", name);
    if (g_persistWorkSpace.persistParamOps.save != NULL) {
        g_persistWorkSpace.persistParamOps.save(name, value);
    }

    // 不需要批量保存
    if (g_persistWorkSpace.persistParamOps.batchSave == NULL) {
        return 0;
    }

    // check timer for save all
    time_t currTimer;
    (void)time(&currTimer);
    uint32_t diff = (uint32_t)difftime(currTimer, g_persistWorkSpace.lastSaveTimer);
    PARAM_LOGV("WritePersistParam name %s  %d ", name, diff);
    if (diff > PARAM_MUST_SAVE_PARAM_DIFF) {
        if (g_persistWorkSpace.saveTimer != NULL) {
            ParamTaskClose(g_persistWorkSpace.saveTimer);
            g_persistWorkSpace.saveTimer = NULL;
        }
        return BatchSavePersistParam(&workSpace->paramSpace);
    }
    PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE);
    if (g_persistWorkSpace.saveTimer == NULL) {
        ParamTimerCreate(&g_persistWorkSpace.saveTimer,
            TimerCallbackForSave, &workSpace->paramSpace);
        ParamTimerStart(g_persistWorkSpace.saveTimer, PARAM_MUST_SAVE_PARAM_DIFF * MS_UNIT, MS_UNIT);
    }
    return 0;
}
