/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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
#include "property.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "property_manager.h"
#include "property_trie.h"

#define LABEL "Manager"
#define MAX_BUFF 256

typedef struct {
    WorkSpace *workSpace;
    WorkSpace *persistWorkSpace;
    char *buffer;
} PersistContext;

static PropertyPersistWorkSpace g_persistWorkSpace = {ATOMIC_VAR_INIT(0), };

static int ProcessPropertTraversal(WorkSpace *workSpace, TrieNode *node, void *cookie)
{
    PROPERTY_CHECK(workSpace != 0 && node != NULL && cookie != NULL, return -1, "Invalid param");
    TrieDataNode *current = (TrieDataNode *)node;
    if (current == NULL || current->dataIndex == 0) {
        return 0;
    }
    DataEntry *entry = (DataEntry *)GetTrieNode(workSpace, &current->dataIndex);
    if (entry == NULL) {
        return -1;
    }
    PersistContext *persistContext = (PersistContext *)cookie;
    int ret = GetDataName(entry, persistContext->buffer, MAX_BUFF);
    if (strncmp(persistContext->buffer, "persist.", strlen("persist.")) != 0) {
        return 0;
    }
    ret |= GetDataValue(entry, persistContext->buffer + MAX_BUFF, MAX_BUFF);
    if (ret == 0) { // 只支持新建
        PROPERTY_LOGI("Insert new persist property from normal property %s", persistContext->buffer);
        ret = AddProperty(persistContext->persistWorkSpace, persistContext->buffer, persistContext->buffer + MAX_BUFF);
    }
    PROPERTY_CHECK(ret == 0, return ret, "Failed to add persist property");
    return ret;
}

static int ProcessPersistPropertTraversal(WorkSpace *workSpace, TrieNode *node, void *cookie)
{
    TrieDataNode *current = (TrieDataNode *)node;
    if (current == NULL || current->dataIndex == 0) {
        return 0;
    }
    DataEntry *entry = (DataEntry *)GetTrieNode(workSpace, &current->dataIndex);
    if (entry == NULL) {
        return -1;
    }
    PersistContext *persistContext = (PersistContext *)cookie;
    int ret = GetDataName(entry, persistContext->buffer, MAX_BUFF);
    ret |= GetDataValue(entry, persistContext->buffer + MAX_BUFF, MAX_BUFF);
    if (ret == 0) {
        PROPERTY_LOGI("update normal property %s %s from persist property ",
            persistContext->buffer, persistContext->buffer + MAX_BUFF);
        ret = WriteProperty(persistContext->workSpace, persistContext->buffer, persistContext->buffer + MAX_BUFF);
    }
    PROPERTY_CHECK(ret == 0, return ret, "Failed to add persist property");
    return ret;
}

int InitPersistPropertyWorkSpace(const char *context)
{
    u_int32_t flags = atomic_load_explicit(&g_persistWorkSpace.flags, memory_order_relaxed);
    PROPERTY_LOGI("InitPersistPropertyWorkSpace flags %x", flags);
    if ((flags & WORKSPACE_FLAGS_INIT) == WORKSPACE_FLAGS_INIT) {
        return 0;
    }
    g_persistWorkSpace.persistWorkSpace.compareTrieNode = CompareTrieDataNode;
    g_persistWorkSpace.persistWorkSpace.allocTrieNode = AllocateTrieDataNode;
    int ret = InitPersistWorkSpace(PROPERTY_PERSIST_PATH, &g_persistWorkSpace.persistWorkSpace);
    atomic_store_explicit(&g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT, memory_order_release);
    return ret;
}

int RefreshPersistProperties(PropertyWorkSpace *workSpace, const char *context)
{
    u_int32_t flags = atomic_load_explicit(&g_persistWorkSpace.flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) {
        int ret = InitPersistPropertyWorkSpace(context);
        PROPERTY_CHECK(ret == 0, return ret, "Failed to init persist property");
        flags = atomic_load_explicit(&g_persistWorkSpace.flags, memory_order_relaxed);
    }

    if ((flags & WORKSPACE_FLAGS_LOADED) == WORKSPACE_FLAGS_LOADED) {
        return 0;
    }

    // 申请临时的缓存，用于数据读取
    char *buffer = (char *)malloc(MAX_BUFF + MAX_BUFF);
    PROPERTY_CHECK(buffer != NULL, return -1, "Failed to malloc memory for property");
    PersistContext persistContext = {
        &workSpace->propertySpace, &g_persistWorkSpace.persistWorkSpace, buffer
    };

    // 遍历当前的属性，并把persist的写入
    int ret = TraversalTrieDataNode(&workSpace->propertySpace,
        (TrieDataNode *)workSpace->propertySpace.rootNode, ProcessPropertTraversal, &persistContext);

    // 修改默认属性值
    ret = TraversalTrieDataNode(&g_persistWorkSpace.persistWorkSpace,
        (TrieDataNode *)g_persistWorkSpace.persistWorkSpace.rootNode, ProcessPersistPropertTraversal, &persistContext);

    atomic_store_explicit(&g_persistWorkSpace.flags, flags | WORKSPACE_FLAGS_LOADED, memory_order_release);
    free(buffer);
    return ret;
}

void ClosePersistPropertyWorkSpace()
{
    CloseWorkSpace(&g_persistWorkSpace.persistWorkSpace);
    atomic_store_explicit(&g_persistWorkSpace.flags, 0, memory_order_release);
}

int WritePersistProperty(const char *name, const char *value)
{
    PROPERTY_CHECK(value != NULL && name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");

    u_int32_t flags = atomic_load_explicit(&g_persistWorkSpace.flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_LOADED) != WORKSPACE_FLAGS_LOADED) {
        return 0;
    }

    if (strncmp(name, "persist.", strlen("persist.")) != 0) {
        return 0;
    }
    return WriteProperty(&g_persistWorkSpace.persistWorkSpace, name, value);
}
