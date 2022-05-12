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
    INIT_POST_CFG_LOAD     = 40
};

inline int InitAddGlobalInitHook(int prio, OhosHook hook)
{
    return HookMgrAdd(NULL, INIT_GLOBAL_INIT, prio, hook);
}

inline int InitAddPreParamServiceHook(int prio, OhosHook hook)
{
    return HookMgrAdd(NULL, INIT_PRE_PARAM_SERVICE, prio, hook);
}

inline int InitAddPreParamLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(NULL, INIT_PRE_PARAM_LOAD, prio, hook);
}

inline int InitAddPreCfgLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(NULL, INIT_PRE_CFG_LOAD, prio, hook);
}

inline int InitAddPostCfgLoadHook(int prio, OhosHook hook)
{
    return HookMgrAdd(NULL, INIT_POST_CFG_LOAD, prio, hook);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
