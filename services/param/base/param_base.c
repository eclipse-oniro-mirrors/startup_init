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
#include <errno.h>
#include <limits.h>

#include "init_param.h"
#ifndef STARTUP_INIT_TEST
#include "param_include.h"
#endif
#include "param_manager.h"
#include "param_security.h"
#include "param_trie.h"

static ParamWorkSpace g_paramWorkSpace = {0};

STATIC_INLINE int CheckParamPermission_(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
STATIC_INLINE uint32_t ReadCommitId(ParamNode *entry);
STATIC_INLINE int ReadParamValue(ParamNode *entry, char *value, uint32_t *length);
STATIC_INLINE int CheckAndExtendSpace(ParamWorkSpace *workSpace, const char *name, uint32_t labelIndex);
STATIC_INLINE int ReadParamWithCheck(WorkSpace **workspace, const char *name, uint32_t op, ParamTrieNode **node);
STATIC_INLINE ParamTrieNode *BaseFindTrieNode(WorkSpace *workSpace,
    const char *key, uint32_t keyLen, uint32_t *matchLabel);
STATIC_INLINE int ReadParamValue_(ParamNode *entry, uint32_t *commitId, char *value, uint32_t *length);

// return workspace no check valid
STATIC_INLINE WorkSpace *GetWorkSpaceByName(const char *name)
{
#ifdef PARAM_SUPPORT_SELINUX
    if (g_paramWorkSpace.selinuxSpace.getParamLabelIndex == NULL) {
        return NULL;
    }
    uint32_t labelIndex = (uint32_t)g_paramWorkSpace.selinuxSpace.getParamLabelIndex(name) + WORKSPACE_INDEX_BASE;
    if (labelIndex < g_paramWorkSpace.maxLabelIndex) {
        return g_paramWorkSpace.workSpace[labelIndex];
    }
    return NULL;
#else
    return g_paramWorkSpace.workSpace[WORKSPACE_INDEX_DAC];
#endif
}

static int InitParamSecurity(ParamWorkSpace *workSpace,
    RegisterSecurityOpsPtr registerOps, ParamSecurityType type, int isInit, int op)
{
    PARAM_CHECK(workSpace != NULL && type < PARAM_SECURITY_MAX, return -1, "Invalid param");
    int ret = registerOps(&workSpace->paramSecurityOps[type], isInit);
    PARAM_CHECK(workSpace->paramSecurityOps[type].securityInitLabel != NULL,
        return -1, "Invalid securityInitLabel");
    ret = workSpace->paramSecurityOps[type].securityInitLabel(&workSpace->securityLabel, isInit);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to init security");

    ParamSecurityOps *paramSecurityOps = GetParamSecurityOps(type);
    PARAM_CHECK(paramSecurityOps != NULL, return -1, "Invalid paramSecurityOps");
    PARAM_CHECK(paramSecurityOps->securityFreeLabel != NULL, return -1, "Invalid securityFreeLabel");
    PARAM_CHECK(paramSecurityOps->securityCheckFilePermission != NULL, return -1, "Invalid securityCheck");
    if (isInit == LABEL_INIT_FOR_INIT) {
        PARAM_CHECK(paramSecurityOps->securityGetLabel != NULL, return -1, "Invalid securityGetLabel");
    }
    ret = paramSecurityOps->securityCheckFilePermission(&workSpace->securityLabel, PARAM_STORAGE_PATH, op);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "No permission to read file %s", PARAM_STORAGE_PATH);
    PARAM_LOGV("Init parameter %s success", paramSecurityOps->name);
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
        if (g_paramWorkSpace.ops.logFunc == NULL && ops->logFunc != NULL) {
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
#ifdef STARTUP_INIT_TEST
    // for ut, do not init workspace
    char path[PATH_MAX] = { 0 };
    (void)readlink("/proc/self/exe", path, sizeof(path) - 1);
    char *name = strstr(path, "/init_unittest");
    if (name != NULL) {
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
    g_paramWorkSpace.maxLabelIndex = PARAM_DEF_SELINUX_LABEL;
    g_paramWorkSpace.workSpace = (WorkSpace **)calloc(g_paramWorkSpace.maxLabelIndex, sizeof(WorkSpace *));
    PARAM_CHECK(g_paramWorkSpace.workSpace != NULL, return -1, "Failed to alloc memory for workSpace");
    PARAM_SET_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT);

    int ret = RegisterSecurityOps(onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");

    g_paramWorkSpace.checkParamPermission = CheckParamPermission_;
#ifndef PARAM_SUPPORT_SELINUX
    ret = AddWorkSpace(WORKSPACE_NAME_NORMAL, 0, onlyRead, PARAM_WORKSPACE_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed to add dac workspace");
#else
    // for default
    ret = AddWorkSpace(WORKSPACE_NAME_DEF_SELINUX, WORKSPACE_INDEX_BASE, onlyRead, PARAM_WORKSPACE_DEF);
    PARAM_CHECK(ret == 0, return -1, "Failed to add default workspace");
    // add dac workspace
    ret = AddWorkSpace(WORKSPACE_NAME_DAC, WORKSPACE_INDEX_DAC, onlyRead, PARAM_WORKSPACE_SMALL);
    PARAM_CHECK(ret == 0, return -1, "Failed to add dac workspace");
#endif
    if (onlyRead == 0) {
        // load user info for dac
        LoadGroupUser();
        // add default dac policy
        ParamAuditData auditData = {0};
        auditData.name = "#";
        auditData.dacData.gid = DAC_DEFAULT_GROUP;
        auditData.dacData.uid = DAC_DEFAULT_USER;
        auditData.dacData.mode = DAC_DEFAULT_MODE; // 0774 default mode
        auditData.dacData.paramType = PARAM_TYPE_STRING;
#ifdef PARAM_SUPPORT_SELINUX
        auditData.selinuxIndex = INVALID_SELINUX_INDEX;
#endif
        ret = AddSecurityLabel(&auditData);
        PARAM_CHECK(ret == 0, return ret, "Failed to add default dac label");
    } else {
        ret = OpenWorkSpace(WORKSPACE_INDEX_DAC, onlyRead);
        PARAM_CHECK(ret == 0, return -1, "Failed to open dac workspace");
        ret = OpenWorkSpace(WORKSPACE_INDEX_BASE, onlyRead);
        PARAM_CHECK(ret == 0, return -1, "Failed to open default workspace");
#ifdef PARAM_SUPPORT_SELINUX // load security label and create workspace
        ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_SELINUX);
        if (ops != NULL && ops->securityGetLabel != NULL) {
            ops->securityGetLabel(NULL);
        }
#endif
    }
    return ret;
}

INIT_LOCAL_API void CloseParamWorkSpace(void)
{
    PARAM_LOGI("CloseParamWorkSpace %x", g_paramWorkSpace.flags);
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return;
    }
    for (uint32_t i = 0; i < g_paramWorkSpace.maxLabelIndex; i++) {
        if (g_paramWorkSpace.workSpace[i] != NULL) {
            CloseWorkSpace(g_paramWorkSpace.workSpace[i]);
            free(g_paramWorkSpace.workSpace[i]);
        }
        g_paramWorkSpace.workSpace[i] = NULL;
    }
    free(g_paramWorkSpace.workSpace);
    g_paramWorkSpace.workSpace = NULL;
    for (int i = 0; i < PARAM_SECURITY_MAX; i++) {
        if (g_paramWorkSpace.paramSecurityOps[i].securityFreeLabel != NULL) {
            g_paramWorkSpace.paramSecurityOps[i].securityFreeLabel(&g_paramWorkSpace.securityLabel);
        }
    }
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
    return &g_paramWorkSpace;
}

int SystemReadParam(const char *name, char *value, uint32_t *len)
{
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return -1, "Param workspace has not init.");
    PARAM_CHECK(name != NULL && len != NULL, return -1, "The name or value is null");
    ParamTrieNode *node = NULL;
    WorkSpace *workspace = NULL;
    int ret = ReadParamWithCheck(&workspace, name, DAC_READ, &node);
    if (ret != 0) {
        return ret;
    }
    if (node == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    return ReadParamValue((ParamNode *)GetTrieNode(workspace, node->dataIndex), value, len);
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

INIT_LOCAL_API int AddWorkSpace(const char *name, uint32_t labelIndex, int onlyRead, uint32_t spaceSize)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid workspace");
#ifdef PARAM_SUPPORT_SELINUX
    const char *realName = name;
#else
    const char *realName = WORKSPACE_NAME_NORMAL;
    labelIndex = 0;
#endif
    int ret = CheckAndExtendSpace(paramSpace, name, labelIndex);
    PARAM_CHECK(ret == 0, return -1, "Not enough memory for %s", realName);
    if (paramSpace->workSpace[labelIndex] != NULL) {
        return 0;
    }

    const size_t size = strlen(realName) + 1;
    WorkSpace *workSpace = (WorkSpace *)malloc(sizeof(WorkSpace) + size);
    PARAM_CHECK(workSpace != NULL, return -1, "Failed to create workspace for %s", realName);
    workSpace->flags = 0;
    workSpace->spaceSize = spaceSize;
    workSpace->area = NULL;
    workSpace->spaceIndex = labelIndex;
    ATOMIC_INIT(&workSpace->rwSpaceLock, 0);
    PARAMSPACE_AREA_INIT_LOCK(workSpace);
    ret = ParamStrCpy(workSpace->fileName, size, realName);
    PARAM_CHECK(ret == 0, free(workSpace);
        return -1, "Failed to copy file name %s", realName);
    paramSpace->workSpace[labelIndex] = workSpace;
    if (!onlyRead) {
        PARAM_LOGI("AddWorkSpace %s index %d spaceSize: %u onlyRead %s",
            workSpace->fileName, workSpace->spaceIndex, workSpace->spaceSize, onlyRead ? "true" : "false");
        ret = OpenWorkSpace(labelIndex, onlyRead);
        PARAM_CHECK(ret == 0, free(workSpace);
            paramSpace->workSpace[labelIndex] = NULL;
            return -1, "Failed to open workspace for name %s", realName);
    }
    return ret;
}

int SystemFindParameter(const char *name, ParamHandle *handle)
{
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return -1, "Param workspace has not init.");
    PARAM_CHECK(name != NULL && handle != NULL, return -1, "The name or handle is null");
    *handle = -1;
    ParamTrieNode *entry = NULL;
    WorkSpace *workspace = NULL;
    int ret = ReadParamWithCheck(&workspace, name, DAC_READ, &entry);
    PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
    if (entry != NULL && entry->dataIndex != 0) {
        *handle = PARAM_HANDLE(workspace, entry->dataIndex);
        return 0;
    } else if (entry != NULL) {
        return PARAM_CODE_NODE_EXIST;
    }
    return PARAM_CODE_NOT_FOUND;
}

STATIC_INLINE ParamTrieNode *GetTrieNodeByHandle(ParamHandle handle)
{
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return NULL);
    uint32_t labelIndex = 0;
    uint32_t index = 0;
    PARAM_GET_HANDLE_INFO(handle, labelIndex, index);
    WorkSpace *workSpace = NULL;
    if (labelIndex < g_paramWorkSpace.maxLabelIndex) {
        workSpace = g_paramWorkSpace.workSpace[labelIndex];
    }
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return NULL, "Invalid workSpace for handle %x", handle);
    if (PARAM_IS_ALIGNED(index)) {
        return (ParamTrieNode *)GetTrieNode(workSpace, index);
    }
    return NULL;
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
    PARAM_WORKSPACE_CHECK(&g_paramWorkSpace, return 0, "Param workspace has not init.");
    WorkSpace *space = g_paramWorkSpace.workSpace[0];
    if (space == NULL || space->area == NULL) {
        return 0;
    }
    return ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_acquire);
}

int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len)
{
    PARAM_CHECK(len != NULL && handle != 0, return -1, "The value is null");
    return ReadParamValue((ParamNode *)GetTrieNodeByHandle(handle), value, len);
}

STATIC_INLINE int CheckAndExtendSpace(ParamWorkSpace *paramSpace, const char *name, uint32_t labelIndex)
{
    if (paramSpace->maxLabelIndex > labelIndex) {
        return 0;
    }
    if (labelIndex >= PARAM_MAX_SELINUX_LABEL) {
        PARAM_LOGE("Not enough memory for label index %u", labelIndex);
        return -1;
    }
    PARAM_LOGW("Not enough memory for label index %u need to extend memory %u", labelIndex, paramSpace->maxLabelIndex);
    WorkSpace **space = (WorkSpace **)calloc(sizeof(WorkSpace *),
        paramSpace->maxLabelIndex + PARAM_DEF_SELINUX_LABEL);
    PARAM_CHECK(space != NULL, return -1, "Failed to realloc memory for %s", name);
    int ret = ParamMemcpy(space, sizeof(WorkSpace *) * paramSpace->maxLabelIndex,
        paramSpace->workSpace, sizeof(WorkSpace *) * paramSpace->maxLabelIndex);
    PARAM_CHECK(ret == 0, free(space);
        return -1, "Failed to copy memory for %s", name);
    paramSpace->maxLabelIndex += PARAM_DEF_SELINUX_LABEL;
    free(paramSpace->workSpace);
    paramSpace->workSpace = space;
    return 0;
}

INIT_LOCAL_API int OpenWorkSpace(uint32_t index, int readOnly)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid workspace");
    WorkSpace *workSpace = NULL;
    if (index < paramSpace->maxLabelIndex) {
        workSpace = paramSpace->workSpace[index];
    }
    if (workSpace == NULL) {
        return 0;
    }
    int ret = 0;
    uint32_t rwSpaceLock = ATOMIC_LOAD_EXPLICIT(&workSpace->rwSpaceLock, memory_order_acquire);
    if (rwSpaceLock == 1) {
        PARAM_LOGW("Workspace %s in init", workSpace->fileName);
        return -1;
    }
    ATOMIC_STORE_EXPLICIT(&workSpace->rwSpaceLock, 1, memory_order_release);
    if (workSpace->area == NULL) {
        ret = InitWorkSpace(workSpace, readOnly, workSpace->spaceSize);
        if (ret != 0) {
            PARAM_LOGE("Forbid to open workspace for %s error %d", workSpace->fileName, errno);
        }
    }
    ATOMIC_STORE_EXPLICIT(&workSpace->rwSpaceLock, 0, memory_order_release);
    return ret;
}

STATIC_INLINE uint32_t ReadCommitId(ParamNode *entry)
{
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_acquire);
    while (commitId & PARAM_FLAGS_MODIFY) {
        futex_wait(&entry->commitId, commitId);
        commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_acquire);
    }
    return commitId & PARAM_FLAGS_COMMITID;
}

STATIC_INLINE int ReadParamValue_(ParamNode *entry, uint32_t *commitId, char *value, uint32_t *length)
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

STATIC_INLINE int ReadParamValue(ParamNode *entry, char *value, uint32_t *length)
{
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

static int GetParamLabelInfo(const char *name, ParamLabelIndex *labelIndex, ParamTrieNode **node)
{
    // search node from dac space, and get selinux label index
    WorkSpace *dacSpace = g_paramWorkSpace.workSpace[0];
    PARAM_CHECK(dacSpace != NULL && dacSpace->area != NULL,
        return DAC_RESULT_FORBIDED, "Invalid workSpace for %s", name);
    *node = BaseFindTrieNode(dacSpace, name, strlen(name), &labelIndex->dacLabelIndex);
    ParamSecurityNode *securityNode = (ParamSecurityNode *)GetTrieNode(dacSpace, labelIndex->dacLabelIndex);
    if ((securityNode == NULL) || (securityNode->selinuxIndex == 0) ||
        (securityNode->selinuxIndex == INVALID_SELINUX_INDEX)) {
        labelIndex->workspace = GetWorkSpaceByName(name);
    } else if (securityNode->selinuxIndex < g_paramWorkSpace.maxLabelIndex) {
        labelIndex->workspace = g_paramWorkSpace.workSpace[securityNode->selinuxIndex];
    }
    PARAM_CHECK(labelIndex->workspace != NULL, return DAC_RESULT_FORBIDED,
        "Invalid workSpace for %s %d", name, securityNode->selinuxIndex);
    labelIndex->selinuxLabelIndex = labelIndex->workspace->spaceIndex;
    return 0;
}

STATIC_INLINE int ReadParamWithCheck(WorkSpace **workspace, const char *name, uint32_t op, ParamTrieNode **node)
{
    ParamLabelIndex labelIndex = {0};
    int ret = GetParamLabelInfo(name, &labelIndex, node);
    PARAM_CHECK(ret == 0, return ret, "Forbid to read parameter %s", name);
    ret = CheckParamPermission_(&labelIndex, &g_paramWorkSpace.securityLabel, name, op);
    PARAM_CHECK(ret == 0, return ret, "Forbid to read parameter %s", name);
#ifdef PARAM_SUPPORT_SELINUX
    // search from real workspace
    *node = BaseFindTrieNode(labelIndex.workspace, name, strlen(name), NULL);
#endif
    *workspace = labelIndex.workspace;
    return ret;
}

static int CheckUserInGroup(WorkSpace *space, gid_t groupId, uid_t uid)
{
    char buffer[USER_BUFFER_LEN] = {0};
    int ret = ParamSprintf(buffer, sizeof(buffer), GROUP_FORMAT, groupId, uid);
    PARAM_CHECK(ret >= 0, return -1, "Failed to format name for "GROUP_FORMAT, groupId, uid);
    ParamNode *node = GetParamNode(WORKSPACE_INDEX_BASE, buffer);
    if (node != NULL) {
        return 0;
    }
    return -1;
}

STATIC_INLINE int DacCheckParamPermission(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
#ifndef STARTUP_INIT_TEST
    if (srcLabel->cred.uid == 0) {
        return DAC_RESULT_PERMISSION;
    }
#endif

    int ret = DAC_RESULT_FORBIDED;
    // get dac label
    WorkSpace *space = g_paramWorkSpace.workSpace[WORKSPACE_INDEX_DAC];
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex->dacLabelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %u selinuxLabelIndex %u for %s",
        labelIndex->dacLabelIndex, labelIndex->selinuxLabelIndex, name);
    /**
     * DAC group
     * user:group:read|write|watch
     */
    uint32_t localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_OTHER_START;
    // 1, check other
    if ((node->mode & localMode) != 0) {
        ret = DAC_RESULT_PERMISSION;
    } else {
        if (srcLabel->cred.uid == node->uid) { // 2, check uid
            localMode = mode & (DAC_READ | DAC_WRITE | DAC_WATCH);
        } else if (srcLabel->cred.gid == node->gid) { // 3, check gid
            localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
        } else if (CheckUserInGroup(space, node->gid, srcLabel->cred.uid) == 0) {  // 4, check user in group
            localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
        }
        if ((node->mode & localMode) != 0) {
            ret = DAC_RESULT_PERMISSION;
        } else {
            PARAM_LOGW("Param '%s' label gid:%d uid:%d mode 0%o",
                name, srcLabel->cred.gid, srcLabel->cred.uid, localMode);
            PARAM_LOGW("Cfg label %u gid:%d uid:%d mode 0%o ",
                labelIndex->dacLabelIndex, node->gid, node->uid, node->mode);
#ifndef __MUSL__
#ifndef STARTUP_INIT_TEST
        ret = DAC_RESULT_PERMISSION;
#endif
#endif
        }
    }
    return ret;
}

