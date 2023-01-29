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

#include "init_param.h"
#include "list.h"
#include "param_base.h"
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
    HashMapHandle workSpaceHashHandle;
    ListHead workSpaceList;
#ifdef PARAMWORKSPACE_NEED_MUTEX
    ParamRWMutex rwlock;
#endif
    PARAM_WORKSPACE_OPS ops;
#ifdef PARAM_SUPPORT_SELINUX
    SelinuxSpace selinuxSpace;
#endif
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
    time_t lastSaveTimer;
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

INIT_LOCAL_API int AddWorkSpace(const char *name, int onlyRead, uint32_t spacesize);
INIT_LOCAL_API WorkSpace *GetFirstWorkSpace(void);
INIT_LOCAL_API WorkSpace *GetNextWorkSpace(WorkSpace *curr);
INIT_LOCAL_API WorkSpace *GetWorkSpace(const char *name);
INIT_LOCAL_API ParamTrieNode *GetTrieNodeByHandle(ParamHandle handle);
INIT_LOCAL_API int ReadParamWithCheck(const char *name, uint32_t op, ParamHandle *handle);
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

INIT_LOCAL_API uint32_t ReadCommitId(ParamNode *entry);
INIT_LOCAL_API int ReadParamName(ParamHandle handle, char *name, uint32_t length);
INIT_LOCAL_API int CheckParameterSet(const char *name, const char *value,
    const ParamSecurityLabel *srcLabel, int *ctrlService);
INIT_LOCAL_API ParamHandle GetParamHandle(const WorkSpace *workSpace, uint32_t index, const char *name);
INIT_LOCAL_API int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);

INIT_LOCAL_API int SysCheckParamExist(const char *name);
INIT_LOCAL_API int GenerateKeyHasCode(const char *buff, size_t len);

INIT_INNER_API ParamWorkSpace *GetParamWorkSpace(void);
INIT_INNER_API int GetParamSecurityAuditData(const char *name, int type, ParamAuditData *auditData);
INIT_LOCAL_API int GetServiceCtrlInfo(const char *name, const char *value, ServiceCtrlInfo **ctrlInfo);

#ifdef STARTUP_INIT_TEST
ParamService *GetParamService();
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif