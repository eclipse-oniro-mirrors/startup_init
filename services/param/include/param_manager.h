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
#include <stdio.h>
#include <string.h>

#include "param_message.h"
#include "param_persist.h"
#include "param_security.h"
#include "param_trie.h"
#include "param_utils.h"
#include "sys_param.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define futex(addr1, op, val, rel, addr2, val3) syscall(SYS_futex, addr1, op, val, rel, addr2, val3)
#define futex_wait_always(addr1) syscall(SYS_futex, addr1, FUTEX_WAIT, *(int *)(addr1), 0, 0, 0)
#define futex_wake_single(addr1) syscall(SYS_futex, addr1, FUTEX_WAKE, 1, 0, 0, 0)

typedef struct {
    uint32_t flags;
    WorkSpace paramSpace;
    ParamSecurityLabel *securityLabel;
    ParamSecurityOps paramSecurityOps;
    ParamTaskPtr serverTask;
    ParamTaskPtr timer;
    ParamTaskPtr watcherTask;
    char buffer[PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10]; // 10 max len
} ParamWorkSpace;

typedef struct {
    uint32_t flags;
    ParamTaskPtr saveTimer;
    time_t lastSaveTimer;
    PersistParamOps persistParamOps;
} ParamPersistWorkSpace;

int InitParamWorkSpace(ParamWorkSpace *workSpace, int onlyRead);
void CloseParamWorkSpace(ParamWorkSpace *workSpace);

int ReadParamWithCheck(const ParamWorkSpace *workSpace, const char *name, uint32_t op, ParamHandle *handle);
int ReadParamValue(const ParamWorkSpace *workSpace, ParamHandle handle, char *value, uint32_t *len);
int ReadParamName(const ParamWorkSpace *workSpace, ParamHandle handle, char *name, uint32_t len);
int ReadParamCommitId(const ParamWorkSpace *workSpace, ParamHandle handle, uint32_t *commitId);

int CheckParamValue(const WorkSpace *workSpace, const ParamTrieNode *node, const char *name, const char *value);
int CheckParamName(const char *name, int paramInfo);
int CheckParamPermission(const ParamWorkSpace *workSpace,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);

typedef void (*TraversalParamPtr)(ParamHandle handle, void *context);
typedef struct {
    TraversalParamPtr traversalParamPtr;
    void *context;
    const char *prefix;
} ParamTraversalContext;
int TraversalParam(const ParamWorkSpace *workSpace,
    const char *prefix, TraversalParamPtr walkFunc, void *cookie);

ParamWorkSpace *GetParamWorkSpace(void);
ParamWorkSpace *GetClientParamWorkSpace(void);
void DumpParameters(const ParamWorkSpace *workSpace, int verbose);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif