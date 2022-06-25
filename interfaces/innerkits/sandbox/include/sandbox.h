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

#ifndef BASE_STARTUP_INITLITE_SANDBOX_H
#define BASE_STARTUP_INITLITE_SANDBOX_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include "init_utils.h"

typedef struct {
    char *source;  // source 目录，一般是全局的fs 目录
    char *target;  // 沙盒化后的目录
    unsigned long flags;
    bool ignoreErrors;
} mount_t;

typedef struct MountList {
    mount_t *info;
    struct MountList *next;
} mountlist_t;

typedef struct {
    char *target;
    char *linkName;
} linker_t;

typedef struct LinkList {
    linker_t *info;
    struct LinkList *next;
} linklist_t;

typedef struct {
    mountlist_t *pathMounts;
    mountlist_t *fileMounts;
    linklist_t *links;
    char *rootPath; // /mnt/sandbox/system|vendor|xxx
    char name[MAX_BUFFER_LEN]; // name of sandbox. i.e system, chipset etc.
    bool isCreated; // sandbox already created or not
    int ns; // namespace
} sandbox_t;

bool InitSandboxWithName(const char *name);
int PrepareSandbox(const char *name);
int EnterSandbox(const char *name);
void DestroySandbox(const char *name);
void DumpSandboxByName(const char *name);
#ifdef __cplusplus
}
#endif
#endif
