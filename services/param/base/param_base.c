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
#include "param_base.h"

#include <ctype.h>
#include <limits.h>

#include "param_manager.h"
#include "param_trie.h"

static int ReadParamValue(ParamHandle handle, char *value, uint32_t *length);

static ParamWorkSpace g_paramWorkSpace = {0};
PARAM_STATIC int WorkSpaceNodeCompare(const HashNode *node1, const HashNode *node2)
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

    ParamSecurityOps *paramSecurityOps = GetParamSecurityOps(type);
    PARAM_CHECK(paramSecurityOps != NULL, return -1, "Invalid paramSecurityOps");
    PARAM_CHECK(paramSecurityOps->securityFreeLabel != NULL, return -1, "Invalid securityFreeLabel");
    PARAM_CHECK(paramSecurityOps->securityCheckFilePermission != NULL, return -1, "Invalid securityCheck");
    PARAM_CHECK(paramSecurityOps->securityCheckParamPermission != NULL, return -1, "Invalid securityCheck");
    if (isInit == LABEL_INIT_FOR_INIT) {
        PARAM_CHECK(paramSecurityOps->securityGetLabel != NULL, return -1, "Invalid securityGetLabel");
    }
    ret = paramSecurityOps->securityCheckFilePermission(&workSpace->securityLabel, PARAM_STORAGE_PATH, op);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "No permission to read file %s", PARAM_STORAGE_PATH);
    PARAM_LOGI("Init parameter %s success", paramSecurityOps->name);
    return 0;
}

INIT_LOCAL_API int RegisterSecurityOps(int onlyRead)
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

static int CheckNeedInit(int onlyRead, const PARAM_WORKSPACE_OPS *ops)
{
    if (ops != NULL) {
        g_paramWorkSpace.ops.updaterMode = ops->updaterMode;
        if (g_paramWorkSpace.ops.logFunc == NULL) {
            g_paramWorkSpace.ops.logFunc = ops->logFunc;
        }
#ifdef PARAM_SUPPORT_SELINUX
        g_paramWorkSpace.ops.setfilecon = ops->setfilecon;
#endif
    }
    if (PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    if (onlyRead == 0) {
        return 1;
    }
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
    if (getpid() == 1) { // init process only for write
        return 0;
    }
    // for ut, do not init workspace
    char path[PATH_MAX] = { 0 };
    (void)readlink("/proc/self/exe", path, sizeof(path) - 1);
    char *name = strrchr(path, '/');
    if (name != NULL) {
        name++;
    } else {
        name = path;
    }
    if (strcmp(name, "init_unittest") == 0) {
        PARAM_LOGW("Can not init client for init_test");
        return 0;
    }
#endif
    return 1;
}

INIT_INNER_API int InitParamWorkSpace(int onlyRead, const PARAM_WORKSPACE_OPS *ops)
{
    if (CheckNeedInit(onlyRead, ops) == 0) {
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
    int ret = OH_HashMapCreate(&g_paramWorkSpace.workSpaceHashHandle, &info);
    PARAM_CHECK(ret == 0, return -1, "Failed to create hash map for workspace");
    WORKSPACE_INIT_LOCK(g_paramWorkSpace);
    OH_ListInit(&g_paramWorkSpace.workSpaceList);
    PARAM_SET_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT);

    ret = RegisterSecurityOps(onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");

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
        ParamAuditData auditData = {0};
        auditData.name = "#";
        auditData.dacData.gid = DAC_DEFAULT_GROUP; // 2000 for shell
        auditData.dacData.uid = DAC_DEFAULT_USER; // for root
        auditData.dacData.mode = DAC_DEFAULT_MODE; // 0774 default mode
        auditData.dacData.paramType = PARAM_TYPE_STRING;
        ret = AddSecurityLabel(&auditData);
        PARAM_CHECK(ret == 0, return ret, "Failed to add default dac label");
    }
    return ret;
}

INIT_LOCAL_API void CloseParamWorkSpace(void)
{
    PARAM_LOGI("CloseParamWorkSpace");
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return;
    }
    WORKSPACE_RW_LOCK(g_paramWorkSpace);
    if (g_paramWorkSpace.workSpaceHashHandle != NULL) {
        OH_HashMapDestory(g_paramWorkSpace.workSpaceHashHandle);
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

INIT_LOCAL_API void ParamWorBaseLog(InitLogLevel logLevel, uint32_t domain, const char *tag, const char *fmt, ...)
{
    if (g_paramWorkSpace.ops.logFunc != NULL) {
        va_list vargs;
        va_start(vargs, fmt);
        g_paramWorkSpace.ops.logFunc(logLevel, domain, tag, fmt, vargs);
        va_end(vargs);
    }
}

INIT_INNER_API ParamWorkSpace *GetParamWorkSpace(void)
{
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        PARAM_LOGE("GetParamWorkSpace %p", &g_paramWorkSpace);
        return NULL;
    }
    return &g_paramWorkSpace;
}

int SystemReadParam(const char *name, char *value, uint32_t *len)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Param workspace has not init.");
    PARAM_CHECK(name != NULL && len != NULL && strlen(name) > 0, return -1, "The name or value is null");
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to get parameter %s", name);
    }
    return ReadParamValue(handle, value, len);
}

void InitParameterClient(void)
{
    if (getpid() == 1) {
        return;
    }
    PARAM_WORKSPACE_OPS ops = {0};
    ops.updaterMode = 0;
    InitParamWorkSpace(1, &ops);
}

INIT_LOCAL_API int AddWorkSpace(const char *name, int onlyRead, uint32_t spaceSize)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid workspace");
    int ret = 0;
    // check exist
#ifdef PARAM_SUPPORT_SELINUX
    const char *realName = name;
#else
    const char *realName = WORKSPACE_NAME_NORMAL;
#endif
    WORKSPACE_RW_LOCK(*paramSpace);
    HashNode *node = OH_HashMapGet(paramSpace->workSpaceHashHandle, (const void *)realName);
    if (node != NULL) {
        WORKSPACE_RW_UNLOCK(*paramSpace);
        return 0;
    }
    if (onlyRead == 0) {
        PARAM_LOGI("AddWorkSpace %s spaceSize: %u onlyRead %s", name, spaceSize, onlyRead ? "true" : "false");
    }
    WorkSpace *workSpace = NULL;
    do {
        ret = -1;
        const size_t size = strlen(realName) + 1;
        workSpace = (WorkSpace *)malloc(sizeof(WorkSpace) + size);
        PARAM_CHECK(workSpace != NULL, break, "Failed to create workspace for %s", realName);
        workSpace->flags = 0;
        workSpace->area = NULL;
        OH_ListInit(&workSpace->node);
        ret = ParamStrCpy(workSpace->fileName, size, realName);
        PARAM_CHECK(ret == 0, break, "Failed to copy file name %s", realName);
        HASHMAPInitNode(&workSpace->hashNode);
        ret = InitWorkSpace(workSpace, onlyRead, spaceSize);
        PARAM_CHECK(ret == 0, break, "Failed to init workspace %s", realName);
        ret = OH_HashMapAdd(paramSpace->workSpaceHashHandle, &workSpace->hashNode);
        PARAM_CHECK(ret == 0, CloseWorkSpace(workSpace);
            workSpace = NULL;
            break, "Failed to add hash node");
        OH_ListAddTail(&paramSpace->workSpaceList, &workSpace->node);
        ret = 0;
        workSpace = NULL;
    } while (0);
    if (workSpace != NULL) {
        free(workSpace);
    }
    WORKSPACE_RW_UNLOCK(*paramSpace);
    PARAM_LOGV("AddWorkSpace %s %s", name, ret == 0 ? "success" : "fail");
    return ret;
}

int SystemFindParameter(const char *name, ParamHandle *handle)
{
    PARAM_CHECK(name != NULL && handle != NULL, return -1, "The name or handle is null");
    int ret = ReadParamWithCheck(name, DAC_READ, handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
    }
    return ret;
}

int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId)
{
    PARAM_CHECK(handle != 0 && commitId != NULL, return -1, "The handle is null");

    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    *commitId = ReadCommitId(entry);
    return 0;
}

long long GetSystemCommitId(void)
{
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    if (space == NULL || space->area == NULL) {
        return 0;
    }
    return ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_acquire);
}

int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len)
{
    PARAM_CHECK(len != NULL && handle != 0, return -1, "The value is null");
    return ReadParamValue(handle, value, len);
}

static int ReadParamValue_(ParamNode *entry, uint32_t *commitId, char *value, uint32_t *length)
{
    uint32_t id = *commitId;
    do {
        *commitId = id;
        int ret = ParamMemcpy(value, *length, entry->data + entry->keyLength + 1, entry->valueLength);
        PARAM_CHECK(ret == 0, return -1, "Failed to copy value");
        value[entry->valueLength] = '\0';
        *length = entry->valueLength;
        id = ReadCommitId(entry);
    } while (*commitId != id); // if change,must read
    return 0;
}

static int ReadParamValue(ParamHandle handle, char *value, uint32_t *length)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid workspace");
    PARAM_CHECK(length != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    if (value == NULL) {
        *length = entry->valueLength + 1;
        return 0;
    }
    PARAM_CHECK(*length > entry->valueLength, return PARAM_CODE_INVALID_PARAM,
        "Invalid value len %u %u", *length, entry->valueLength);
    uint32_t commitId = ReadCommitId(entry);
    return ReadParamValue_(entry, &commitId, value, length);
}

CachedHandle CachedParameterCreate(const char *name, const char *defValue)
{
    PARAM_CHECK(name != NULL && defValue != NULL, return NULL, "Invalid name or default value");
    PARAM_CHECK(GetParamWorkSpace() != NULL, return NULL, "Invalid workspace");
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return NULL, "Invalid param workspace");
    uint32_t nameLen = strlen(name);
    PARAM_CHECK(nameLen < PARAM_NAME_LEN_MAX, return NULL, "Invalid name %s", name);
    uint32_t valueLen = strlen(defValue);
    if (IS_READY_ONLY(name)) {
        PARAM_CHECK(valueLen < PARAM_CONST_VALUE_LEN_MAX, return NULL, "Illegal param value %s", defValue);
    } else {
        PARAM_CHECK(valueLen < PARAM_VALUE_LEN_MAX, return NULL, "Illegal param value %s length", defValue);
    }

    int ret = CheckParamPermission(GetParamSecurityLabel(), name, DAC_READ);
    PARAM_CHECK(ret == 0, return NULL, "Forbid to access parameter %s", name);
    WorkSpace *workspace = GetWorkSpace(name);
    PARAM_CHECK(workspace != NULL, return NULL, "Invalid workSpace");
    ParamTrieNode *node = FindTrieNode(workspace, name, strlen(name), NULL);

    CachedParameter *param = (CachedParameter *)malloc(
        sizeof(CachedParameter) + PARAM_ALIGN(nameLen) + 1 + PARAM_VALUE_LEN_MAX);
    PARAM_CHECK(param != NULL, return NULL, "Failed to create CachedParameter for %s", name);
    ret = ParamStrCpy(param->data, nameLen + 1, name);
    PARAM_CHECK(ret == 0, free(param);
        return NULL, "Failed to copy name %s", name);
    param->workspace = workspace;
    param->nameLen = nameLen;
    param->paramValue = &param->data[PARAM_ALIGN(nameLen) + 1];
    param->bufferLen = PARAM_VALUE_LEN_MAX;
    param->dataCommitId = (uint32_t)-1;
    if (node != NULL && node->dataIndex != 0) {
        param->dataIndex = node->dataIndex;
        ParamNode *entry = (ParamNode *)GetTrieNode(workspace, node->dataIndex);
        PARAM_CHECK(entry != NULL, free(param);
            return NULL, "Failed to get trie node %s", name);
        uint32_t length = param->bufferLen;
        param->dataCommitId = ReadCommitId(entry);
        ret = ReadParamValue_(entry, &param->dataCommitId, param->paramValue, &length);
        PARAM_CHECK(ret == 0, free(param);
            return NULL, "Failed to read parameter value %s", name);
    } else {
        param->dataIndex = 0;
        ret = ParamStrCpy(param->paramValue, param->bufferLen, defValue);
        PARAM_CHECK(ret == 0, free(param);
            return NULL, "Failed to copy name %s", name);
    }
    param->spaceCommitId = ATOMIC_LOAD_EXPLICIT(&workspace->area->commitId, memory_order_acquire);
    PARAM_LOGV("CachedParameterCreate %u %u %lld \n", param->dataIndex, param->dataCommitId, param->spaceCommitId);
    return (CachedHandle)param;
}

static const char *CachedParameterCheck(CachedParameter *param)
{
    if (param->dataIndex == 0) {
        // no change, do not to find
        long long spaceCommitId = ATOMIC_LOAD_EXPLICIT(&param->workspace->area->commitId, memory_order_acquire);
        if (param->spaceCommitId == spaceCommitId) {
            return param->paramValue;
        }
        param->spaceCommitId = spaceCommitId;
        ParamTrieNode *node = FindTrieNode(param->workspace, param->data, param->nameLen, NULL);
        if (node != NULL) {
            param->dataIndex = node->dataIndex;
        } else {
            return param->paramValue;
        }
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(param->workspace, param->dataIndex);
    PARAM_CHECK(entry != NULL, return param->paramValue, "Failed to get trie node %s", param->data);
    uint32_t dataCommitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_acquire);
    dataCommitId &= PARAM_FLAGS_COMMITID;
    if (param->dataCommitId == dataCommitId) {
        return param->paramValue;
    }
    uint32_t length = param->bufferLen;
    param->dataCommitId = dataCommitId;
    int ret = ReadParamValue_(entry, &param->dataCommitId, param->paramValue, &length);
    PARAM_CHECK(ret == 0, return NULL, "Failed to copy value %s", param->data);
    PARAM_LOGI("CachedParameterCheck %u", param->dataCommitId);
    return param->paramValue;
}

const char *CachedParameterGet(CachedHandle handle)
{
    CachedParameter *param = (CachedParameter *)handle;
    PARAM_CHECK(param != NULL, return NULL, "Invalid handle");
    return CachedParameterCheck(param);
}

void CachedParameterDestroy(CachedHandle handle)
{
    if (handle != NULL) {
        free(handle);
    }
}