
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "init_hook.h"
#include "init_service.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"
#include "init_module_engine.h"
#include "init_group_manager.h"
#include "hookmgr.h"
#include "bootstage.h"

static int ServiceExtDataCompareProc(ListNode *node, void *data)
{
    ServiceExtData *item = ListEntry(node, ServiceExtData, node);
    if (item->dataId == *(uint32_t *)data) {
        return 0;
    }
    return -1;
}

static ServiceExtData *GetServiceExtData_(Service *service, uint32_t id)
{
    ListNode *node = OH_ListFind(&service->extDataNode, (void *)&id, ServiceExtDataCompareProc);
    return (ServiceExtData *)node;
}

ServiceExtData *AddServiceExtData(const char *serviceName, uint32_t id, void *data, uint32_t dataLen)
{
    Service *service = GetServiceByName(serviceName);
    PLUGIN_CHECK(service != NULL, return NULL, "Can not find service for %s", serviceName);
    ServiceExtData *extData = GetServiceExtData_(service, id);
    if (extData != NULL) {
        return NULL;
    }
    extData = calloc(1, sizeof(ServiceExtData) + dataLen);
    PLUGIN_CHECK(extData != NULL, return NULL, "Can not alloc extData for %d", id);
    OH_ListInit(&extData->node);
    extData->dataId = id;
    if (data != NULL) {
        int ret = memcpy_s(extData->data, dataLen, data, dataLen);
        if (ret == 0) {
            OH_ListAddTail(&service->extDataNode, &extData->node);
            return extData;
        }
    } else {
        OH_ListAddTail(&service->extDataNode, &extData->node);
        return extData;
    }
    free(extData);
    return NULL;
}

void DelServiceExtData(const char *serviceName, uint32_t id)
{
    Service *service = GetServiceByName(serviceName);
    PLUGIN_CHECK(service != NULL, return, "Can not find service for %s", serviceName);
    ServiceExtData *extData = GetServiceExtData_(service, id);
    if (extData == NULL) {
        return;
    }
    OH_ListRemove(&extData->node);
    free(extData);
}

ServiceExtData *GetServiceExtData(const char *serviceName, uint32_t id)
{
    Service *service = GetServiceByName(serviceName);
    PLUGIN_CHECK (service != NULL, return NULL, "Can not find service for %s", serviceName);
    return GetServiceExtData_(service, id);
}

static int ServiceClearHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    SERVICE_INFO_CTX *ctx = (SERVICE_INFO_CTX *)executionContext;
    ServiceHook realHook = (ServiceHook)hookInfo->hookCookie;
    realHook(ctx);
    return 0;
};

int InitAddClearServiceHook(ServiceHook hook)
{
    HOOK_INFO info;
    info.stage = INIT_SERVICE_CLEAR;
    info.prio = 0;
    info.hook = ServiceClearHookWrapper;
    info.hookCookie = (void *)hook;
    return HookMgrAddEx(GetBootStageHookMgr(), &info);
}

static int CmdClear_(int id, const char *name, int argc, const char **argv)
{
    SERVICE_INFO_CTX ctx = {0};
    ctx.reserved = argc >= 1 ? argv[0] : NULL;
    PLUGIN_LOGI("CmdClear_ %s cmd: %s", name, ctx.reserved);

    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        if (node->data.service == NULL) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        ctx.serviceName = node->name;
        HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_CLEAR, (void *)&ctx, NULL);
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
    return 0;
}

static int ParamSetBootEventHook(const HOOK_INFO *hookInfo, void *cookie)
{
    AddCmdExecutor("clear", CmdClear_);
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    InitAddGlobalInitHook(0, ParamSetBootEventHook);
}
