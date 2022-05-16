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
#include <stdlib.h>
#include "init_log.h"
#include "init_module_engine.h"

static MODULE_MGR *defaultModuleMgr = NULL;

int InitModuleMgrInstall(const char *moduleName)
{
    if (moduleName == NULL) {
        return -1;
    }

    if (defaultModuleMgr == NULL) {
        defaultModuleMgr = ModuleMgrCreate(moduleName);
    }
    if (defaultModuleMgr == NULL) {
        return -1;
    }

    return ModuleMgrInstall(defaultModuleMgr, moduleName, 0, NULL);
}

void InitModuleMgrUnInstall(const char *moduleName)
{
    ModuleMgrUninstall(defaultModuleMgr, moduleName);
}

static int ModuleMgrCmdInstall(int id, const char *name, int argc, const char **argv)
{
    INIT_ERROR_CHECK(argv != NULL && argc >= 1, return -1, "Invalid install parameter");
    int ret;
    ret = ModuleMgrInstall(NULL, argv[0], argc-1, argv+1);
    INIT_ERROR_CHECK(ret == 0, return ret, "Install module %s fail", argv[0]);
    return 0;
}

static int ModuleMgrCmdUninstall(int id, const char *name, int argc, const char **argv)
{
    INIT_ERROR_CHECK(argv != NULL && argc >= 1, return -1, "Invalid install parameter");
    ModuleMgrUninstall(NULL, argv[0]);
    return 0;
}

static int moduleMgrCommandsInit(int stage, int prio, void *cookie)
{
    // "ohos.servicectrl.install"
    (void)AddCmdExecutor("install", ModuleMgrCmdInstall);
    (void)AddCmdExecutor("uninstall", ModuleMgrCmdUninstall);
    // read cfg and start static plugin
    return 0;
}

static int loadAutorunModules(int stage, int prio, void *cookie)
{
    MODULE_MGR *autorun = ModuleMgrScan("init/autorun");
    INIT_LOGI("Load autorun modules return %p", autorun);
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    // Depends on parameter service
    InitAddGlobalInitHook(0, loadAutorunModules);
    InitAddPreCfgLoadHook(0, moduleMgrCommandsInit);
}
