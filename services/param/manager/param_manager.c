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
#include <dlfcn.h>
#ifdef WITH_SELINUX
#include "selinux_parameter.h"
#endif

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
    if (onlyRead) {
        ret = InitWorkSpace(CLIENT_PARAM_STORAGE_PATH, &workSpace->paramSpace, onlyRead);
    } else {
        ret = InitWorkSpace(PARAM_STORAGE_PATH, &workSpace->paramSpace, onlyRead);
    }
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to init workspace");
    PARAM_SET_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT);
    return ret;
}

void CloseParamWorkSpace(ParamWorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return, "Invalid workSpace");
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
#ifdef READ_CHECK
    int ret = CheckParamPermission(workSpace, workSpace->securityLabel, name, op);
    PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
#endif
    ParamTrieNode *node = FindTrieNode(&workSpace->paramSpace, name, strlen(name), NULL);
    if (node != NULL && node->dataIndex != 0) {
        *handle = node->dataIndex;
        return 0;
    } else if (node != NULL) {
        return PARAM_CODE_NODE_EXIST;
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

int CheckParamValue(const WorkSpace *workSpace, const ParamTrieNode *node, const char *name, const char *value)
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
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
    if (entry == NULL) {
        return 0;
    }
    if ((strcmp("#", context->prefix) != 0) &&
        (strncmp(entry->data, context->prefix, strlen(context->prefix)) != 0)) {
        return 0;
    }
    context->traversalParamPtr(current->dataIndex, context->context);
    return 0;
}

int TraversalParam(const ParamWorkSpace *workSpace,
    const char *prefix, TraversalParamPtr walkFunc, void *cookie)
{
    PARAM_CHECK(workSpace != NULL && walkFunc != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamTraversalContext context = {
        walkFunc, cookie, prefix
    };
    ParamTrieNode *root = FindTrieNode(&workSpace->paramSpace, prefix, strlen(prefix), NULL);
    PARAM_LOGV("TraversalParam prefix %s", prefix);
    return TraversalTrieNode(&workSpace->paramSpace, root, ProcessParamTraversal, &context);
}

#ifdef WITH_SELINUX
static void *g_selinuxHandle = NULL;
static int CheckParamPermissionWithSelinux(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    static void (*setSelinuxLogCallback)();
    static int (*setParamCheck)(const char *paraName, struct ucred *uc);
    if (g_selinuxHandle == NULL) {
        g_selinuxHandle = dlopen("/system/lib/libparaperm_checker.z.so", RTLD_LAZY);
        if (g_selinuxHandle == NULL) {
            PARAM_LOGE("Failed to dlopen libparaperm_checker.z.so, %s\n", dlerror());
            return DAC_RESULT_FORBIDED;
        }
    }
    if (setSelinuxLogCallback == NULL) {
        setSelinuxLogCallback = (void (*)())dlsym(g_selinuxHandle, "SetSelinuxLogCallback");
        if (setSelinuxLogCallback == NULL) {
            PARAM_LOGE("Failed to dlsym setSelinuxLogCallback, %s\n", dlerror());
            return DAC_RESULT_FORBIDED;
        }
    }
    (*setSelinuxLogCallback)();

    if (setParamCheck == NULL) {
        setParamCheck = (int (*)(const char *paraName, struct ucred *uc))dlsym(g_selinuxHandle, "SetParamCheck");
        if (setParamCheck == NULL) {
            PARAM_LOGE("Failed to dlsym setParamCheck, %s\n", dlerror());
            return DAC_RESULT_FORBIDED;
        }
    }
    struct ucred uc;
    uc.pid = srcLabel->cred.pid;
    uc.uid = srcLabel->cred.uid;
    uc.gid = srcLabel->cred.gid;
    int ret = setParamCheck(name, &uc);
    if (ret != 0) {
        PARAM_LOGI("Selinux check name %s pid %d uid %d %d result %d", name, uc.pid, uc.uid, uc.gid, ret);
    }
    return ret;
}
#endif

int CheckParamPermission(const ParamWorkSpace *workSpace,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    PARAM_CHECK(workSpace != NULL && workSpace->securityLabel != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid param");
    if (LABEL_IS_ALL_PERMITTED(workSpace->securityLabel)) {
        return 0;
    }
    PARAM_CHECK(name != NULL && srcLabel != NULL, return -1, "Invalid param");
#ifdef WITH_SELINUX
    if (mode == DAC_WRITE) {
        int ret = CheckParamPermissionWithSelinux(srcLabel, name, mode);
        if (ret == DAC_RESULT_PERMISSION) {
            PARAM_LOGI("CheckParamPermission %s", name);
            return DAC_RESULT_PERMISSION;
        }
    }
#endif
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
        ParamSecruityNode *label = (ParamSecruityNode *)GetTrieNode(workSpace, current->labelIndex);
        if (label != NULL) {
            PARAM_DUMP("\tparameter label dac %d %d %o \n\t  label: %s \n",
                label->uid, label->gid, label->mode, (label->length > 0) ? label->data : "null");
        }
    }
    return 0;
}

static void DumpWorkSpace(const ParamWorkSpace *workSpace, int verbose)
{
    PARAM_DUMP("workSpace information \n");
    PARAM_DUMP("    map file: %s \n", workSpace->paramSpace.fileName);
    if (workSpace->paramSpace.area != NULL) {
        PARAM_DUMP("    total size: %d \n", workSpace->paramSpace.area->dataSize);
        PARAM_DUMP("    first offset: %d \n", workSpace->paramSpace.area->firstNode);
        PARAM_DUMP("    current offset: %d \n", workSpace->paramSpace.area->currOffset);
        PARAM_DUMP("    total node: %d \n", workSpace->paramSpace.area->trieNodeCount);
        PARAM_DUMP("    total param node: %d \n", workSpace->paramSpace.area->paramNodeCount);
        PARAM_DUMP("    total security node: %d\n", workSpace->paramSpace.area->securityNodeCount);
    }
    PARAM_DUMP("    node info: \n");
    TraversalTrieNode(&workSpace->paramSpace, NULL, DumpTrieDataNodeTraversal, (void *)&verbose);
}

void DumpParameters(const ParamWorkSpace *workSpace, int verbose)
{
    PARAM_CHECK(workSpace != NULL && workSpace->securityLabel != NULL, return, "Invalid param");
    PARAM_DUMP("Dump all paramters begin ...\n");
    DumpWorkSpace(workSpace, verbose);
    if (verbose) {
        PARAM_DUMP("Local sercurity information\n");
        PARAM_DUMP("\t pid: %d uid: %d gid: %d \n",
            workSpace->securityLabel->cred.pid,
            workSpace->securityLabel->cred.uid,
            workSpace->securityLabel->cred.gid);
    }
    PARAM_DUMP("Dump all paramters finish\n");
}