#ifdef PARAM_SUPPORT_SELINUX
STATIC_INLINE int IsWorkSpaceReady(WorkSpace *workSpace)
{
    if (workSpace == NULL) {
        return -1;
    }
    int ret = -1;
    uint32_t rwSpaceLock = ATOMIC_LOAD_EXPLICIT(&workSpace->rwSpaceLock, memory_order_acquire);
    if (rwSpaceLock == 1) {
        return ret;
    }
    if (workSpace->area != NULL) {
        if ((g_paramWorkSpace.flags & WORKSPACE_FLAGS_NEED_ACCESS) == WORKSPACE_FLAGS_NEED_ACCESS) {
            char buffer[FILENAME_LEN_MAX] = {0};
            int size = ParamSprintf(buffer, sizeof(buffer), "%s/%s", PARAM_STORAGE_PATH, workSpace->fileName);
            if (size > 0 && access(buffer, R_OK) == 0) {
                ret = 0;
            }
        } else {
            ret = 0;
        }
    }
    return ret;
}

STATIC_INLINE const char *GetSelinuxContent(const char *name)
{
    SelinuxSpace *selinuxSpace = &g_paramWorkSpace.selinuxSpace;
    const char *content = WORKSPACE_NAME_DEF_SELINUX;
    if (selinuxSpace->getParamLabel != NULL) {
        content = selinuxSpace->getParamLabel(name);
    }
    return content;
}

