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

#include "param_manager.h"

#include <ctype.h>
#include <limits.h>

#include "param_trie.h"

static ParamWorkSpace g_paramWorkSpace = {};

static int WorkSpaceNodeCompare(const HashNode *node1, const HashNode *node2)
{
    WorkSpace *workSpace1 = HASHMAP_ENTRY(node1, WorkSpace, hashNode);
    WorkSpace *workSpace2 = HASHMAP_ENTRY(node2, WorkSpace, hashNode);
    return strcmp(workSpace1->fileName, workSpace2->fileName);
}

static int WorkSpaceKeyCompare(const HashNode *node1, const void *key)
{
    WorkSpace *workSpace1 = HASHMAP_ENTRY(node1, WorkSpace, hashNode);
    return strcmp(workSpace1->fileName, (char *)key);
}

static int GenerateKeyHasCode(const char *buff, uint32_t len)
{
    int code = 0;
    for (size_t i = 0; i < len; i++) {
        code += buff[i] - 'A';
    }
    return code;
}

static int WorkSpaceGetNodeHasCode(const HashNode *node)
{
    WorkSpace *workSpace = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    size_t nameLen = strlen(workSpace->fileName);
    return GenerateKeyHasCode(workSpace->fileName, nameLen);
}

static int WorkSpaceGetKeyHasCode(const void *key)
{
    const char *buff = (char *)key;
    return GenerateKeyHasCode(buff, strlen(buff));
}

static void WorkSpaceFree(const HashNode *node)
{
    WorkSpace *workSpace = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    CloseWorkSpace(workSpace);
}

static ParamHandle GetParamHandle(const WorkSpace *workSpace, uint32_t index, const char *name)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return -1, "Invalid param");
    uint32_t hashCode = (uint32_t)GenerateKeyHasCode(workSpace->fileName, strlen(workSpace->fileName));
    uint32_t handle = (hashCode % HASH_BUTT) << 24; // 24 left shift
    handle = handle | (index + workSpace->area->startIndex);
    return handle;
}

static int InitParamSecurity(ParamWorkSpace *workSpace,
    RegisterSecurityOpsPtr registerOps, ParamSecurityType type, int isInit, int op)
{
    PARAM_CHECK(workSpace != NULL && type < PARAM_SECURITY_MAX, return -1, "Invalid param");
    int ret = 0;
    if (registerOps != NULL) {
        ret = registerOps(&workSpace->paramSecurityOps[type], isInit);
        PARAM_CHECK(workSpace->paramSecurityOps[type].securityInitLabel != NULL,
            return -1, "Invalid securityInitLabel");
        ret = workSpace->paramSecurityOps[type].securityInitLabel(&workSpace->securityLabel, isInit);
        PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to init security");
    }

    ParamSecurityOps *paramSecurityOps = &workSpace->paramSecurityOps[type];
    PARAM_CHECK(paramSecurityOps->securityFreeLabel != NULL, return -1, "Invalid securityFreeLabel");
    PARAM_CHECK(paramSecurityOps->securityCheckFilePermission != NULL, return -1, "Invalid securityCheck");
    PARAM_CHECK(paramSecurityOps->securityCheckParamPermission != NULL, return -1, "Invalid securityCheck");
    if (isInit == LABEL_INIT_FOR_INIT) {
        PARAM_CHECK(paramSecurityOps->securityGetLabel != NULL, return -1, "Invalid securityGetLabel");
    }
    ret = paramSecurityOps->securityCheckFilePermission(&workSpace->securityLabel, PARAM_STORAGE_PATH, op);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "No permission to read file %s", PARAM_STORAGE_PATH);
    PARAM_LOGI("InitParamSecurity %s success", paramSecurityOps->name);
    return 0;
}

