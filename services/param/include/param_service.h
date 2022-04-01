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

#ifndef BASE_STARTUP_PARAM_SERVICE_H
#define BASE_STARTUP_PARAM_SERVICE_H
#include <limits.h>
#include <stdio.h>

#include "param_manager.h"
#include "sys_param.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_WATCH_FLAGS_WAIT 0x01
struct CmdLineEntry {
    char *key;
    int set;
};

typedef struct cmdLineInfo {
    const char* name;
    int (*processor)(const char* name, const char* value, int);
} cmdLineInfo;

int WriteParam(const WorkSpace *workSpace, const char *name, const char *value, uint32_t *dataIndex, int onlyAdd);

int InitPersistParamWorkSpace(const ParamWorkSpace *workSpace);
void ClosePersistParamWorkSpace(void);
int LoadPersistParam(ParamWorkSpace *workSpace);
int WritePersistParam(ParamWorkSpace *workSpace, const char *name, const char *value);

#ifdef STARTUP_INIT_TEST
int ProcessMessage(const ParamTaskPtr worker, const ParamMessage *msg);
int AddSecurityLabel(const ParamAuditData *auditData, void *context);
int OnIncomingConnect(LoopHandle loop, TaskHandle server);
PARAM_STATIC void ProcessBeforeEvent(const ParamTaskPtr stream,
    uint64_t eventId, const uint8_t *content, uint32_t size);
#endif

int ProcessParamWaitAdd(ParamWorkSpace *worksapce, const ParamTaskPtr worker, const ParamMessage *msg);
int ProcessParamWatchAdd(ParamWorkSpace *worksapce, const ParamTaskPtr worker, const ParamMessage *msg);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif