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
    PARAM_LOGI("InitParamSecurity %s success", paramSecurityOps->name);
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
        g_paramWorkSpace.ops.logFunc = ops->logFunc;
#ifdef PARAM_SUPPORT_SELINUX
        g_paramWorkSpace.ops.setfilecon = ops->setfilecon;
#endif
    }
    PARAM_LOGI("InitParamWorkSpace %x", g_paramWorkSpace.flags);
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

INIT_LOCAL_API void CloseParamWorkSpace(void)
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
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL && len != NULL && strlen(name) > 0, return -1, "The name or value is null");
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to get parameter %s", name);
    }
    return ReadParamValue(handle, value, len);
}
