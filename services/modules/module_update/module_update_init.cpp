/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <cerrno>
#include "bootstage.h"
#include "init_log.h"
#include "init_module_engine.h"

static void HandleModuleUpdate(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("HandleModuleUpdate, fork fail.");
    } else if (pid == 0) {
        INIT_LOGI("HandleModuleUpdate, exec chekc_module_update.");
        int ret = execv("/system/bin/check_module_update_init", nullptr);
        if (ret != 0) {
            INIT_LOGE("exec error, ret = [%d], err = [%s].", ret, strerror(errno));
        }
        _exit(0);
    }
}

static int CheckModuleUpdate(const HOOK_INFO *hookInfo, void *ctx)
{
    INIT_LOGI("check_module_update start.");
    static bool isExecuted = false;
    if (isExecuted) {
        INIT_LOGI("check_module_update has been executed.");
        return 0;
    }
    isExecuted = true;
    HandleModuleUpdate();
    return 0;
}

static int InitBootCompleted(const HOOK_INFO *hookInfo, void *ctx)
{
    INIT_LOGI("boot completed, uninstall module_update_init.");
    AutorunModuleMgrUnInstall("module_update_init");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    INIT_LOGI("module_update_init add hookMgr.");
    HookMgrAdd(GetBootStageHookMgr(), INIT_MOUNT_STAGE, 0, CheckModuleUpdate);
    HookMgrAdd(GetBootStageHookMgr(), INIT_BOOT_COMPLETE, 0, InitBootCompleted);
}

MODULE_DESTRUCTOR(void)
{
    INIT_LOGI("module_update_init destroy.");
    HookMgrDel(GetBootStageHookMgr(), INIT_MOUNT_STAGE, CheckModuleUpdate);
    HookMgrDel(GetBootStageHookMgr(), INIT_BOOT_COMPLETE, InitBootCompleted);
}
