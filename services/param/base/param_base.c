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

#define PUBLIC_APP_BEGIN_UID 10000

static ParamWorkSpace g_paramWorkSpace = {0};

static int CheckParamPermission_(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
STATIC_INLINE int CheckAndExtendSpace(ParamWorkSpace *workSpace, const char *name, uint32_t labelIndex);
STATIC_INLINE ParamTrieNode *BaseFindTrieNode(WorkSpace *workSpace,
    const char *key, uint32_t keyLen, uint32_t *matchLabel);
STATIC_INLINE const char *CachedParameterCheck(CachedParameter *param, int *changed);

static int InitParamSecurity(ParamWorkSpace *workSpace,
    RegisterSecurityOpsPtr registerOps, ParamSecurityType type, int isInit, int op)
{
    PARAM_CHECK(workSpace != NULL && type < PARAM_SECURITY_MAX, return -1, "Invalid param");
    registerOps(&workSpace->paramSecurityOps[type], isInit);
    PARAM_CHECK(workSpace->paramSecurityOps[type].securityInitLabel != NULL,
        return -1, "Invalid securityInitLabel");
    int ret = workSpace->paramSecurityOps[type].securityInitLabel(&workSpace->securityLabel, isInit);
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
        if (ops->getServiceGroupIdByPid != NULL) {
            g_paramWorkSpace.ops.getServiceGroupIdByPid = ops->getServiceGroupIdByPid;
        }
        if (ops->logFunc != NULL) {
            if (onlyRead == 0) {
                g_paramWorkSpace.ops.logFunc = ops->logFunc;
            } else if (g_paramWorkSpace.ops.logFunc == NULL) {
                g_paramWorkSpace.ops.logFunc = ops->logFunc;
            }
        }
#ifdef PARAM_SUPPORT_SELINUX
        g_paramWorkSpace.ops.setfilecon = ops->setfilecon;
#endif
    }
    if (PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        PARAM_LOGV("Param workspace has been init");
        if (PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_FOR_INIT)) {
            return 0; // init has been init workspace, do not init
        }
        if (onlyRead == 0) { // init not init workspace, do init it
            CloseParamWorkSpace();
            return 1;
        }
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

static int AllocSpaceMemory(uint32_t maxLabel)
{
    WorkSpace *workSpace = GetWorkSpace(WORKSPACE_INDEX_SIZE);
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_ERROR, "Invalid dac workspace");
    if (workSpace->area->spaceSizeOffset != 0) {
        return 0;
    }
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid workspace");
    uint32_t realLen = PARAM_ALIGN(sizeof(WorkSpaceSize) + sizeof(uint32_t) * maxLabel);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);
    WorkSpaceSize *node = (WorkSpaceSize *)(workSpace->area->data + workSpace->area->currOffset);
    node->maxLabelIndex = maxLabel;
    node->spaceSize[WORKSPACE_INDEX_DAC] = PARAM_WORKSPACE_DAC;
    node->spaceSize[WORKSPACE_INDEX_BASE] = PARAM_WORKSPACE_MAX;
    for (uint32_t i = WORKSPACE_INDEX_BASE + 1; i < maxLabel; i++) {
        node->spaceSize[i] = PARAM_WORKSPACE_MIN;
        PARAM_LOGV("AllocSpaceMemory spaceSize index %u %u", i, node->spaceSize[i]);
        if (paramSpace->workSpace[i] != NULL) {
            paramSpace->workSpace[i]->spaceSize = PARAM_WORKSPACE_MIN;
        }
    }
    workSpace->area->spaceSizeOffset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    return 0;
}

static int CreateWorkSpace(int onlyRead)
{
    int ret = 0;
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid workspace");
#ifdef PARAM_SUPPORT_SELINUX
    ret = AddWorkSpace(WORKSPACE_NAME_DAC, WORKSPACE_INDEX_DAC, 0, PARAM_WORKSPACE_DAC);
    PARAM_CHECK(ret == 0, return -1, "Failed to add dac workspace");
    ret = AddWorkSpace(WORKSPACE_NAME_DEF_SELINUX, WORKSPACE_INDEX_BASE, onlyRead, PARAM_WORKSPACE_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed to add default workspace");

    // open dac workspace
    ret = OpenWorkSpace(WORKSPACE_INDEX_DAC, onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to open dac workspace");

    // for other workspace
    ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_SELINUX);
    if (ops != NULL && ops->securityGetLabel != NULL) {
        ret = ops->securityGetLabel("create");
    }
    paramSpace->maxLabelIndex++;
#else
    ret = AddWorkSpace(WORKSPACE_NAME_NORMAL, WORKSPACE_INDEX_DAC, onlyRead, PARAM_WORKSPACE_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed to add dac workspace");
    ret = OpenWorkSpace(WORKSPACE_INDEX_DAC, onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to open dac workspace");
    paramSpace->maxLabelIndex = 1;
#endif
    return ret;
}

INIT_INNER_API int InitParamWorkSpace(int onlyRead, const PARAM_WORKSPACE_OPS *ops)
{
    PARAM_ONLY_CHECK(CheckNeedInit(onlyRead, ops) != 0, return 0);

    paramMutexEnvInit();
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        g_paramWorkSpace.maxSpaceCount = PARAM_DEF_SELINUX_LABEL;
        g_paramWorkSpace.workSpace = (WorkSpace **)calloc(g_paramWorkSpace.maxSpaceCount, sizeof(WorkSpace *));
        PARAM_CHECK(g_paramWorkSpace.workSpace != NULL, return -1, "Failed to alloc memory for workSpace");
    }
    PARAM_SET_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT);

    int ret = RegisterSecurityOps(onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to get security operations");
    g_paramWorkSpace.checkParamPermission = CheckParamPermission_;
    ret = CreateWorkSpace(onlyRead);
    PARAM_CHECK(ret == 0, return -1, "Failed to create workspace");

    if (onlyRead == 0) {
        PARAM_LOGI("Max selinux label %u %u", g_paramWorkSpace.maxSpaceCount, g_paramWorkSpace.maxLabelIndex);
        // alloc space size memory from dac
        ret = AllocSpaceMemory(g_paramWorkSpace.maxLabelIndex);
        PARAM_CHECK(ret == 0, return -1, "Failed to alloc space size");

        // add default dac policy
        ParamAuditData auditData = {0};
        auditData.name = "#";
        auditData.dacData.gid = DAC_DEFAULT_GROUP;
        auditData.dacData.uid = DAC_DEFAULT_USER;
        auditData.dacData.mode = DAC_DEFAULT_MODE; // 0774 default mode
        auditData.dacData.paramType = PARAM_TYPE_STRING;
        auditData.memberNum = 1;
        auditData.members[0] = DAC_DEFAULT_GROUP;
#ifdef PARAM_SUPPORT_SELINUX
        auditData.selinuxIndex = INVALID_SELINUX_INDEX;
#endif
        ret = AddSecurityLabel(&auditData);
        PARAM_CHECK(ret == 0, return ret, "Failed to add default dac label");
        PARAM_SET_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_FOR_INIT);
    }
    return ret;
}

INIT_LOCAL_API void CloseParamWorkSpace(void)
{
    PARAM_LOGI("CloseParamWorkSpace %x", g_paramWorkSpace.flags);
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return;
    }
    for (uint32_t i = 0; i < g_paramWorkSpace.maxSpaceCount; i++) {
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

void InitParameterClient(void)
{
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
    ret = PARAM_STRCPY(workSpace->fileName, size, realName);
    PARAM_CHECK(ret == 0, free(workSpace);
        return -1, "Failed to copy file name %s", realName);
    paramSpace->workSpace[labelIndex] = workSpace;
    PARAM_LOGV("AddWorkSpace %s index %d onlyRead %s", paramSpace->workSpace[labelIndex]->fileName,
        paramSpace->workSpace[labelIndex]->spaceIndex, onlyRead ? "true" : "false");

    if (spaceSize != 0) {
        return ret;
    }
    // get size
    WorkSpaceSize *workSpaceSize = GetWorkSpaceSize(GetWorkSpace(WORKSPACE_INDEX_SIZE));
    if (workSpaceSize != NULL) {
        paramSpace->workSpace[labelIndex]->spaceSize = workSpaceSize->spaceSize[labelIndex];
    }
    return ret;
}

STATIC_INLINE int CheckAndExtendSpace(ParamWorkSpace *paramSpace, const char *name, uint32_t labelIndex)
{
    if (paramSpace->maxSpaceCount > labelIndex) {
        return 0;
    }
    if (labelIndex >= PARAM_MAX_SELINUX_LABEL) {
        PARAM_LOGE("Not enough memory for label index %u", labelIndex);
        return -1;
    }
    PARAM_LOGW("Not enough memory for label index %u need to extend memory %u", labelIndex, paramSpace->maxSpaceCount);
    WorkSpace **space = (WorkSpace **)calloc(sizeof(WorkSpace *),
        paramSpace->maxSpaceCount + PARAM_DEF_SELINUX_LABEL);
    PARAM_CHECK(space != NULL, return -1, "Failed to realloc memory for %s", name);
    int ret = PARAM_MEMCPY(space, sizeof(WorkSpace *) * paramSpace->maxSpaceCount,
        paramSpace->workSpace, sizeof(WorkSpace *) * paramSpace->maxSpaceCount);
    PARAM_CHECK(ret == 0, free(space);
        return -1, "Failed to copy memory for %s", name);
    paramSpace->maxSpaceCount += PARAM_DEF_SELINUX_LABEL;
    free(paramSpace->workSpace);
    paramSpace->workSpace = space;
    return 0;
}

INIT_LOCAL_API int OpenWorkSpace(uint32_t index, int readOnly)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL && paramSpace->workSpace != NULL, return -1, "Invalid workspace index %u", index);
    WorkSpace *workSpace = NULL;
    PARAM_ONLY_CHECK(index >= paramSpace->maxSpaceCount, workSpace = paramSpace->workSpace[index]);
    PARAM_CHECK(workSpace != NULL, return 0, "Invalid index %d", index);
    int ret = 0;
    uint32_t rwSpaceLock = ATOMIC_LOAD_EXPLICIT(&workSpace->rwSpaceLock, MEMORY_ORDER_ACQUIRE);
    while (rwSpaceLock & WORKSPACE_STATUS_IN_PROCESS) {
        futex_wait_private(&workSpace->rwSpaceLock, rwSpaceLock);
        rwSpaceLock = ATOMIC_LOAD_EXPLICIT(&workSpace->rwSpaceLock, MEMORY_ORDER_ACQUIRE);
    }

    ATOMIC_STORE_EXPLICIT(&workSpace->rwSpaceLock, rwSpaceLock | WORKSPACE_STATUS_IN_PROCESS, MEMORY_ORDER_RELEASE);
    if (workSpace->area == NULL) {
        ret = InitWorkSpace(workSpace, readOnly, workSpace->spaceSize);
        if (ret != 0) {
            PARAM_LOGE("Forbid to open workspace for %s error %d", workSpace->fileName, errno);
        }
#ifndef PARAM_SUPPORT_SELINUX
    }
    ATOMIC_STORE_EXPLICIT(&workSpace->rwSpaceLock, rwSpaceLock & ~WORKSPACE_STATUS_IN_PROCESS, MEMORY_ORDER_RELEASE);
#else
    } else if (readOnly) {
        if ((rwSpaceLock & WORKSPACE_STATUS_VALID) == WORKSPACE_STATUS_VALID) {
            ret = 0;
        } else if ((paramSpace->flags & WORKSPACE_FLAGS_NEED_ACCESS) == WORKSPACE_FLAGS_NEED_ACCESS) {
            char buffer[FILENAME_LEN_MAX] = {0};
            int size = PARAM_SPRINTF(buffer, sizeof(buffer), "%s/%s", PARAM_STORAGE_PATH, workSpace->fileName);
            if (size > 0 && access(buffer, R_OK) == 0) {
                PARAM_LOGW("Open workspace %s access ok ", workSpace->fileName);
                rwSpaceLock |= WORKSPACE_STATUS_VALID;
                ret = 0;
            } else {
                ret = -1;
                PARAM_LOGE("Forbid to open workspace for %s error %d", workSpace->fileName, errno);
                rwSpaceLock &= ~WORKSPACE_STATUS_VALID;
            }
        }
    }
    ATOMIC_STORE_EXPLICIT(&workSpace->rwSpaceLock, rwSpaceLock & ~WORKSPACE_STATUS_IN_PROCESS, MEMORY_ORDER_RELEASE);
    futex_wake_private(&workSpace->rwSpaceLock, INT_MAX);
#endif
    return ret;
}

STATIC_INLINE int ReadParamWithCheck(WorkSpace **workspace, const char *name, uint32_t op, ParamTrieNode **node)
{
    ParamLabelIndex labelIndex = {0};
    WorkSpace *dacSpace = g_paramWorkSpace.workSpace[0];
    PARAM_CHECK(dacSpace != NULL && dacSpace->area != NULL,
        return DAC_RESULT_FORBIDED, "Invalid workSpace for %s", name);
    *node = BaseFindTrieNode(dacSpace, name, strlen(name), &labelIndex.dacLabelIndex);
    labelIndex.workspace = GetWorkSpaceByName(name);
    PARAM_CHECK(labelIndex.workspace != NULL, return DAC_RESULT_FORBIDED, "Invalid workSpace for %s", name);
    labelIndex.selinuxLabelIndex = labelIndex.workspace->spaceIndex;

    int ret = CheckParamPermission_(&labelIndex, &g_paramWorkSpace.securityLabel, name, op);
    PARAM_CHECK(ret == 0, return ret, "Forbid to read parameter %s", name);
#ifdef PARAM_SUPPORT_SELINUX
    // search from real workspace
    *node = BaseFindTrieNode(labelIndex.workspace, name, strlen(name), NULL);
#endif
    *workspace = labelIndex.workspace;
    return ret;
}

static int CheckUserInGroup(WorkSpace *space, const ParamSecurityNode *node, uid_t uid)
{
    for (uint32_t i = 0; i < node->memberNum; i++) {
        if (node->members[i] == uid) {
            return 0;
        }
    }
    return -1;
}

STATIC_INLINE int DacCheckGroupPermission(const ParamSecurityLabel *srcLabel, uint32_t mode, ParamSecurityNode *node)
{
    uint32_t localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
    if (srcLabel->cred.gid == node->gid) {
        if ((node->mode & localMode) != 0) {
            return DAC_RESULT_PERMISSION;
        }
    }
    if (mode != DAC_WRITE || g_paramWorkSpace.ops.getServiceGroupIdByPid == NULL) {
        return DAC_RESULT_FORBIDED;
    }
    gid_t gids[64] = { 0 }; // max gid number
    const uint32_t gidNumber = (uint32_t)g_paramWorkSpace.ops.getServiceGroupIdByPid(
        srcLabel->cred.pid, gids, sizeof(gids) / sizeof(gids[0]));
    for (uint32_t index = 0; index < gidNumber; index++) {
        PARAM_LOGV("DacCheckGroupPermission gid %u", gids[index]);
        if (gids[index] != node->gid) {
            continue;
        }
        if ((node->mode & localMode) != 0) {
            return DAC_RESULT_PERMISSION;
        }
    }
    return DAC_RESULT_FORBIDED;
}

STATIC_INLINE int DacCheckParamPermission(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
#ifndef STARTUP_INIT_TEST
    if (srcLabel->cred.uid == 0) {
        return DAC_RESULT_PERMISSION;
    }
#endif
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
        return DAC_RESULT_PERMISSION;
    }
    // 2, check uid
    if (srcLabel->cred.uid == node->uid) {
        localMode = mode & (DAC_READ | DAC_WRITE | DAC_WATCH);
        if ((node->mode & localMode) != 0) {
            return DAC_RESULT_PERMISSION;
        }
    }
    // 3, check gid
    if (DacCheckGroupPermission(srcLabel, mode, node) == DAC_RESULT_PERMISSION) {
        return DAC_RESULT_PERMISSION;
    }
    // 4, check user in group
    if (CheckUserInGroup(space, node, srcLabel->cred.uid) == 0) {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
        if ((node->mode & localMode) != 0) {
            return DAC_RESULT_PERMISSION;
        }
    }
    // forbid
    PARAM_LOGW("Param '%s' label gid:%d uid:%d mode 0%x", name, srcLabel->cred.gid, srcLabel->cred.uid, mode);
    PARAM_LOGW("Cfg label %u gid:%d uid:%d mode 0%x ", labelIndex->dacLabelIndex, node->gid, node->uid, node->mode);

    int ret = DAC_RESULT_FORBIDED;
#ifndef __MUSL__
#ifndef STARTUP_INIT_TEST
    ret = DAC_RESULT_PERMISSION;
#endif
#endif
    return ret;
}

#ifdef PARAM_SUPPORT_SELINUX
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
    int ret = SELINUX_RESULT_FORBIDED;
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
        ret = OpenWorkSpace(labelIndex->selinuxLabelIndex, 1);
    }
    if (ret != 0) {
        ret = SELINUX_RESULT_FORBIDED;
        PARAM_LOGE("Selinux check name %s in %s info [%d %d %d] failed!",
            name, GetSelinuxContent(name), srcLabel->cred.pid, srcLabel->cred.uid, srcLabel->cred.gid);
    }
    return ret;
}
#endif

#if defined(STARTUP_INIT_TEST) || defined(__LITEOS_A__) || defined(__LITEOS_M__)
static int CheckParamPermission_(const ParamLabelIndex *labelIndex,
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
static int CheckParamPermission_(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    // only for root and write, all permission, but for appspawn must to check
    // for clod start in new namespace, pid==1 and uid==root
    if (srcLabel->cred.uid == 0 && srcLabel->cred.pid == 1 && mode == DAC_WRITE) {
        return DAC_RESULT_PERMISSION;
    }
    int ret = 0;
    if (srcLabel->cred.uid < PUBLIC_APP_BEGIN_UID) {
        ret = DacCheckParamPermission(labelIndex, srcLabel, name, mode);
    }
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
    if (name == NULL || defValue == NULL) {
        return NULL;
    }
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
    PARAM_CHECK(workspace != NULL && workspace->area != NULL, return NULL, "Forbid to access parameter %s", name);

    CachedParameter *param = (CachedParameter *)malloc(
        sizeof(CachedParameter) + PARAM_ALIGN(nameLen) + 1 + PARAM_VALUE_LEN_MAX);
    PARAM_CHECK(param != NULL, return NULL, "Failed to create CachedParameter for %s", name);
    ret = PARAM_STRCPY(param->data, nameLen + 1, name);
    PARAM_CHECK(ret == 0, free(param);
        return NULL, "Failed to copy name %s", name);
    param->cachedParameterCheck = CachedParameterCheck;
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
        ret = PARAM_STRCPY(param->paramValue, param->bufferLen, defValue);
        PARAM_CHECK(ret == 0, free(param);
            return NULL, "Failed to copy name %s", name);
    }
    param->spaceCommitId = ATOMIC_UINT64_LOAD_EXPLICIT(&workspace->area->commitId, MEMORY_ORDER_ACQUIRE);
    PARAM_LOGV("CachedParameterCreate %u %u %lld \n", param->dataIndex, param->dataCommitId, param->spaceCommitId);
    return (CachedHandle)param;
}

STATIC_INLINE const char *CachedParameterCheck(CachedParameter *param, int *changed)
{
    *changed = 0;
    if (param->dataIndex == 0) {
        ParamTrieNode *node = BaseFindTrieNode(param->workspace, param->data, param->nameLen, NULL);
        if (node != NULL) {
            param->dataIndex = node->dataIndex;
        } else {
            return param->paramValue;
        }
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(param->workspace, param->dataIndex);
    PARAM_CHECK(entry != NULL, return param->paramValue, "Failed to get trie node %s", param->data);
    uint32_t dataCommitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, MEMORY_ORDER_ACQUIRE);
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

void CachedParameterDestroy(CachedHandle handle)
{
    if (handle != NULL) {
        free(handle);
    }
}
