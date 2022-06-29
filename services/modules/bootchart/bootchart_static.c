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
#include <string.h>
#include "init_module_engine.h"
#include "plugin_adapter.h"

static int bootchartEarlyHook(const HOOK_INFO *info, void *cookie)
{
    char enable[4] = {}; // 4 enable size
    uint32_t size = sizeof(enable);
    SystemReadParam("persist.init.bootchart.enabled", enable, &size);
    if (strcmp(enable, "1") != 0) {
        PLUGIN_LOGI("bootchart disabled.");
        return 0;
    }

    InitModuleMgrInstall("bootchart");
    PLUGIN_LOGI("bootchart enabled.");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    // Depends on parameter service
    InitAddPostPersistParamLoadHook(0, bootchartEarlyHook);
}
