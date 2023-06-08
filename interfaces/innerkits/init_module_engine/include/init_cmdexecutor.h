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
#ifndef STARTUP_INIT_CMD_EXECUTOR_H
#define STARTUP_INIT_CMD_EXECUTOR_H
#include <stdlib.h>
#include <string.h>

#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct {
    ListNode cmdExecutor;
    int cmdId;
    char *name;
} PluginCmd;

typedef int (*CmdExecutor)(int id, const char *name, int argc, const char **argv);
typedef struct {
    ListNode node;
    int id;
    CmdExecutor execCmd;
} PluginCmdExecutor;

void PluginExecCmdByName(const char *name, const char *cmdContent);
void PluginExecCmdByCmdIndex(int index, const char *cmdContent);
int PluginExecCmd(const char *name, int argc, const char **argv);
const char *PluginGetCmdIndex(const char *cmdStr, int *index);
const char *GetPluginCmdNameByIndex(int index);

int AddCmdExecutor(const char *cmdName, CmdExecutor execCmd);

int AddRebootCmdExecutor(const char *cmd, CmdExecutor executor);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
