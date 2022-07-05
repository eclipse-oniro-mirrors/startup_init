/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <ctype.h>
#include <limits.h>

#include "param_manager.h"
#include "param_trie.h"
#include "param_base.h"

INIT_LOCAL_API WorkSpace *GetWorkSpace(const char *name)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    PARAM_CHECK(paramSpace->workSpaceHashHandle != NULL, return NULL, "Invalid workSpaceHashHandle");
    char *tmpName = (char *)name;
#ifndef PARAM_SUPPORT_SELINUX
    tmpName = WORKSPACE_NAME_NORMAL;
#else
    if (strcmp(name, WORKSPACE_NAME_DAC) != 0) {
        tmpName = (char *)GetSelinuxContent(name);
    }
#endif
    WorkSpace *space = NULL;
    WORKSPACE_RD_LOCK(*paramSpace);
    HashNode *node = OH_HashMapGet(paramSpace->workSpaceHashHandle, (const void *)tmpName);
    if (node != NULL) {
        space = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    }
    WORKSPACE_RW_UNLOCK(*paramSpace);
    return (space != NULL && space->area != NULL) ? space : NULL;
}

static int CompareIndex(const HashNode *node, const void *key)
{
    WorkSpace *workSpace = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    if (workSpace == NULL || workSpace->area == NULL) {
        return 1;
    }
    uint32_t index = *(uint32_t *)key;
    if (workSpace->area->startIndex <= index && index < (workSpace->area->startIndex + workSpace->area->dataSize)) {
        return 0;
    }
    return 1;
}

INIT_LOCAL_API ParamTrieNode *GetTrieNodeByHandle(ParamHandle handle)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return NULL);
    int hashCode = ((handle >> 24) & 0x000000ff);  // 24 left shift
    uint32_t index = handle & 0x00ffffff;
    WORKSPACE_RD_LOCK(*paramSpace);
    HashNode *node = OH_HashMapFind(paramSpace->workSpaceHashHandle, hashCode, (const void *)&index, CompareIndex);
    if (node == NULL) {
        WORKSPACE_RW_UNLOCK(*paramSpace);
        PARAM_LOGV("Failed to get workspace for 0x%x index %d hashCode %d", handle, index, hashCode);
        return NULL;
    }
    WorkSpace *workSpace = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    WORKSPACE_RW_UNLOCK(*paramSpace);
    index = index - workSpace->area->startIndex;
    return (ParamTrieNode *)GetTrieNode(workSpace, index);
}

INIT_LOCAL_API WorkSpace *GetFirstWorkSpace(void)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");

    WorkSpace *workSpace = NULL;
    WORKSPACE_RD_LOCK(*paramSpace);
    ListNode *node = paramSpace->workSpaceList.next;
    if (node != &paramSpace->workSpaceList) {
        workSpace = HASHMAP_ENTRY(node, WorkSpace, node);
    }
    WORKSPACE_RW_UNLOCK(*paramSpace);
    return workSpace;
}

INIT_LOCAL_API WorkSpace *GetNextWorkSpace(WorkSpace *curr)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    PARAM_CHECK(curr != NULL, return NULL, "Invalid curr");
    WorkSpace *workSpace = NULL;
    WORKSPACE_RD_LOCK(*paramSpace);
    ListNode *node = curr->node.next;
    if (node != &paramSpace->workSpaceList) {
        workSpace = HASHMAP_ENTRY(node, WorkSpace, node);
    }
    WORKSPACE_RW_UNLOCK(*paramSpace);
    return workSpace;
}

INIT_LOCAL_API int ReadParamWithCheck(const char *name, uint32_t op, ParamHandle *handle)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(handle != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param handle");
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param name");
    *handle = -1;
    int ret = CheckParamPermission(GetParamSecurityLabel(), name, op);
    PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);

    WorkSpace *space = GetWorkSpace(name);
    PARAM_CHECK(space != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    ParamTrieNode *node = FindTrieNode(space, name, strlen(name), NULL);
    if (node != NULL && node->dataIndex != 0) {
        *handle = GetParamHandle(space, node->dataIndex, name);
        PARAM_LOGV("ReadParamWithCheck %s 0x%x %d", name, *handle, node->dataIndex);
        return 0;
    } else if (node != NULL) {
        return PARAM_CODE_NODE_EXIST;
    }
    return PARAM_CODE_NOT_FOUND;
}

INIT_LOCAL_API int CheckParamValue(const ParamTrieNode *node, const char *name, const char *value)
{
    if (IS_READY_ONLY(name)) {
        PARAM_CHECK(strlen(value) < PARAM_CONST_VALUE_LEN_MAX,
            return PARAM_CODE_INVALID_VALUE, "Illegal param value %s", value);
        if (node != NULL && node->dataIndex != 0) {
            PARAM_LOGE("Read-only param was already set %s", name);
            return PARAM_CODE_READ_ONLY;
        }
    } else {
        PARAM_CHECK(strlen(value) < PARAM_VALUE_LEN_MAX,
            return PARAM_CODE_INVALID_VALUE, "Illegal param value %s", value);
    }
    return 0;
}

INIT_LOCAL_API int CheckParamName(const char *name, int info)
{
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    size_t nameLen = strlen(name);
    if (nameLen >= PARAM_NAME_LEN_MAX) {
        return PARAM_CODE_INVALID_NAME;
    }
    if (strcmp(name, "#") == 0) {
        return 0;
    }

    if (nameLen < 1 || name[0] == '.' || (!info && name[nameLen - 1] == '.')) {
        PARAM_LOGE("CheckParamName %s %d", name, info);
        return PARAM_CODE_INVALID_NAME;
    }

    /* Only allow alphanumeric, plus '.', '-', '@', ':', or '_' */
    /* Don't allow ".." to appear in a param name */
    for (size_t i = 0; i < nameLen; i++) {
        if (name[i] == '.') {
            if (name[i - 1] == '.') {
                return PARAM_CODE_INVALID_NAME;
            }
            continue;
        }
        if (name[i] == '_' || name[i] == '-' || name[i] == '@' || name[i] == ':') {
            continue;
        }
        if (isalnum(name[i])) {
            continue;
        }
        return PARAM_CODE_INVALID_NAME;
    }
    return 0;
}

static int AddParam(WorkSpace *workSpace, const char *name, const char *value, uint32_t *dataIndex)
{
    ParamTrieNode *node = AddTrieNode(workSpace, name, strlen(name));
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node");
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
    if (entry == NULL) {
        uint32_t offset = AddParamNode(workSpace, name, strlen(name), value, strlen(value));
        PARAM_CHECK(offset > 0, return PARAM_CODE_REACHED_MAX, "Failed to allocate name %s", name);
        SaveIndex(&node->dataIndex, offset);
        long long globalCommitId = ATOMIC_LOAD_EXPLICIT(&workSpace->area->commitId, memory_order_relaxed);
        ATOMIC_STORE_EXPLICIT(&workSpace->area->commitId, ++globalCommitId, memory_order_release);
#ifdef PARAM_SUPPORT_SELINUX
        WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
        if (space != workSpace) { // dac commit is global commit
            globalCommitId = ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_relaxed);
            ATOMIC_STORE_EXPLICIT(&space->area->commitId, ++globalCommitId, memory_order_release);
        }
#endif
    }
    if (dataIndex != NULL) {
        *dataIndex = node->dataIndex;
    }
    PARAM_LOGV("AddParam name %s value: %s", name, value);
    return 0;
}

static int UpdateParam(const WorkSpace *workSpace, uint32_t *dataIndex, const char *name, const char *value)
{
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, *dataIndex);
    PARAM_CHECK(entry != NULL, return PARAM_CODE_REACHED_MAX, "Failed to update param value %s %u", name, *dataIndex);
    PARAM_CHECK(entry->keyLength == strlen(name), return PARAM_CODE_INVALID_NAME, "Failed to check name len %s", name);

    uint32_t valueLen = strlen(value);
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_relaxed);
    ATOMIC_STORE_EXPLICIT(&entry->commitId, commitId | PARAM_FLAGS_MODIFY, memory_order_relaxed);
    long long globalCommitId = ATOMIC_LOAD_EXPLICIT(&workSpace->area->commitId, memory_order_relaxed);
    if (entry->valueLength < PARAM_VALUE_LEN_MAX && valueLen < PARAM_VALUE_LEN_MAX) {
        int ret = ParamMemcpy(entry->data + entry->keyLength + 1, PARAM_VALUE_LEN_MAX, value, valueLen + 1);
        PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_VALUE, "Failed to copy value");
        entry->valueLength = valueLen;
    }
    uint32_t flags = commitId & ~PARAM_FLAGS_COMMITID;
    ATOMIC_STORE_EXPLICIT(&entry->commitId, (++commitId) | flags, memory_order_release);
    ATOMIC_STORE_EXPLICIT(&workSpace->area->commitId, ++globalCommitId, memory_order_release);
#ifdef PARAM_SUPPORT_SELINUX
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space != workSpace) { // dac commit is global commit
        globalCommitId = ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_relaxed);
        ATOMIC_STORE_EXPLICIT(&space->area->commitId, ++globalCommitId, memory_order_release);
    }
#endif
    PARAM_LOGV("UpdateParam name %s value: %s", name, value);
    futex_wake(&entry->commitId, INT_MAX);
    return 0;
}

