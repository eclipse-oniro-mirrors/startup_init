/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_PARAM_MANAGER_H
#define BASE_STARTUP_PARAM_MANAGER_H
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <grp.h>

#include "init_param.h"
#include "list.h"

#include "param_osadp.h"
#include "param_persist.h"
#include "param_security.h"
#include "param_trie.h"
#include "param_utils.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_MAX_SELINUX_LABEL 256
#ifdef PARAM_SUPPORT_SELINUX
#define PARAM_DEF_SELINUX_LABEL 64
#else
#define PARAM_DEF_SELINUX_LABEL 1
#endif

#define WORKSPACE_INDEX_DAC 0
#define WORKSPACE_INDEX_BASE 1
#define WORKSPACE_INDEX_SIZE WORKSPACE_INDEX_DAC

#define WORKSPACE_NAME_DAC "param_sec_dac"
#define WORKSPACE_NAME_DEF_SELINUX "u:object_r:default_param:s0"
#ifndef PARAM_SUPPORT_SELINUX
#define WORKSPACE_NAME_NORMAL "param_storage"
#else
#define WORKSPACE_NAME_NORMAL WORKSPACE_NAME_DEF_SELINUX
#endif

#define PARAM_NEED_CHECK_IN_SERVICE 0x2
#define PARAM_CTRL_SERVICE 0x1

#define PARAM_WORKSPACE_CHECK(space, exper, ...) \
    if (((*space).flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) { \
        PARAM_LOGE(__VA_ARGS__);     \
        exper;                       \
    }

typedef struct {
    uint32_t flags;
    ParamSecurityLabel securityLabel;
    ParamSecurityOps paramSecurityOps[PARAM_SECURITY_MAX];
    PARAM_WORKSPACE_OPS ops;
#ifdef PARAM_SUPPORT_SELINUX
    SelinuxSpace selinuxSpace;
#endif
    int (*checkParamPermission)(const ParamLabelIndex *labelIndex,
        const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
    uint32_t maxSpaceCount;
    uint32_t maxLabelIndex;
    WorkSpace **workSpace;
} ParamWorkSpace;

typedef struct {
    ParamTaskPtr serverTask;
    ParamTaskPtr timer;
    ParamTaskPtr watcherTask;
} ParamService;

typedef struct {
    uint32_t flags;
    long long commitId;
    ParamTaskPtr saveTimer;
    struct timespec lastSaveTimer;
    PersistParamOps persistParamOps;
} ParamPersistWorkSpace;

typedef struct {
    char realKey[PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 1];
    char cmdName[32];
    uint32_t valueOffset;
    uint8_t ctrlParam;
} ServiceCtrlInfo;

typedef void (*TraversalParamPtr)(ParamHandle handle, void *context);
typedef struct {
    TraversalParamPtr traversalParamPtr;
    void *context;
    char *prefix;
} ParamTraversalContext;

#define  PARAM_HANDLE(workSpace, index) (ParamHandle)((workSpace)->spaceIndex << 24 | (index))
#define  PARAM_GET_HANDLE_INFO(handle, label, index) \
    do { \
        (label) = (((handle) >> 24) & 0x000000ff);  \
        (index) = (handle) & 0x00ffffff; \
        if (((index) & 0x03) != 0) { \
            (index) = 0; \
        } \
    } while (0)

INIT_LOCAL_API int AddWorkSpace(const char *name, uint32_t labelIndex, int onlyRead, uint32_t spacesize);
INIT_LOCAL_API int OpenWorkSpace(uint32_t index, int readOnly);

INIT_LOCAL_API WorkSpace *GetNextWorkSpace(WorkSpace *curr);
INIT_LOCAL_API WorkSpace *GetWorkSpace(uint32_t labelIndex);
INIT_LOCAL_API WorkSpace *GetWorkSpaceByName(const char *name);

INIT_LOCAL_API int CheckParamValue(const ParamTrieNode *node, const char *name, const char *value, uint8_t paramType);
INIT_LOCAL_API int CheckParamName(const char *name, int paramInfo);
INIT_LOCAL_API uint8_t GetParamValueType(const char *name);

INIT_LOCAL_API ParamNode *SystemCheckMatchParamWait(const char *name, const char *value);
INIT_LOCAL_API int WriteParam(const char *name, const char *value, uint32_t *dataIndex, int onlyAdd);
INIT_LOCAL_API int AddSecurityLabel(const ParamAuditData *auditData);
INIT_LOCAL_API ParamSecurityLabel *GetParamSecurityLabel(void);

INIT_LOCAL_API void LoadParamFromBuild(void);
INIT_LOCAL_API int LoadParamFromCmdLine(void);
INIT_LOCAL_API void LoadParamAreaSize(void);
INIT_LOCAL_API int InitPersistParamWorkSpace(void);
INIT_LOCAL_API void ClosePersistParamWorkSpace(void);
INIT_LOCAL_API int WritePersistParam(const char *name, const char *value);

INIT_LOCAL_API int CheckParameterSet(const char *name, const char *value,
    const ParamSecurityLabel *srcLabel, int *ctrlService);

INIT_LOCAL_API int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);

INIT_LOCAL_API int SysCheckParamExist(const char *name);
INIT_LOCAL_API int GenerateKeyHasCode(const char *buff, size_t len);

INIT_INNER_API ParamWorkSpace *GetParamWorkSpace(void);
INIT_INNER_API int GetParamSecurityAuditData(const char *name, int type, ParamAuditData *auditData);
INIT_LOCAL_API int GetServiceCtrlInfo(const char *name, const char *value, ServiceCtrlInfo **ctrlInfo);

INIT_INNER_API int InitParamWorkSpace(int onlyRead, const PARAM_WORKSPACE_OPS *ops);
INIT_LOCAL_API void CloseParamWorkSpace(void);
INIT_LOCAL_API int CheckIfUidInGroup(const gid_t groupId, const char *groupCheckName);

#ifdef STARTUP_INIT_TEST
ParamService *GetParamService();
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif