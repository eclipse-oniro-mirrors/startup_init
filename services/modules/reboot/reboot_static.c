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
#include "init_hook.h"
#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "hookmgr.h"
#include "bootstage.h"

static int RebootHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    RebootHookCtx *ctx = (RebootHookCtx *)executionContext;
    InitRebootHook realHook = (InitRebootHook)hookInfo->hookCookie;
    realHook(ctx);
    return 0;
};

int InitAddRebootHook(InitRebootHook hook)
{
    HOOK_INFO info;
    info.stage = INIT_REBOOT;
    info.prio = 0;
    info.hook = RebootHookWrapper;
    info.hookCookie = (void *)hook;
    return HookMgrAddEx(GetBootStageHookMgr(), &info);
}

static int RebookGlobalHook(const HOOK_INFO *hookInfo, void *cookie)
{
    InitModuleMgrInstall("rebootmodule");
    PLUGIN_LOGI("Install rebootmodule.");
    return 0;
}

static void InitRebootHook_(RebootHookCtx *ctx)
{
    InitModuleMgrInstall("rebootmodule");
    PLUGIN_LOGI("Install rebootmodule.");
}

MODULE_CONSTRUCTOR(void)
{
    // 执行reboot时调用，安装reboot模块
    InitAddRebootHook(InitRebootHook_);
    InitAddGlobalInitHook(0, RebookGlobalHook);
}