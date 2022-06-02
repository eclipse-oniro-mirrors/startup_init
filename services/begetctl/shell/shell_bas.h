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
#ifndef BSHELL_BAS_H
#define BSHELL_BAS_H
#include <stdint.h>
#include <stdlib.h>
#include "shell.h"

#define BSH_VERSION "0.0.1"
#define BSH_KEY_LF 0x0A
#define BSH_KEY_CR 0x0D
#define BSH_KEY_TAB 0x09
#define BSH_KEY_BACKSPACE 0x08
#define BSH_KEY_DELETE 0x7F
#define BSH_KEY_CTRLC 0x03  // ctr + c
#define BSH_KEY_ESC 0x1B    // ecs

#define BSH_COMMAND_MAX_LENGTH (5 * 1024)
#define BSH_PARAMETER_MAX_NUMBER 10
#define BSH_CMD_NAME_END 48
#define BSH_CMD_MAX_KEY 5

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef STARTUP_INIT_TEST
#define SHELLSTATIC
#else
#define SHELLSTATIC static
#endif

typedef enum {
    BSH_IN_NORMAL = 0,
    BSH_ANSI_ESC,
    BSH_ANSI_CSI,
} BShellState;

typedef enum {
    BSH_EXEC_TASK = 0,
    BSH_EXEC_INDEPENDENT,
} BShellExecMode;

typedef struct BShellCommand_ {
    char *desc;
    char *help;
    BShellCmdExecuter_ executer;
    struct BShellCommand_ *next;
    char *multikey;
    char *multikeys[BSH_CMD_MAX_KEY];
    uint8_t argStart;
    char name[0];
} BShellCommand;

typedef struct {
    uint8_t shellState : 2;
    uint8_t tabFlag : 1;
} BShellStatus;

typedef struct BShellKey_ {
    uint8_t keyCode;
    BShellkeyHandle keyHandle;
    struct BShellKey_ *next;
} BShellKey;

typedef struct BShellEnv_ {
    char *prompt;
    char buffer[BSH_COMMAND_MAX_LENGTH];
    uint16_t length;
    uint16_t cursor;
    char *args[BSH_PARAMETER_MAX_NUMBER];
    uint8_t argc;
    uint8_t shellState : 3;
    uint8_t execMode : 2;
    BShellCommand *command;
    BShellParam *param;
    BShellKey *keyHandle;
    BShellStatus status;
    BShellInput_ input;
    char data[BSH_COMMAND_MAX_LENGTH];
} BShellEnv;

BShellKey *BShellEnvGetKey(BShellHandle handle, uint8_t code);
BShellCommand *BShellEnvGetCmd(BShellHandle handle, int32_t argc, char *argv[]);
int32_t BShellEnvOutputString(BShellHandle handle, const char *string);
void BShellEnvOutputByte(BShellHandle handle, char data);
void BShellEnvOutputResult(BShellHandle handle, int32_t result);
char *BShellEnvErrString(BShellHandle handle, int32_t err);
const char *BShellEnvGetStringParam(BShellHandle handle, const char *name);

#ifdef STARTUP_INIT_TEST
void BShellEnvProcessInput(BShellHandle handle, char data);
BShellKey *BShellEnvGetDefaultKey(uint8_t code);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif