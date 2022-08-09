/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE_STARTUP_INIT_PLUGIN_H
#define BASE_STARTUP_INIT_PLUGIN_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "modulemgr.h"
#include "bootstage.h"
#include "init_modulemgr.h"
#include "init_cmdexecutor.h"
#include "init_running_hooks.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int SystemWriteParam(const char *name, const char *value);

int SystemReadParam(const char *name, char *value, unsigned int *len);

int LoadParamsFile(const char *fileName, bool onlyAdd);

typedef int (*CmdExecutor)(int id, const char *name, int argc, const char **argv);

int AddCmdExecutor(const char *cmdName, CmdExecutor execCmd);

void RemoveCmdExecutor(const char *cmdName, int id);

int DoJobNow(const char *jobName);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_INIT_PLUGIN_H
