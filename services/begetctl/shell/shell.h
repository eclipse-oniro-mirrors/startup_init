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
#ifndef _BSHELL_H_
#define _BSHELL_H_
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_REVERESD_NAME_CURR_PARAMETER "_current_param_"
#define PARAM_SHELL_DEFAULT_PROMPT "param#"

typedef struct BShellEnv_ *BShellHandle;
typedef int32_t (*BShellCmdExecuter_)(BShellHandle handle, int32_t argc, char *argv[]);
typedef int32_t (*BShellInput_)(char *, int32_t);
typedef int32_t (*BShellOutput_)(const char *, int32_t);
typedef int32_t (*BShellkeyHandle)(BShellHandle, uint8_t code);

typedef enum {
    BSH_ERRNO_BASE = 1000,
    BSH_SHELL_INFO,
    BSH_INVALID_PARAM,
    BSH_CMD_TOO_LONG,
    BSH_SHOW_CMD_LIST,
    BSH_CMD_NOT_EXIST,
    BSH_CMD_PARAM_INVALID,
    BSH_SYSTEM_ERR,
} BShellErrNo;

typedef struct BShellErrInfo_ {
    BShellErrNo err;
    char *desc;
} BShellErrInfo;

typedef struct CmdInfo_ {
    char *name;
    BShellCmdExecuter_ executer;
    char *desc;
    char *help;
    char *multikey;
} CmdInfo;

typedef enum {
    PARAM_INT8 = 0,
    PARAM_INT16,
    PARAM_INT32,
    PARAM_STRING,
} BShellParamType;

typedef struct ParamInfo_ {
    char *name;
    char *desc;
    BShellParamType type;
} ParamInfo;

typedef struct BShellParam_ {
    char *desc;
    BShellParamType type;
    union {
        char data;
        uint8_t data8;
        uint16_t data16;
        uint32_t data32;
        char *string;
    } value;
    struct BShellParam_ *next;
    char name[0];
} BShellParam;

typedef struct BShellInfo_ {
    char *prompt;
    BShellInput_ input;
} BShellInfo;

int BShellEnvInit(BShellHandle *handle, const BShellInfo *info);
int BShellEnvStart(BShellHandle handle);
void BShellEnvDestory(BShellHandle handle);

int BShellEnvRegisterCmd(BShellHandle handle, const CmdInfo *cmdInfo);
int BShellEnvSetParam(BShellHandle handle, const char *name, const char *desc, BShellParamType type, void *value);
const BShellParam *BShellEnvGetParam(BShellHandle handle, const char *name);
int BShellEnvRegisterKeyHandle(BShellHandle handle, uint8_t code, BShellkeyHandle keyHandle);
void BShellEnvLoop(BShellHandle handle);
const ParamInfo *BShellEnvGetReservedParam(BShellHandle handle, const char *name);

int32_t BShellEnvOutput(BShellHandle handle, char *fmt, ...);
int32_t BShellEnvOutputString(BShellHandle handle, const char *string);
int32_t BShellEnvOutputPrompt(BShellHandle handle, const char *prompt);

int32_t BShellCmdHelp(BShellHandle handle, int32_t argc, char *argv[]);
int32_t BShellEnvDirectExecute(BShellHandle handle, int argc, char *args[]);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif