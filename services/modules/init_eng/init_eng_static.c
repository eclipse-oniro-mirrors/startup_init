/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <string.h>
#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "init_utils.h"

static int InitEngEarlyHook(const HOOK_INFO *info, void *cookie)
{
    char value[MAX_BUFFER_LEN] = {0};
    int ret = GetParameterFromCmdLine("ohos.boot.root_package", value, MAX_BUFFER_LEN);
    if (ret == 0 && strcmp(value, "on") == 0) {
#ifndef STARTUP_INIT_TEST
        InitModuleMgrInstall("init_eng");
        InitModuleMgrUnInstall("init_eng");
#endif
        PLUGIN_LOGI("eng_mode enable.");
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    InitAddGlobalInitHook(0, InitEngEarlyHook);
}