STATIC_INLINE int SelinuxCheckParamPermission(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    SelinuxSpace *selinuxSpace = &g_paramWorkSpace.selinuxSpace;
    int ret = DAC_RESULT_FORBIDED;
    if (mode == DAC_WRITE) {
        PARAM_CHECK(selinuxSpace->setParamCheck != NULL, return ret, "Invalid setParamCheck");
        // check
        SrcInfo info;
        info.uc.pid = srcLabel->cred.pid;
        info.uc.uid = srcLabel->cred.uid;
        info.uc.gid = srcLabel->cred.gid;
        info.sockFd = srcLabel->sockFd;
        const char *context = GetSelinuxContent(name);
        ret = selinuxSpace->setParamCheck(name, context, &info);
    } else {
#ifdef STARTUP_INIT_TEST
        return selinuxSpace->readParamCheck(name);
#endif
        if (IsWorkSpaceReady(labelIndex->workspace) == 0) {
            return DAC_RESULT_PERMISSION;
        }
        ret = OpenWorkSpace(labelIndex->selinuxLabelIndex, 1);
    }
    if (ret != 0) {
        PARAM_LOGE("Selinux check name %s in %s info [%d %d %d] result %d",
            name, GetSelinuxContent(name), srcLabel->cred.pid, srcLabel->cred.uid, srcLabel->cred.gid, ret);
        ret = DAC_RESULT_FORBIDED;
    }
    return ret;
}
#endif

#if defined(STARTUP_INIT_TEST) || defined(__LITEOS_A__) || defined(__LITEOS_M__)
STATIC_INLINE int CheckParamPermission_(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    // for root, all permission, but for appspawn must to check
    if (srcLabel->cred.uid == 0 && srcLabel->cred.pid == 1) {
        return DAC_RESULT_PERMISSION;
    }
    int ret = DAC_RESULT_PERMISSION;
    for (int i = 0; i < PARAM_SECURITY_MAX; i++) {
        if (PARAM_TEST_FLAG(g_paramWorkSpace.securityLabel.flags[i], LABEL_ALL_PERMISSION)) {
            continue;
        }
        ParamSecurityOps *ops = &g_paramWorkSpace.paramSecurityOps[i];
        if (ops->securityCheckParamPermission == NULL) {
            continue;
        }
        ret = ops->securityCheckParamPermission(labelIndex, srcLabel, name, mode);
        if (ret == DAC_RESULT_FORBIDED) {
            PARAM_LOGW("CheckParamPermission %s %s FORBID", ops->name, name);
            break;
        }
    }
    return ret;
}
#else
STATIC_INLINE int CheckParamPermission_(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    // for root, all permission, but for appspawn must to check
    if (srcLabel->cred.uid == 0 && srcLabel->cred.pid == 1) {
        return DAC_RESULT_PERMISSION;
    }
    int ret = DacCheckParamPermission(labelIndex, srcLabel, name, mode);
#ifdef PARAM_SUPPORT_SELINUX
    if (ret == DAC_RESULT_PERMISSION) {
        ret = SelinuxCheckParamPermission(labelIndex, srcLabel, name, mode);
    }
#endif
    return ret;
}
#endif

STATIC_INLINE ParamTrieNode *BaseFindTrieNode(WorkSpace *workSpace,
    const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    PARAM_CHECK(key != NULL && keyLen > 0, return NULL, "Invalid key ");
    uint32_t tmpMatchLen = 0;
    ParamTrieNode *node = FindTrieNode_(workSpace, key, keyLen, &tmpMatchLen);
    if (matchLabel != NULL) {
        *matchLabel = tmpMatchLen;
    }
    if (node != NULL && node->dataIndex != 0) {
        ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
        if (entry != NULL && entry->keyLength == keyLen) {
            return node;
        }
        return NULL;
    }
    return node;
}

CachedHandle CachedParameterCreate(const char *name, const char *defValue)
{
    PARAM_CHECK(name != NULL && defValue != NULL, return NULL, "Invalid name or default value");
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return NULL, "Invalid param workspace");
    uint32_t nameLen = strlen(name);
    PARAM_CHECK(nameLen < PARAM_NAME_LEN_MAX, return NULL, "Invalid name %s", name);
    uint32_t valueLen = strlen(defValue);
    if (IS_READY_ONLY(name)) {
        PARAM_CHECK(valueLen < PARAM_CONST_VALUE_LEN_MAX, return NULL, "Illegal param value %s", defValue);
    } else {
        PARAM_CHECK(valueLen < PARAM_VALUE_LEN_MAX, return NULL, "Illegal param value %s length", defValue);
    }

    ParamTrieNode *node = NULL;
    WorkSpace *workspace = NULL;
    int ret = ReadParamWithCheck(&workspace, name, DAC_READ, &node);
    PARAM_CHECK(ret == 0, return NULL, "Forbid to access parameter %s", name);

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

STATIC_INLINE const char *CachedParameterCheck(CachedParameter *param, int *changed)
{
    if (param->dataIndex == 0) {
        // no change, do not to find
        long long spaceCommitId = ATOMIC_LOAD_EXPLICIT(&param->workspace->area->commitId, memory_order_acquire);
        if (param->spaceCommitId == spaceCommitId) {
            return param->paramValue;
        }
        param->spaceCommitId = spaceCommitId;
        ParamTrieNode *node = BaseFindTrieNode(param->workspace, param->data, param->nameLen, NULL);
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
    PARAM_LOGV("CachedParameterCheck %u", param->dataCommitId);
    *changed = 1;
    return param->paramValue;
}

const char *CachedParameterGet(CachedHandle handle)
{
    CachedParameter *param = (CachedParameter *)handle;
    PARAM_CHECK(param != NULL, return NULL, "Invalid handle");
    int changed = 0;
    return CachedParameterCheck(param, &changed);
}

const char *CachedParameterGetChanged(CachedHandle handle, int *changed)
{
    CachedParameter *param = (CachedParameter *)handle;
    PARAM_CHECK(param != NULL, return NULL, "Invalid handle");
    PARAM_CHECK(changed != NULL, return NULL, "Invalid changed");
    *changed = 0;
    return CachedParameterCheck(param, changed);
}

void CachedParameterDestroy(CachedHandle handle)
{
    if (handle != NULL) {
        free(handle);
    }
}

#ifdef PARAM_TEST_PERFORMANCE
#define MAX_TEST 10000
STATIC_INLINE long long DiffLocalTime(struct timespec *startTime)
{
    struct timespec endTime;
    clock_gettime(CLOCK_MONOTONIC, &(endTime));
    long long diff = (long long)((endTime.tv_sec - startTime->tv_sec) * 1000000); // 1000000 1000ms
    if (endTime.tv_nsec > startTime->tv_nsec) {
        diff += (endTime.tv_nsec - startTime->tv_nsec) / 1000; // 1000 1ms
    } else {
        diff -= (startTime->tv_nsec - endTime.tv_nsec) / 1000; // 1000 1ms
    }
    return diff;
}

static void TestPermissionCheck(const char *testParamName)
{
    struct timespec startTime;
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    ParamSecurityLabel *label = &(GetParamWorkSpace()->securityLabel);
    ParamLabelIndex labelIndex = {0};
    ParamTrieNode *node = NULL;
    GetParamLabelInfo(testParamName, &labelIndex, &node);

    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    for (int i = 0; i < MAX_TEST; ++i) {
        CheckParamPermission_(&labelIndex, label, testParamName, DAC_READ);
    }
    printf("CheckParamPermission total time %lld us \n", DiffLocalTime(&startTime));

    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    for (int i = 0; i < MAX_TEST; ++i) {
        DacCheckParamPermission(&labelIndex, label, testParamName, DAC_READ);
    }
    printf("DacCheckParamPermission DAC  total time %lld us \n", DiffLocalTime(&startTime));
#ifdef PARAM_SUPPORT_SELINUX
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    for (int i = 0; i < MAX_TEST; ++i) {
        SelinuxCheckParamPermission(&labelIndex, label, testParamName, DAC_READ);
    }
    printf("CheckParamPermission selinux total time %lld us \n", DiffLocalTime(&startTime));
#endif

    ParamHandle handle = -1;
    uint32_t index = 0;
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    for (int i = 0; i < MAX_TEST; ++i) {
        ParamTrieNode *node = BaseFindTrieNode(labelIndex.workspace, testParamName, strlen(testParamName), &index);
        if (node != NULL && node->dataIndex != 0) {
            handle = PARAM_HANDLE(labelIndex.workspace, node->dataIndex);
        }
    }
    printf("BaseFindTrieNode total time %lld us handle %x \n", DiffLocalTime(&startTime), handle);

    CachedHandle cacheHandle2 = CachedParameterCreate(testParamName, "true");
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    const char *value = NULL;
    for (int i = 0; i < MAX_TEST; ++i) {
        value = CachedParameterGet(cacheHandle2);
    }
    CachedParameterDestroy(cacheHandle2);
    printf("CachedParameterGet time %lld us value %s \n", DiffLocalTime(&startTime), value);
    return;
}

void TestParameterReaderPerformance(void)
{
    struct timespec startTime;
    const char *testParamName = "persist.appspawn.randrom.read";
    const uint32_t buffSize = PARAM_VALUE_LEN_MAX;
    char buffer[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t size = buffSize;
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    printf("TestReader total time %lld us %s \n", DiffLocalTime(&startTime), testParamName);
    for (int j = 0; j < 5; ++j) { // retry 5
        clock_gettime(CLOCK_MONOTONIC, &(startTime));
        for (int i = 0; i < MAX_TEST; ++i) {
            size = buffSize;
            SystemReadParam(testParamName, buffer, &size);
        }
        printf("SystemReadParam total time %lld us \n", DiffLocalTime(&startTime));
        printf("SystemReadParam result %s \n", buffer);

        WorkSpace *workspace = NULL;
        clock_gettime(CLOCK_MONOTONIC, &(startTime));
        for (int i = 0; i < MAX_TEST; ++i) {
            ParamTrieNode *entry = NULL;
            ReadParamWithCheck(&workspace, testParamName, DAC_READ, &entry);
        }
        printf("ReadParamWithCheck total time %lld us \n", DiffLocalTime(&startTime));
    }

    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    for (int i = 0; i < MAX_TEST; ++i) {
        GetWorkSpaceByName(testParamName);
    }
    printf("GetWorkSpaceByName total time %lld us \n", DiffLocalTime(&startTime));

    TestPermissionCheck(testParamName);
    return;
}
#endif