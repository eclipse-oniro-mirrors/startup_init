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
#include <string.h>
#include "init_module_engine.h"
#include "plugin_adapter.h"

static int InitTraceEarlyHook(const HOOK_INFO *info, void *cookie)
{
    PLUGIN_LOGI("Install inittrace.");
    InitModuleMgrInstall("inittrace");
    return 0;
}

static int InitTraceCompleteHook(const HOOK_INFO *info, void *cookie)
{
    PLUGIN_LOGI("Uninstall inittrace.");
    InitModuleMgrUnInstall("inittrace");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    InitAddPostCfgLoadHook(0, InitTraceEarlyHook);
    InitAddBootCompleteHook(0, InitTraceCompleteHook);
}
