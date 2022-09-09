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
#include "seccomp_policy.h"

static int SetSystemSeccompPolicy(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("SetSystemSeccompPolicy argc %d %s", argc, name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");

    bool ret = SetSeccompPolicyWithName(SYSTEM_NAME);
    PLUGIN_CHECK(ret == true, return -1, "SetSystemSeccompPolicy failed");

    return 0;
}

static int32_t g_executorId = -1;
static int SetSeccompPolicyInit(void)
{
    if (g_executorId == -1) {
        g_executorId = AddCmdExecutor("SetSeccompPolicy", SetSystemSeccompPolicy);
        PLUGIN_LOGI("SetSeccompPolicy executorId %d", g_executorId);
    }
    return 0;
}

static int SeccompHook(const HOOK_INFO *info, void *cookie)
{
    SetSeccompPolicyInit();
    PLUGIN_LOGI("seccomp enabled.");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    InitAddPostPersistParamLoadHook(0, SeccompHook);
}