PARAM_STATIC int RegisterSecurityOps(int onlyRead)
{
    int isInit = 0;
    int op = DAC_READ;
    if (onlyRead == 0) {
        isInit = LABEL_INIT_FOR_INIT;
        op = DAC_WRITE;
    }
    int ret = InitParamSecurity(&g_paramWorkSpace, RegisterSecurityDacOps, PARAM_SECURITY_DAC, isInit, op);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");
#ifdef PARAM_SUPPORT_SELINUX
    ret = InitParamSecurity(&g_paramWorkSpace, RegisterSecuritySelinuxOps, PARAM_SECURITY_SELINUX, isInit, op);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");
#endif
    return ret;
}

int InitParamWorkSpace(int onlyRead)
{
    if (PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    paramMutexEnvInit();
    HashInfo info = {
        WorkSpaceNodeCompare,
        WorkSpaceKeyCompare,
        WorkSpaceGetNodeHasCode,
        WorkSpaceGetKeyHasCode,
        WorkSpaceFree,
        HASH_BUTT
    };
    int ret = HashMapCreate(&g_paramWorkSpace.workSpaceHashHandle, &info);
    PARAM_CHECK(ret == 0, return -1, "Failed to create hash map for workspace");
    WORKSPACE_INIT_LOCK(g_paramWorkSpace);
    ListInit(&g_paramWorkSpace.workSpaceList);

    ret = RegisterSecurityOps(onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");
    PARAM_SET_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT);

#ifndef PARAM_SUPPORT_SELINUX
    ret = AddWorkSpace(WORKSPACE_NAME_NORMAL, onlyRead, PARAM_WORKSPACE_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed to add dac workspace");
#endif
    // add dac workspace
    ret = AddWorkSpace(WORKSPACE_NAME_DAC, onlyRead, PARAM_WORKSPACE_SMALL);
    PARAM_CHECK(ret == 0, return -1, "Failed to add dac workspace");
    if (onlyRead == 0) {
        // load user info for dac
        LoadGroupUser();
        // add default dac policy
        ParamAuditData auditData = {};
        auditData.name = "#";
        auditData.dacData.gid = DAC_DEFAULT_GROUP; // 2000 for shell
        auditData.dacData.uid = DAC_DEFAULT_USER; // for root
        auditData.dacData.mode = DAC_DEFAULT_MODE; // 0774 default mode
        ret = AddSecurityLabel(&auditData);
        PARAM_CHECK(ret == 0, return ret, "Failed to add default dac label");
    }
    return ret;
}

void CloseParamWorkSpace(void)
{
    PARAM_LOGI("CloseParamWorkSpace");
    WORKSPACE_RW_LOCK(g_paramWorkSpace);
    if (g_paramWorkSpace.workSpaceHashHandle != NULL) {
        HashMapDestory(g_paramWorkSpace.workSpaceHashHandle);
        g_paramWorkSpace.workSpaceHashHandle = NULL;
    }
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    for (int i = 0; i < PARAM_SECURITY_MAX; i++) {
        if (g_paramWorkSpace.paramSecurityOps[i].securityFreeLabel != NULL) {
            g_paramWorkSpace.paramSecurityOps[i].securityFreeLabel(&g_paramWorkSpace.securityLabel);
        }
    }
#ifdef PARAMWORKSPACE_NEED_MUTEX
    ParamRWMutexDelete(&g_paramWorkSpace.rwlock);
#endif
    g_paramWorkSpace.flags = 0;
}

static uint32_t ReadCommitId(ParamNode *entry)
{
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_acquire);
    while (commitId & PARAM_FLAGS_MODIFY) {
        futex_wait(&entry->commitId, commitId);
        commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_acquire);
    }
    return commitId & PARAM_FLAGS_COMMITID;
}

int ReadParamCommitId(ParamHandle handle, uint32_t *commitId)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return -1;
    }
    *commitId = ReadCommitId(entry);
    return 0;
}

int ReadParamWithCheck(const char *name, uint32_t op, ParamHandle *handle)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(handle != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param handle");
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param name");
    *handle = -1;
    int ret = CheckParamPermission(&g_paramWorkSpace.securityLabel, name, op);
    PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
#ifdef PARAM_SUPPORT_SELINUX
    if (ret == DAC_RESULT_PERMISSION) {
        const char *label = GetSelinuxContent(name);
        if (label != NULL) {
            AddWorkSpace(label, 1, PARAM_WORKSPACE_DEF);
        } else {
            AddWorkSpace(WORKSPACE_NAME_DEF_SELINUX, 1, PARAM_WORKSPACE_DEF);
        }
    }
#endif
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

int ReadParamValue(ParamHandle handle, char *value, uint32_t *length)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(length != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return -1;
    }
    if (value == NULL) {
        *length = entry->valueLength + 1;
        return 0;
    }
    PARAM_CHECK(*length > entry->valueLength, return PARAM_CODE_INVALID_PARAM,
        "Invalid value len %u %u", *length, entry->valueLength);
    uint32_t commitId = ReadCommitId(entry);
    do {
        int ret = memcpy_s(value, *length, entry->data + entry->keyLength + 1, entry->valueLength);
        PARAM_CHECK(ret == EOK, return -1, "Failed to copy value");
        value[entry->valueLength] = '\0';
        *length = entry->valueLength;
    } while (commitId != ReadCommitId(entry));
    return 0;
}

int ReadParamName(ParamHandle handle, char *name, uint32_t length)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return -1;
    }
    PARAM_CHECK(length > entry->keyLength, return -1, "Invalid param size %u %u", entry->keyLength, length);
    int ret = memcpy_s(name, length, entry->data, entry->keyLength);
    PARAM_CHECK(ret == EOK, return PARAM_CODE_INVALID_PARAM, "Failed to copy name");
    name[entry->keyLength] = '\0';
    return 0;
}

int CheckParamValue(const ParamTrieNode *node, const char *name, const char *value)
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

int CheckParamName(const char *name, int info)
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
        int ret = memcpy_s(entry->data + entry->keyLength + 1, PARAM_VALUE_LEN_MAX, value, valueLen + 1);
        PARAM_CHECK(ret == EOK, return PARAM_CODE_INVALID_VALUE, "Failed to copy value");
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

int WriteParam(const char *name, const char *value, uint32_t *dataIndex, int mode)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
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

int AddSecurityLabel(const ParamAuditData *auditData)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
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

ParamNode *SystemCheckMatchParamWait(const char *name, const char *value)
{
    WorkSpace *worksapce = GetWorkSpace(name);
    PARAM_CHECK(worksapce != NULL, return NULL, "Failed to get workspace %s", name);
    PARAM_LOGV("SystemCheckMatchParamWait name %s", name);
    uint32_t nameLength = strlen(name);
    ParamTrieNode *node = FindTrieNode(worksapce, name, nameLength, NULL);
    if (node == NULL || node->dataIndex == 0) {
        return NULL;
    }
    ParamNode *param = (ParamNode *)GetTrieNode(worksapce, node->dataIndex);
    if (param == NULL) {
        return NULL;
    }
    if ((param->keyLength != nameLength) || (strncmp(param->data, name, nameLength) != 0)) {  // compare name
        return NULL;
    }
    ATOMIC_STORE_EXPLICIT(&param->commitId,
        ATOMIC_LOAD_EXPLICIT(&param->commitId, memory_order_relaxed) | PARAM_FLAGS_WAITED, memory_order_release);
    if ((strncmp(value, "*", 1) == 0) || (strcmp(param->data + nameLength + 1, value) == 0)) { // compare value
        return param;
    }
    char *tmp = strstr(value, "*");
    if (tmp != NULL && (strncmp(param->data + nameLength + 1, value, tmp - value) == 0)) {
        return param;
    }
    return NULL;
}

static int ProcessParamTraversal(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie)
{
    ParamTraversalContext *context = (ParamTraversalContext *)cookie;
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (current->dataIndex == 0) {
        return 0;
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
    if (entry == NULL) {
        return 0;
    }
    if ((strcmp("#", context->prefix) != 0) && (strncmp(entry->data, context->prefix, strlen(context->prefix)) != 0)) {
        return 0;
    }
    uint32_t index = GetParamHandle(workSpace, current->dataIndex, entry->data);
    context->traversalParamPtr(index, context->context);
    return 0;
}

int SystemTraversalParameter(const char *prefix, TraversalParamPtr traversalParameter, void *cookie)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(traversalParameter != NULL, return -1, "The param is null");

    ParamTraversalContext context = {traversalParameter, cookie, "#"};
    if (!(prefix == NULL || strlen(prefix) == 0)) {
        ParamHandle handle = 0;
        // only check for valid parameter
        int ret = ReadParamWithCheck(prefix, DAC_READ, &handle);
        if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
            PARAM_CHECK(ret == 0, return ret, "Forbid to traversal parameters");
        }
        context.prefix = (char *)prefix;
    }
#ifdef PARAM_SUPPORT_SELINUX
    OpenPermissionWorkSpace();
#endif
    WorkSpace *workSpace = GetFirstWorkSpace();
    if (workSpace != NULL && strcmp(workSpace->fileName, WORKSPACE_NAME_DAC) == 0) {
        workSpace = GetNextWorkSpace(workSpace);
    }
    while (workSpace != NULL) {
        PARAM_LOGV("SystemTraversalParameter prefix %s in space %s", context.prefix, workSpace->fileName);
        WorkSpace *next = GetNextWorkSpace(workSpace);
        ParamTrieNode *root = NULL;
        if (prefix != NULL && strlen(prefix) != 0) {
            root = FindTrieNode(workSpace, prefix, strlen(prefix), NULL);
        }
        PARAMSPACE_AREA_RD_LOCK(workSpace);
        TraversalTrieNode(workSpace, root, ProcessParamTraversal, (const void *)&context);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
        workSpace = next;
    }
    return 0;
}

int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    PARAM_CHECK(srcLabel != NULL, return DAC_RESULT_FORBIDED, "The srcLabel is null");
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return DAC_RESULT_FORBIDED, "Invalid space");
    int ret = DAC_RESULT_PERMISSION;
    // for root, all permission
    if (srcLabel->cred.uid != 0) {
        for (int i = 0; i < PARAM_SECURITY_MAX; i++) {
            if (PARAM_TEST_FLAG(g_paramWorkSpace.securityLabel.flags[i], LABEL_ALL_PERMISSION)) {
                continue;
            }
            if (g_paramWorkSpace.paramSecurityOps[i].securityCheckParamPermission == NULL) {
                ret = DAC_RESULT_FORBIDED;
                continue;
            }
            ret = g_paramWorkSpace.paramSecurityOps[i].securityCheckParamPermission(srcLabel, name, mode);
            PARAM_LOGV("CheckParamPermission %s %s ret %d", g_paramWorkSpace.paramSecurityOps[i].name, name, ret);
            if (ret == DAC_RESULT_PERMISSION) {
                break;
            }
        }
    }
    return ret;
}

static int DumpTrieDataNodeTraversal(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie)
{
    int verbose = *(int *)cookie;
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (verbose) {
        PARAM_DUMP("\tTrie node info [%u,%u,%u] data: %u label: %u key length:%d \n\t  key: %s \n",
            current->left, current->right, current->child,
            current->dataIndex, current->labelIndex, current->length, current->key);
    }
    if (current->dataIndex != 0) {
        ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
        if (entry != NULL) {
            PARAM_DUMP("\tparameter length info [%d, %d] \n\t  param: %s \n",
                entry->keyLength, entry->valueLength, (entry != NULL) ? entry->data : "null");
        }
    }
    if (current->labelIndex != 0 && verbose) {
        ParamSecurityNode *label = (ParamSecurityNode *)GetTrieNode(workSpace, current->labelIndex);
        if (label != NULL) {
            PARAM_DUMP("\tparameter label dac %d %d %o \n\t  label: %s \n",
                label->uid, label->gid, label->mode, (label->length > 0) ? label->data : "null");
        }
    }
    return 0;
}

static void HashNodeTraverseForDump(WorkSpace *workSpace, int verbose)
{
    PARAM_DUMP("    map file: %s \n", workSpace->fileName);
    if (workSpace->area != NULL) {
        PARAM_DUMP("    total size: %d \n", workSpace->area->dataSize);
        PARAM_DUMP("    first offset: %d \n", workSpace->area->firstNode);
        PARAM_DUMP("    current offset: %d \n", workSpace->area->currOffset);
        PARAM_DUMP("    total node: %d \n", workSpace->area->trieNodeCount);
        PARAM_DUMP("    total param node: %d \n", workSpace->area->paramNodeCount);
        PARAM_DUMP("    total security node: %d\n", workSpace->area->securityNodeCount);
    }
    PARAM_DUMP("    node info: \n");
    PARAMSPACE_AREA_RD_LOCK(workSpace);
    TraversalTrieNode(workSpace, NULL, DumpTrieDataNodeTraversal, (const void *)&verbose);
    PARAMSPACE_AREA_RW_UNLOCK(workSpace);
}

void SystemDumpParameters(int verbose)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return, "Invalid space");
    // check default dac
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck("#", DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return, "Forbid to dump parameters");
    }
#ifdef PARAM_SUPPORT_SELINUX
    // open all workspace
    OpenPermissionWorkSpace();
#endif
    PARAM_DUMP("Dump all paramters begin ...\n");
    if (verbose) {
        PARAM_DUMP("Local sercurity information\n");
        PARAM_DUMP("\t pid: %d uid: %d gid: %d \n",
            g_paramWorkSpace.securityLabel.cred.pid,
            g_paramWorkSpace.securityLabel.cred.uid,
            g_paramWorkSpace.securityLabel.cred.gid);
    }
    WorkSpace *workSpace = GetFirstWorkSpace();
    while (workSpace != NULL) {
        WorkSpace *next = GetNextWorkSpace(workSpace);
        HashNodeTraverseForDump(workSpace, verbose);
        workSpace = next;
    }
    PARAM_DUMP("Dump all paramters finish\n");
}

int AddWorkSpace(const char *name, int onlyRead, uint32_t spaceSize)
{
    int ret = 0;
    // check exist
#ifdef PARAM_SUPPORT_SELINUX
    const char *realName = name;
#else
    const char *realName = WORKSPACE_NAME_NORMAL;
#endif
    WORKSPACE_RW_LOCK(g_paramWorkSpace);
    HashNode *node = HashMapGet(g_paramWorkSpace.workSpaceHashHandle, (const void *)realName);
    if (node != NULL) {
        WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
        return 0;
    }
    PARAM_LOGI("AddWorkSpace %s spaceSize: %u onlyRead %s", name, spaceSize, onlyRead ? "true" : "false");
    WorkSpace *workSpace = NULL;
    do {
        ret = -1;
        const size_t size = strlen(realName) + 1;
        workSpace = (WorkSpace *)malloc(sizeof(WorkSpace) + size);
        PARAM_CHECK(workSpace != NULL, break, "Failed to create workspace for %s", realName);
        workSpace->flags = 0;
        workSpace->area = NULL;
        ListInit(&workSpace->node);
        ret = strcpy_s(workSpace->fileName, size, realName);
        PARAM_CHECK(ret == 0, break, "Failed to copy file name %s", realName);
        HASHMAPInitNode(&workSpace->hashNode);
        ret = InitWorkSpace(workSpace, onlyRead, spaceSize);
        PARAM_CHECK(ret == 0, break, "Failed to init workspace %s", realName);
        ret = HashMapAdd(g_paramWorkSpace.workSpaceHashHandle, &workSpace->hashNode);
        PARAM_CHECK(ret == 0, CloseWorkSpace(workSpace);
            workSpace = NULL;
            break, "Failed to add hash node");
        ListAddTail(&g_paramWorkSpace.workSpaceList, &workSpace->node);
        ret = 0;
        workSpace = NULL;
    } while (0);
    if (workSpace != NULL) {
        free(workSpace);
    }
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    PARAM_LOGI("AddWorkSpace %s %s", name, ret == 0 ? "success" : "fail");
    return ret;
}

WorkSpace *GetWorkSpace(const char *name)
{
    PARAM_CHECK(g_paramWorkSpace.workSpaceHashHandle != NULL, return NULL, "Invalid workSpaceHashHandle");
    char *tmpName = (char *)name;
#ifndef PARAM_SUPPORT_SELINUX
    tmpName = WORKSPACE_NAME_NORMAL;
#else
    if (strcmp(name, WORKSPACE_NAME_DAC) != 0) {
        tmpName = (char *)GetSelinuxContent(name);
    }
#endif
    WorkSpace *space = NULL;
    WORKSPACE_RD_LOCK(g_paramWorkSpace);
    HashNode *node = HashMapGet(g_paramWorkSpace.workSpaceHashHandle, (const void *)tmpName);
    if (node != NULL) {
        space = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    }
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    PARAM_LOGV("GetWorkSpace %s space-name %s, space %p", name, tmpName, space);
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

ParamTrieNode *GetTrieNodeByHandle(ParamHandle handle)
{
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return NULL);
    int hashCode = ((handle >> 24) & 0x000000ff);  // 24 left shift
    uint32_t index = handle & 0x00ffffff;
    WORKSPACE_RD_LOCK(g_paramWorkSpace);
    HashNode *node = HashMapFind(g_paramWorkSpace.workSpaceHashHandle, hashCode, (const void *)&index, CompareIndex);
    if (node == NULL) {
        WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
        PARAM_LOGV("Failed to get workspace for 0x%x index %d hashCode %d", handle, index, hashCode);
        return NULL;
    }
    WorkSpace *workSpace = HASHMAP_ENTRY(node, WorkSpace, hashNode);
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    index = index - workSpace->area->startIndex;
    return (ParamTrieNode *)GetTrieNode(workSpace, index);
}

WorkSpace *GetFirstWorkSpace(void)
{
    WorkSpace *workSpace = NULL;
    WORKSPACE_RD_LOCK(g_paramWorkSpace);
    ListNode *node = g_paramWorkSpace.workSpaceList.next;
    if (node != &g_paramWorkSpace.workSpaceList) {
        workSpace = HASHMAP_ENTRY(node, WorkSpace, node);
    }
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    return workSpace;
}

WorkSpace *GetNextWorkSpace(WorkSpace *curr)
{
    PARAM_CHECK(curr != NULL, return NULL, "Invalid curr");
    WorkSpace *workSpace = NULL;
    WORKSPACE_RD_LOCK(g_paramWorkSpace);
    ListNode *node = curr->node.next;
    if (node != &g_paramWorkSpace.workSpaceList) {
        workSpace = HASHMAP_ENTRY(node, WorkSpace, node);
    }
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    return workSpace;
}

int SystemReadParam(const char *name, char *value, uint32_t *len)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL && len != NULL && strlen(name) > 0, return -1, "The name or value is null");
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to get parameter %s", name);
    }
    return ReadParamValue(handle, value, len);
}

int SysCheckParamExist(const char *name)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL, return -1, "The name or handle is null");
#ifdef PARAM_SUPPORT_SELINUX
    // open all workspace
    OpenPermissionWorkSpace();
#endif
    WorkSpace *workSpace = GetFirstWorkSpace();
    while (workSpace != NULL) {
        PARAM_LOGV("SysCheckParamExist name %s in space %s", name, workSpace->fileName);
        WorkSpace *next = GetNextWorkSpace(workSpace);
        ParamTrieNode *node = FindTrieNode(workSpace, name, strlen(name), NULL);
        if (node != NULL && node->dataIndex != 0) {
            return 0;
        } else if (node != NULL) {
            return PARAM_CODE_NODE_EXIST;
        }
        workSpace = next;
    }
    return PARAM_CODE_NOT_FOUND;
}

int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(handle != 0 || commitId != NULL, return -1, "The handle is null");
    return ReadParamCommitId(handle, commitId);
}

long long GetSystemCommitId(void)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return 0, "Invalid space");
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space == NULL || space->area == NULL) {
        return 0;
    }
    return ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_acquire);
}

int SystemGetParameterName(ParamHandle handle, char *name, unsigned int len)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL && handle != 0, return -1, "The name is null");
    return ReadParamName(handle, name, len);
}

int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(len != NULL && handle != 0, return -1, "The value is null");
    return ReadParamValue(handle, value, len);
}

int GetParamSecurityAuditData(const char *name, int type, ParamAuditData *auditData)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    uint32_t labelIndex = 0;
    // get from dac
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    PARAM_CHECK(space != NULL, return -1, "Invalid workSpace");
    FindTrieNode(space, name, strlen(name), &labelIndex);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);

    auditData->name = name;
    auditData->dacData.uid = node->uid;
    auditData->dacData.gid = node->gid;
    auditData->dacData.mode = node->mode;
#ifdef PARAM_SUPPORT_SELINUX
    const char *tmpName = GetSelinuxContent(name);
    if (tmpName != NULL) {
        int ret = strcpy_s(auditData->label, sizeof(auditData->label), tmpName);
        PARAM_CHECK(ret == 0, return 0, "Failed to copy label for %s", name);
    }
#endif
    return 0;
}

int CheckParameterSet(const char *name, const char *value, const ParamSecurityLabel *srcLabel, int *ctrlService)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_LOGV("CheckParameterSet name %s value: %s", name, value);
    PARAM_CHECK(srcLabel != NULL && ctrlService != NULL, return -1, "Invalid param ");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    ret = CheckParamValue(NULL, name, value);
    PARAM_CHECK(ret == 0, return ret, "Illegal param value %s", value);
    *ctrlService = 0;

#ifndef __LITEOS_M__
    if (getpid() != 1) { // none init
#ifdef PARAM_SUPPORT_SELINUX
        *ctrlService |= PARAM_NEED_CHECK_IN_SERVICE;
        return 0;
#else
        if ((srcLabel->flags[0] & LABEL_CHECK_IN_ALL_PROCESS) != LABEL_CHECK_IN_ALL_PROCESS) {
            *ctrlService |= PARAM_NEED_CHECK_IN_SERVICE;
            return 0;
        }
#endif
    }
#endif
    char *key = GetServiceCtrlName(name, value);
    ret = CheckParamPermission(srcLabel, (key == NULL) ? name : key, DAC_WRITE);
    if (key != NULL) {  // ctrl param
        free(key);
        *ctrlService |= PARAM_CTRL_SERVICE;
    }
    return ret;
}

int LoadSecurityLabel(const char *fileName)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(fileName != NULL, return -1, "Invalid fielname for load");
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
    // load security label
    ParamSecurityOps *ops = &g_paramWorkSpace.paramSecurityOps[PARAM_SECURITY_DAC];
    if (ops->securityGetLabel != NULL) {
        ops->securityGetLabel(fileName);
    }
#endif
    return 0;
}

void LoadSelinuxLabel(void)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return, "Invalid space");
    // load security label
#ifdef PARAM_SUPPORT_SELINUX
    ParamSecurityOps *ops = &g_paramWorkSpace.paramSecurityOps[PARAM_SECURITY_SELINUX];
    if (ops->securityGetLabel != NULL) {
        ops->securityGetLabel(NULL);
    }
#endif
}

ParamSecurityLabel *GetParamSecurityLabel()
{
    return &g_paramWorkSpace.securityLabel;
}

long long GetPersistCommitId(void)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space == NULL || space->area == NULL) {
        return 0;
    }
    return ATOMIC_LOAD_EXPLICIT(&space->area->commitPersistId, memory_order_acquire);
}

void UpdatePersistCommitId(void)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return, "Invalid space");
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space == NULL || space->area == NULL) {
        return;
    }
    long long globalCommitId = ATOMIC_LOAD_EXPLICIT(&space->area->commitPersistId, memory_order_relaxed);
    ATOMIC_STORE_EXPLICIT(&space->area->commitPersistId, ++globalCommitId, memory_order_release);
}

#if defined STARTUP_INIT_TEST || defined LOCAL_TEST
ParamSecurityOps *GetParamSecurityOps(int type)
{
    PARAM_CHECK(type < PARAM_SECURITY_MAX, return NULL, "Invalid type");
    return &g_paramWorkSpace.paramSecurityOps[type];
}
#endif
