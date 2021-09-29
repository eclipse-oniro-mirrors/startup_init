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

#define LABEL "Manager"
#if !defined PARAM_SUPPORT_SELINUX && !defined PARAM_SUPPORT_DAC
static ParamSecurityLabel g_defaultSecurityLabel;
#endif

static int GetParamSecurityOps(ParamWorkSpace *workSpace, int isInit)
{
    UNUSED(isInit);
#if (defined PARAM_SUPPORT_SELINUX || defined PARAM_SUPPORT_DAC)
    int ret = RegisterSecurityOps(&workSpace->paramSecurityOps, isInit);
    PARAM_CHECK(workSpace->paramSecurityOps.securityInitLabel != NULL, return -1, "Invalid securityInitLabel");
    ret = workSpace->paramSecurityOps.securityInitLabel(&workSpace->securityLabel, isInit);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to init security");
#else
    workSpace->securityLabel = &g_defaultSecurityLabel;
    workSpace->securityLabel->flags |= LABEL_ALL_PERMISSION;
#endif
    return 0;
}

int InitParamWorkSpace(ParamWorkSpace *workSpace, int onlyRead)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_NAME, "Invalid param");
    if (PARAM_TEST_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    int isInit = 0;
    int op = DAC_READ;
    if (onlyRead == 0) {
        isInit = LABEL_INIT_FOR_INIT;
        op = DAC_WRITE;
    }
    int ret = GetParamSecurityOps(workSpace, isInit);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");
    ParamSecurityOps *paramSecurityOps = &workSpace->paramSecurityOps;
    if (!LABEL_IS_ALL_PERMITTED(workSpace->securityLabel)) {
        PARAM_CHECK(paramSecurityOps->securityFreeLabel != NULL, return -1, "Invalid securityFreeLabel");
        PARAM_CHECK(paramSecurityOps->securityCheckFilePermission != NULL, return -1, "Invalid securityCheck");
        PARAM_CHECK(paramSecurityOps->securityCheckParamPermission != NULL, return -1, "Invalid securityCheck");
        if (isInit == LABEL_INIT_FOR_INIT) {
            PARAM_CHECK(paramSecurityOps->securityGetLabel != NULL, return -1, "Invalid securityGetLabel");
            PARAM_CHECK(paramSecurityOps->securityDecodeLabel != NULL, return -1, "Invalid securityDecodeLabel");
        } else {
            PARAM_CHECK(paramSecurityOps->securityEncodeLabel != NULL, return -1, "Invalid securityEncodeLabel");
        }
        ret = paramSecurityOps->securityCheckFilePermission(workSpace->securityLabel, PARAM_STORAGE_PATH, op);
        PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "No permission to read file %s", PARAM_STORAGE_PATH);
    }
    ret = InitWorkSpace(PARAM_STORAGE_PATH, &workSpace->paramSpace, onlyRead);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to init workspace");
    PARAM_SET_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT);
    return ret;
}

void CloseParamWorkSpace(ParamWorkSpace *workSpace)
{
    CloseWorkSpace(&workSpace->paramSpace);
    if (workSpace->paramSecurityOps.securityFreeLabel != NULL) {
        workSpace->paramSecurityOps.securityFreeLabel(workSpace->securityLabel);
    }
    workSpace->flags = 0;
}

static uint32_t ReadCommitId(ParamNode *entry)
{
    uint32_t commitId = atomic_load_explicit(&entry->commitId, memory_order_acquire);
    while (commitId & PARAM_FLAGS_MODIFY) {
        futex_wait(&entry->commitId, commitId);
        commitId = atomic_load_explicit(&entry->commitId, memory_order_acquire);
    }
    return commitId & PARAM_FLAGS_COMMITID;
}

int ReadParamCommitId(const ParamWorkSpace *workSpace, ParamHandle handle, uint32_t *commitId)
{
    PARAM_CHECK(workSpace != NULL && commitId != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    ParamNode *entry = (ParamNode *)GetTrieNode(&workSpace->paramSpace, handle);
    if (entry == NULL) {
        return -1;
    }
    *commitId = ReadCommitId(entry);
    return 0;
}

int ReadParamWithCheck(const ParamWorkSpace *workSpace, const char *name, uint32_t op, ParamHandle *handle)
{
    PARAM_CHECK(handle != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param handle");
    PARAM_CHECK(workSpace != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param name");
    *handle = -1;
    int ret = CheckParamPermission(workSpace, workSpace->securityLabel, name, op);
    PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);

    ParamTrieNode *node = FindTrieNode(&workSpace->paramSpace, name, strlen(name), NULL);
    if (node != NULL && node->dataIndex != 0) {
        *handle = node->dataIndex;
        return 0;
    }
    return PARAM_CODE_NOT_FOUND;
}

int ReadParamValue(const ParamWorkSpace *workSpace, ParamHandle handle, char *value, uint32_t *length)
{
    PARAM_CHECK(workSpace != NULL && length != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamNode *entry = (ParamNode *)GetTrieNode(&workSpace->paramSpace, handle);
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

int ReadParamName(const ParamWorkSpace *workSpace, ParamHandle handle, char *name, uint32_t length)
{
    PARAM_CHECK(workSpace != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamNode *entry = (ParamNode *)GetTrieNode(&workSpace->paramSpace, handle);
    if (entry == NULL) {
        return -1;
    }
    PARAM_CHECK(length > entry->keyLength, return -1, "Invalid param size %u %u", entry->keyLength, length);
    int ret = memcpy_s(name, length, entry->data, entry->keyLength);
    PARAM_CHECK(ret == EOK, return PARAM_CODE_INVALID_PARAM, "Failed to copy name");
    name[entry->keyLength] = '\0';
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

static int ProcessParamTraversal(const WorkSpace *workSpace, const ParamTrieNode *node, void *cookie)
{
    UNUSED(workSpace);
    ParamTraversalContext *context = (ParamTraversalContext *)cookie;
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (current->dataIndex == 0) {
        return 0;
    }
    context->traversalParamPtr(current->dataIndex, context->context);
    return 0;
}

int TraversalParam(const ParamWorkSpace *workSpace, TraversalParamPtr walkFunc, ParamContextPtr cookie)
{
    PARAM_CHECK(workSpace != NULL && walkFunc != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamTraversalContext context = {
        walkFunc, cookie
    };
    return TraversalTrieNode(&workSpace->paramSpace, NULL, ProcessParamTraversal, &context);
}

int CheckParamPermission(const ParamWorkSpace *workSpace,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    PARAM_CHECK(workSpace != NULL && workSpace->securityLabel != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid param");
    if (LABEL_IS_ALL_PERMITTED(workSpace->securityLabel)) {
        return 0;
    }
    PARAM_CHECK(name != NULL && srcLabel != NULL, return -1, "Invalid param");
    if (workSpace->paramSecurityOps.securityCheckParamPermission == NULL) {
        return DAC_RESULT_FORBIDED;
    }
    uint32_t labelIndex = 0;
    FindTrieNode(&workSpace->paramSpace, name, strlen(name), &labelIndex);
    ParamSecruityNode *node = (ParamSecruityNode *)GetTrieNode(&workSpace->paramSpace, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);

    ParamAuditData auditData = {};
    auditData.name = name;
    auditData.dacData.uid = node->uid;
    auditData.dacData.gid = node->gid;
    auditData.dacData.mode = node->mode;
    auditData.label = node->data;
    return workSpace->paramSecurityOps.securityCheckParamPermission(srcLabel, &auditData, mode);
}

static int DumpTrieDataNodeTraversal(const WorkSpace *workSpace, const ParamTrieNode *node, void *cookie)
{
    int verbose = *(int *)cookie;
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (verbose) {
        printf("\tTrie node info [%u,%u,%u] data: %u label: %u key length:%d \n\t  key: %s \n",
            current->left, current->right, current->child,
            current->dataIndex, current->labelIndex, current->length, current->key);
    }
    if (current->dataIndex != 0) {
        ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
        if (entry != NULL) {
            printf("\tparameter length info [%d, %d] \n\t  param: %s \n",
                entry->keyLength, entry->valueLength, (entry != NULL) ? entry->data : "null");
        }
    }
    if (current->labelIndex != 0 && verbose) {
        ParamSecruityNode *label = (ParamSecruityNode *)GetTrieNode(workSpace, current->labelIndex);
        if (label != NULL) {
            printf("\tparameter label dac %d %d %o \n\t  label: %s \n",
                label->uid, label->gid, label->mode, (label->length > 0) ? label->data : "null");
        }
    }
    return 0;
}

static void DumpWorkSpace(const ParamWorkSpace *workSpace, int verbose)
{
    printf("workSpace information \n");
    printf("    map file: %s \n", workSpace->paramSpace.fileName);
    printf("    total size: %d \n", workSpace->paramSpace.area->dataSize);
    printf("    first offset: %d \n", workSpace->paramSpace.area->firstNode);
    printf("    current offset: %d \n", workSpace->paramSpace.area->currOffset);
    printf("    total node: %d \n", workSpace->paramSpace.area->trieNodeCount);
    printf("    total param node: %d \n", workSpace->paramSpace.area->paramNodeCount);
    printf("    total security node: %d\n", workSpace->paramSpace.area->securityNodeCount);
    printf("    node info: \n");
    TraversalTrieNode(&workSpace->paramSpace, NULL, DumpTrieDataNodeTraversal, (void *)&verbose);
}

void DumpParameters(const ParamWorkSpace *workSpace, int verbose)
{
    PARAM_CHECK(workSpace != NULL && workSpace->securityLabel != NULL, return, "Invalid param");
    printf("Dump all paramters begin ...\n");
    DumpWorkSpace(workSpace, verbose);
    if (verbose) {
        printf("Local sercurity information\n");
        printf("\t pid: %d uid: %d gid: %d \n",
            workSpace->securityLabel->cred.pid,
            workSpace->securityLabel->cred.uid,
            workSpace->securityLabel->cred.gid);
    }
    printf("Dump all paramters finish\n");
}
