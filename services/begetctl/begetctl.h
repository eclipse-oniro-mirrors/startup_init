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

#ifndef __BEGETCTL_CMD_H
#define __BEGETCTL_CMD_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "shell.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef int (*BegetCtlCmdPtr)(int argc, char **argv);

int BegetCtlCmdAdd(const char *name, BegetCtlCmdPtr cmd);

#define MODULE_CONSTRUCTOR(void) static void _init(void) __attribute__((constructor)); static void _init(void)
#define MODULE_DESTRUCTOR(void) static void _destroy(void) __attribute__((destructor)); static void _destroy(void)

BShellHandle GetShellHandle(void);
void demoExit(void);

int SetParamShellPrompt(BShellHandle shell, const char *param);
int32_t BShellParamCmdRegister(BShellHandle shell, int execMode);
int32_t BShellCmdRegister(BShellHandle shell, int execMode);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // INIT_UTILS_H