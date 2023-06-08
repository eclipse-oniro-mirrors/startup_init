/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#ifndef BASE_STARTUP_INIT_CMDS_H
#define BASE_STARTUP_INIT_CMDS_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "cJSON.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define DEFAULT_DIR_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH | S_IROTH) // mkdir, default mode
#define SPACES_CNT_IN_CMD_MAX 10                                           // mount, max number of spaces in cmdline
#define SPACES_CNT_IN_CMD_MIN 2                                            // mount, min number of spaces in cmdline

#define LOADCFG_BUF_SIZE 128       // loadcfg, max buffer for one cmdline
#define LOADCFG_MAX_FILE_LEN 51200 // loadcfg, max file size is 50K
#define LOADCFG_MAX_LOOP 20        // loadcfg, to prevent to be trapped in infite loop
#define OCTAL_TYPE 8               // 8 means octal to decimal
#define MAX_BUFFER 256UL
#define AUTHORITY_MAX_SIZE 128

#define MAX_CMD_NAME_LEN 32
#define MAX_CMD_CONTENT_LEN 256
#define MAX_CMD_CNT_IN_ONE_JOB 200
#define MAX_COPY_BUF_SIZE 256
#define DEFAULT_COPY_ARGS_CNT 2

#define OPTIONS_SIZE 128

#define SUPPORT_MAX_ARG_FOR_EXEC 10
// one cmd line
typedef struct {
    int cmdIndex;
    char cmdContent[MAX_CMD_CONTENT_LEN + 1];
} CmdLine;

typedef struct {
    int cmdNum;
    CmdLine cmds[0];
} CmdLines;

struct CmdArgs {
    int argc;
    char *argv[0];
};

struct CmdTable {
    char name[MAX_CMD_NAME_LEN];
    unsigned char minArg;
    unsigned char maxArg;
    void (*DoFuncion)(const struct CmdArgs *ctx);
};

typedef struct INIT_TIMING_STAT {
    struct timespec startTime;
    struct timespec endTime;
} INIT_TIMING_STAT;

int GetParamValue(const char *symValue, unsigned int symLen, char *paramValue, unsigned int paramLen);
const struct CmdArgs *GetCmdArg(const char *cmdContent, const char *delim, int argsCount);
void FreeCmdArg(struct CmdArgs *cmd);
void DoCmdByName(const char *name, const char *cmdContent);
void DoCmdByIndex(int index, const char *cmdContent);
const char *GetMatchCmd(const char *cmdStr, int *index);
const char *GetCmdKey(int index);
const struct CmdTable *GetCmdTable(int *number);
int GetCmdLinesFromJson(const cJSON *root, CmdLines **cmdLines);
const struct CmdTable *GetCmdByName(const char *name);
void ExecReboot(const char *value);
char *BuildStringFromCmdArg(const struct CmdArgs *ctx, int startIndex);
void ExecCmd(const struct CmdTable *cmd, const char *cmdContent);
int SetFileCryptPolicy(const char *dir);

void OpenHidebug(const char *name);
long long InitDiffTime(INIT_TIMING_STAT *stat);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // BASE_STARTUP_INITLITE_CMDS_H
