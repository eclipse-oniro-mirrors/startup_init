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
#ifndef STARTUP_INIT_GROUP_MANAGER_H
#define STARTUP_INIT_GROUP_MANAGER_H
#include <stdlib.h>
#include <string.h>
#include "init_service.h"
#include "init_hashmap.h"
#include "init_cmdexecutor.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define GROUP_IMPORT_MAX_LEVEL 5
#define GROUP_NAME_MAX_LENGTH 64
#define GROUP_HASHMAP_BUCKET 32

#define GROUP_DEFAULT_PATH STARTUP_INIT_UT_PATH"/system/etc"
#define BOOT_GROUP_NAME "bootgroup"
#define BOOT_GROUP_DEFAULT "device.boot.group"

typedef enum {
    GROUP_BOOT,
    GROUP_CHARGE,
    GROUP_UNKNOW
} InitGroupType;

typedef enum {
    NODE_TYPE_JOBS,
    NODE_TYPE_SERVICES,
    NODE_TYPE_PLUGINS,
    NODE_TYPE_CMDS,
    NODE_TYPE_GROUPS,
    NODE_TYPE_MAX
} InitNodeType;

typedef struct InitGroupNode_ {
    struct InitGroupNode_ *next;
    HashNode hashNode;
    unsigned char type;
    union {
        Service *service;
        PluginCmd *cmd;
    } data;
    char name[0];
} InitGroupNode;

typedef struct {
    uint8_t initFlags;
    InitGroupType groupMode;
    InitGroupNode *groupNodes[NODE_TYPE_MAX];
    HashMapHandle hashMap[NODE_TYPE_GROUPS];
    char groupModeStr[GROUP_NAME_MAX_LENGTH];
} InitWorkspace;

void InitServiceSpace(void);
int InitParseGroupCfg(void);

int GenerateHashCode(const char *key);
InitGroupNode *AddGroupNode(int type, const char *name);
InitGroupNode *GetGroupNode(int type, const char *name);
InitGroupNode *GetNextGroupNode(int type, const InitGroupNode *curr);
void DelGroupNode(int type, const char *name);
int CheckNodeValid(int type, const char *name);
HashMapHandle GetGroupHashMap(int type);
#ifdef STARTUP_INIT_TEST
InitWorkspace *GetInitWorkspace(void);
#endif
int GetBootModeFromMisc(void);
void clearMisc(void);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_INIT_GROUP_MANAGER_H
