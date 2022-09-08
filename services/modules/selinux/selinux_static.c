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
#include "selinux_adp.h"

#include "init_hook.h"
#include "init_module_engine.h"
#include "plugin_adapter.h"

static int SelinuxHook(const HOOK_INFO *hookInfo, void *cookie)
{
    PLUGIN_LOGI("Install selinuxadp.");
    InitModuleMgrInstall("selinuxadp");
    return 0;
}

static void ServiceParseSelinuxHook(SERVICE_PARSE_CTX *serviceParseCtx)
{
    char *fieldStr = cJSON_GetStringValue(cJSON_GetObjectItem(serviceParseCtx->serviceNode, SECON_STR_IN_CFG));
    PLUGIN_CHECK(fieldStr != NULL, return, "No secon item in %s", serviceParseCtx->serviceName);
    PLUGIN_LOGV("Cfg %s for %s", fieldStr, serviceParseCtx->serviceName);
    DelServiceExtData(serviceParseCtx->serviceName, HOOK_ID_SELINUX);
    AddServiceExtData(serviceParseCtx->serviceName, HOOK_ID_SELINUX, fieldStr, strlen(fieldStr) + 1);
}

MODULE_CONSTRUCTOR(void)
{
    InitAddServiceParseHook(ServiceParseSelinuxHook);
    InitAddGlobalInitHook(0, SelinuxHook);
}
