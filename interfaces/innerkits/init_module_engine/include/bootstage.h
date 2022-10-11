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

#ifndef BASE_STARTUP_BOOTSTAGE_H
#define BASE_STARTUP_BOOTSTAGE_H

#include "hookmgr.h"
#include "cJSON.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

enum INIT_BOOTSTAGE {
    INIT_GLOBAL_INIT       = 0,
    INIT_PRE_PARAM_SERVICE = 10,
    INIT_PRE_PARAM_LOAD    = 20,
    INIT_PRE_CFG_LOAD      = 30,
    INIT_SERVICE_PARSE     = 35,
    INIT_POST_PERSIST_PARAM_LOAD   = 40,
    INIT_POST_CFG_LOAD     = 50,
    INIT_CMD_RECORD     = 51,
    INIT_REBOOT            = 55,
    INIT_SERVICE_CLEAR     = 56,
    INIT_SERVICE_DUMP      = 57,
    INIT_SERVICE_FORK_BEFORE       = 58,
    INIT_SERVICE_SET_PERMS = 59,
    INIT_SERVICE_FORK_AFTER = 60,
    INIT_JOB_PARSE         = 70,
};

HOOK_MGR *GetBootStageHookMgr();

__attribute__((always_inline)) inline int InitAddGlobalInitHook(int prio, OhosHook hook)
{
    return HookMgrAdd(GetBootStageHookMgr(), INIT_GLOBAL_INIT, prio, hook);
}

__attribute__((always_inline)) inline int InitAddPreParamServiceHook(int prio, OhosHook hook)
{
    return HookMgrAdd(GetBootStageHookMgr(), INIT_PRE_PARAM_SERVICE, prio, hook);
}

__attribute__((always_inline)) inline int InitAddPreParamLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(GetBootStageHookMgr(), INIT_PRE_PARAM_LOAD, prio, hook);
}

__attribute__((always_inline)) inline int InitAddPreCfgLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(GetBootStageHookMgr(), INIT_PRE_CFG_LOAD, prio, hook);
}

__attribute__((always_inline)) inline int InitAddPostCfgLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(GetBootStageHookMgr(), INIT_POST_CFG_LOAD, prio, hook);
}

__attribute__((always_inline)) inline int InitAddPostPersistParamLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, prio, hook);
}

/**
 * @brief service config parsing context information
 */
typedef struct tagSERVICE_PARSE_CTX {
    const char *serviceName;    /* Service name */
    const cJSON *serviceNode;   /* Service JSON node */
} SERVICE_PARSE_CTX;

/**
 * @brief job config parsing context information
 */
typedef struct tagJOB_PARSE_CTX {
    const char *jobName;    /* job name */
    const cJSON *jobNode;   /* job JSON node */
} JOB_PARSE_CTX;

/**
 * @brief service info
 */
typedef struct tagSERVICE_INFO_CTX {
    const char *serviceName;    /* Service name */
    const char *reserved;       /* reserved info */
} SERVICE_INFO_CTX;

/**
 * @brief init cmd info
 */
typedef struct InitCmdInfo {
    const char *cmdName;    /* cmd name */
    const char *cmdContent;    /* cmd content */
    const char *reserved;       /* reserved info */
} INIT_CMD_INFO;

/**
 * @brief service config parse hook function prototype
 *
 * @param serviceParseCtx service config parsing context information
 * @return None
 */
typedef void (*ServiceParseHook)(SERVICE_PARSE_CTX *serviceParseCtx);

/**
 * @brief job config parse hook function prototype
 *
 * @param JobParseHook job config parsing context information
 * @return None
 */
typedef void (*JobParseHook)(JOB_PARSE_CTX *jobParseCtx);

/**
 * @brief service hook function prototype
 *
 * @param ServiceHook service info
 * @return None
 */
typedef void (*ServiceHook)(SERVICE_INFO_CTX *serviceCtx);

/**
 * @brief Register a hook for service config parsing
 *
 * @param hook service config parsing hook
 *   in the hook, we can parse extra fields in the config file.
 * @return return 0 if succeed; other values if failed.
 */
int InitAddServiceParseHook(ServiceParseHook hook);

/**
 * @brief service config parsing context information
 */
typedef struct tagReboot {
    char *reason;
} RebootHookCtx;

/**
 * @brief service config parse hook function prototype
 *
 * @param serviceParseCtx service config parsing context information
 * @return None
 */
typedef void (*InitRebootHook)(RebootHookCtx *ctx);
/**
 * @brief Register a hook for reboot
 *
 * @param hook
 *
 * @return return 0 if succeed; other values if failed.
 */
int InitAddRebootHook(InitRebootHook hook);

/**
 * @brief Register a hook for job config parsing
 *
 * @param hook job config parsing hook
 *   in the hook, we can parse extra fields in the config file.
 * @return return 0 if succeed; other values if failed.
 */
int InitAddJobParseHook(JobParseHook hook);

/**
 * @brief Register a hook for service
 *
 * @param hook service hook
 *   in the hook, we can get service.
 * @param hookState init boot state
 * @return return 0 if succeed; other values if failed.
 */
int InitAddServiceHook(ServiceHook hook, int hookState);

int InitAddClearServiceHook(ServiceHook hook);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
