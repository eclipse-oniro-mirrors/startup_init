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

#ifndef STARTUP_INIT_CONTEXT
#define STARTUP_INIT_CONTEXT
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#include "init_cmds.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_CMD_LEN 512
typedef enum {
    SUB_INIT_STATE_IDLE,
    SUB_INIT_STATE_STARTING,
    SUB_INIT_STATE_RUNNING
} SubInitState;

typedef struct {
    InitContextType type;
    int sendFd;
    int recvFd;
    pid_t subPid;
    SubInitState state;
} SubInitInfo;

typedef struct {
    InitContextType type;
    int (*startSubInit)(InitContextType type);
    void (*stopSubInit)(pid_t pid);
    int (*executeCmdInSubInit)(InitContextType type, const char *, const char *);
    int (*setSubInitContext)(InitContextType type);
} SubInitContext;

int StartSubInit(InitContextType type);
int InitSubInitContext(InitContextType type, const SubInitContext *context);
#ifdef STARTUP_INIT_TEST
SubInitInfo *GetSubInitInfo(InitContextType type);
#endif

typedef struct {
    int socket[2];
    InitContextType type;
} SubInitForkArg;
pid_t SubInitFork(int (*childFunc)(const SubInitForkArg *arg), const SubInitForkArg *args);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif