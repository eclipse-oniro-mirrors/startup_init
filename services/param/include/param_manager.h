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

#ifndef BASE_STARTUP_PARAM_MANAGER_H
#define BASE_STARTUP_PARAM_MANAGER_H
#include <pthread.h>
#include <stdio.h>
#include <string.h>

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

#define WORKSPACE_NAME_DAC "param_sec_dac"
#define WORKSPACE_NAME_NORMAL "param_storage"
#define WORKSPACE_NAME_DEF_SELINUX "u:object_r:default_param:s0"

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
    uint32_t flags;
    int clientFd;
    pthread_mutex_t mutex;
} ClientWorkSpace;

int InitParamWorkSpace(int onlyRead);
void CloseParamWorkSpace(void);
WorkSpace *GetWorkSpace(const char *name);
int AddWorkSpace(const char *name, int onlyRead, uint32_t spacesize);
WorkSpace *GetFirstWorkSpace(void);
WorkSpace *GetNextWorkSpace(WorkSpace *curr);

ParamTrieNode *GetTrieNodeByHandle(ParamHandle handle);

int CheckParameterSet(const char *name, const char *value, const ParamSecurityLabel *srcLabel, int *ctrlService);

int ReadParamWithCheck(const char *name, uint32_t op, ParamHandle *handle);
int ReadParamValue(ParamHandle handle, char *value, uint32_t *len);
int ReadParamName(ParamHandle handle, char *name, uint32_t len);
int ReadParamCommitId(ParamHandle handle, uint32_t *commitId);

int CheckParamValue(const ParamTrieNode *node, const char *name, const char *value);
int CheckParamName(const char *name, int paramInfo);
int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
ParamNode *SystemCheckMatchParamWait(const char *name, const char *value);

int WriteParam(const char *name, const char *value, uint32_t *dataIndex, int onlyAdd);
int AddSecurityLabel(const ParamAuditData *auditData);
int LoadSecurityLabel(const char *fileName);
ParamSecurityLabel *GetParamSecurityLabel(void);

typedef void (*TraversalParamPtr)(ParamHandle handle, void *context);
typedef struct {
    TraversalParamPtr traversalParamPtr;
    void *context;
    char *prefix;
} ParamTraversalContext;

const char *GetSelinuxContent(const char *name);

void LoadParamFromBuild(void);
int LoadParamFromCmdLine(void);
void LoadSelinuxLabel(void);

int InitPersistParamWorkSpace(void);
void ClosePersistParamWorkSpace(void);
int WritePersistParam(const char *name, const char *value);
long long GetPersistCommitId(void);
void UpdatePersistCommitId(void);
int SysCheckParamExist(const char *name);

#ifdef STARTUP_INIT_TEST
ParamService *GetParamService();
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif