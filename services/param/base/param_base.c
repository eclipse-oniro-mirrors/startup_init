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

static ParamWorkSpace g_paramWorkSpace = {};
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

INIT_INNER_API ParamHandle GetParamHandle(const WorkSpace *workSpace, uint32_t index, const char *name)
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

INIT_PUBLIC_API int InitParamWorkSpace(int onlyRead)
{
    PARAM_LOGI("InitParamWorkSpace %p", &g_paramWorkSpace);
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
        ParamAuditData auditData = {};
        auditData.name = "#";
        auditData.dacData.gid = DAC_DEFAULT_GROUP; // 2000 for shell
        auditData.dacData.uid = DAC_DEFAULT_USER; // for root
        auditData.dacData.mode = DAC_DEFAULT_MODE; // 0774 default mode
        auditData.dacData.paramType = PARAM_TYPE_STRING;
        ret = AddSecurityLabel(&auditData);
        PARAM_CHECK(ret == 0, return ret, "Failed to add default dac label");
    } else {
#ifdef PARAM_SUPPORT_SELINUX
        OpenPermissionWorkSpace();
#endif
    }
    return ret;
}

INIT_INNER_API void CloseParamWorkSpace(void)
{
    PARAM_LOGI("CloseParamWorkSpace %p", &g_paramWorkSpace);
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

INIT_LOCAL_API int AddWorkSpace(const char *name, int onlyRead, uint32_t spaceSize)
{
    int ret = 0;
    // check exist
#ifdef PARAM_SUPPORT_SELINUX
    const char *realName = name;
#else
    const char *realName = WORKSPACE_NAME_NORMAL;
#endif
    WORKSPACE_RW_LOCK(g_paramWorkSpace);
    HashNode *node = OH_HashMapGet(g_paramWorkSpace.workSpaceHashHandle, (const void *)realName);
    if (node != NULL) {
        WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
        return 0;
    }
    PARAM_LOGV("AddWorkSpace %s spaceSize: %u onlyRead %s", name, spaceSize, onlyRead ? "true" : "false");
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
        ret = OH_HashMapAdd(g_paramWorkSpace.workSpaceHashHandle, &workSpace->hashNode);
        PARAM_CHECK(ret == 0, CloseWorkSpace(workSpace);
            workSpace = NULL;
            break, "Failed to add hash node");
        OH_ListAddTail(&g_paramWorkSpace.workSpaceList, &workSpace->node);
        ret = 0;
        workSpace = NULL;
    } while (0);
    if (workSpace != NULL) {
        free(workSpace);
    }
    WORKSPACE_RW_UNLOCK(g_paramWorkSpace);
    PARAM_LOGV("AddWorkSpace %s %s", name, ret == 0 ? "success" : "fail");
    return ret;
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

static int ReadParamValue(ParamHandle handle, char *value, uint32_t *length)
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
        int ret = ParamMemcpy(value, *length, entry->data + entry->keyLength + 1, entry->valueLength);
        PARAM_CHECK(ret == 0, return -1, "Failed to copy value");
        value[entry->valueLength] = '\0';
        *length = entry->valueLength;
    } while (commitId != ReadCommitId(entry));
    return 0;
}

static int ReadParamName(ParamHandle handle, char *name, uint32_t length)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return -1;
    }
    PARAM_CHECK(length > entry->keyLength, return -1, "Invalid param size %u %u", entry->keyLength, length);
    int ret = ParamMemcpy(name, length, entry->data, entry->keyLength);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_PARAM, "Failed to copy name");
    name[entry->keyLength] = '\0';
    return 0;
}

INIT_INNER_API int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
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
            ParamSecurityOps *ops = GetParamSecurityOps(i);
            if (ops == NULL) {
                continue;
            }
            if (ops->securityCheckParamPermission == NULL) {
                continue;
            }
            ret = ops->securityCheckParamPermission(srcLabel, name, mode);
            if (ret == DAC_RESULT_FORBIDED) {
                PARAM_LOGW("CheckParamPermission %s %s FORBID", ops->name, name);
                break;
            }
        }
    }
    return ret;
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

int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(handle != 0 && commitId != NULL, return -1, "The handle is null");

    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return -1;
    }
    *commitId = ReadCommitId(entry);
    return 0;
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

INIT_INNER_API ParamWorkSpace *GetParamWorkSpace(void)
{
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        PARAM_LOGE("GetParamWorkSpace %p", &g_paramWorkSpace);
        return NULL;
    }
    return &g_paramWorkSpace;
}

INIT_LOCAL_API int ParamSprintf(char *buffer, size_t buffSize, const char *format, ...)
{
    int len = -1;
    va_list vargs;
    va_start(vargs, format);
#ifdef PARAM_BASE
    len = vsnprintf(buffer, buffSize - 1, format, vargs);
#else
    len = vsnprintf_s(buffer, buffSize, buffSize - 1, format, vargs);
#endif
    va_end(vargs);
    return len;
}

INIT_LOCAL_API int ParamMemcpy(void *dest, size_t destMax, const void *src, size_t count)
{
    int ret = 0;
#ifdef PARAM_BASE
    memcpy(dest, src, count);
#else
    ret = memcpy_s(dest, destMax, src, count);
#endif
    return ret;
}

INIT_LOCAL_API int ParamStrCpy(char *strDest, size_t destMax, const char *strSrc)
{
    int ret = 0;
#ifdef PARAM_BASE
    if (strlen(strSrc) >= destMax) {
        return -1;
    }
    size_t i = 0;
    while ((i < destMax) && *strSrc != '\0') {
        *strDest = *strSrc;
        strDest++;
        strSrc++;
        i++;
    }
    *strDest = '\0';
#else
    ret = strcpy_s(strDest, destMax, strSrc);
#endif
    return ret;
}