INIT_LOCAL_API int WriteParam(const char *name, const char *value, uint32_t *dataIndex, int mode)
{
    PARAM_LOGV("WriteParam %s", name);
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid name or value");
    WorkSpace *workSpace = GetWorkSpace(name);
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    ParamTrieNode *node = FindTrieNode(workSpace, name, strlen(name), NULL);
    int ret = 0;
    if (node != NULL && node->dataIndex != 0) {
        if (dataIndex != NULL) {
            *dataIndex = node->dataIndex;
        }
        if ((mode & LOAD_PARAM_ONLY_ADD) == LOAD_PARAM_ONLY_ADD) {
            return PARAM_CODE_READ_ONLY;
        }
        ret = CheckParamValue(node, name, value);
        PARAM_CHECK(ret == 0, return ret, "Invalid param value param: %s=%s", name, value);
        PARAMSPACE_AREA_RW_LOCK(workSpace);
        ret = UpdateParam(workSpace, &node->dataIndex, name, value);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
    } else {
        ret = CheckParamValue(node, name, value);
        PARAM_CHECK(ret == 0, return ret, "Invalid param value param: %s=%s", name, value);
        PARAMSPACE_AREA_RW_LOCK(workSpace);
        ret = AddParam((WorkSpace *)workSpace, name, value, dataIndex);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
    }
    return ret;
}

INIT_LOCAL_API int AddSecurityLabel(const ParamAuditData *auditData)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(auditData != NULL && auditData->name != NULL, return -1, "Invalid auditData");
    WorkSpace *workSpace = GetWorkSpace(WORKSPACE_NAME_DAC);
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    int ret = CheckParamName(auditData->name, 1);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name \"%s\"", auditData->name);

    ParamTrieNode *node = FindTrieNode(workSpace, auditData->name, strlen(auditData->name), NULL);
    if (node == NULL) {
        node = AddTrieNode(workSpace, auditData->name, strlen(auditData->name));
    }
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node %s", auditData->name);
    uint32_t offset = node->labelIndex;
    if (node->labelIndex == 0) {  // can not support update for label
        offset = AddParamSecruityNode(workSpace, auditData);
        PARAM_CHECK(offset != 0, return PARAM_CODE_REACHED_MAX, "Failed to add label");
        SaveIndex(&node->labelIndex, offset);
    } else {
#ifdef STARTUP_INIT_TEST
        ParamSecurityNode *label = (ParamSecurityNode *)GetTrieNode(workSpace, node->labelIndex);
        label->mode = auditData->dacData.mode;
        label->uid = auditData->dacData.uid;
        label->gid = auditData->dacData.gid;
#endif
        PARAM_LOGE("Error, repeat to add label for name %s", auditData->name);
    }
    PARAM_LOGV("AddSecurityLabel label %d gid %d uid %d mode %o name: %s", offset,
        auditData->dacData.gid, auditData->dacData.uid, auditData->dacData.mode, auditData->name);
    return 0;
}

INIT_LOCAL_API ParamSecurityOps *GetParamSecurityOps(int type)
{
    PARAM_CHECK(type < PARAM_SECURITY_MAX, return NULL, "Invalid type");
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    return &paramSpace->paramSecurityOps[type];
}

INIT_LOCAL_API ParamSecurityLabel *GetParamSecurityLabel()
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    return &paramSpace->securityLabel;
}

int SpliteString(char *line, const char *exclude[], uint32_t count,
    int (*result)(const uint32_t *context, const char *name, const char *value), const uint32_t *context)
{
    // Skip spaces
    char *name = line;
    while (isspace(*name) && (*name != '\0')) {
        name++;
    }
    // Empty line or Comment line
    if (*name == '\0' || *name == '#') {
        return 0;
    }

    char *value = name;
    // find the first delimiter '='
    while (*value != '\0') {
        if (*value == '=') {
            (*value) = '\0';
            value = value + 1;
            break;
        }
        value++;
    }

    // Skip spaces
    char *tmp = name;
    while ((tmp < value) && (*tmp != '\0')) {
        if (isspace(*tmp)) {
            (*tmp) = '\0';
            break;
        }
        tmp++;
    }

    // empty name, just ignore this line
    if (*value == '\0') {
        return 0;
    }

    // Filter excluded parameters
    for (uint32_t i = 0; i < count; i++) {
        if (strncmp(name, exclude[i], strlen(exclude[i])) == 0) {
            return 0;
        }
    }

    // Skip spaces for value
    while (isspace(*value) && (*value != '\0')) {
        value++;
    }

    // Trim the ending spaces of value
    char *pos = value + strlen(value);
    pos--;
    while (isspace(*pos) && pos > value) {
        (*pos) = '\0';
        pos--;
    }

    // Strip starting and ending " for value
    if ((*value == '"') && (pos > value) && (*pos == '"')) {
        value = value + 1;
        *pos = '\0';
    }
    return result(context, name, value);
}