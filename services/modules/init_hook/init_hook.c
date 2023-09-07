
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
#include "init_param.h"
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

static int JobParseHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    JOB_PARSE_CTX *jobParseContext = (JOB_PARSE_CTX *)executionContext;
    JobParseHook realHook = (JobParseHook)hookInfo->hookCookie;
    realHook(jobParseContext);
    return 0;
};

int InitAddJobParseHook(JobParseHook hook)
{
    HOOK_INFO info;
    info.stage = INIT_JOB_PARSE;
    info.prio = 0;
    info.hook = JobParseHookWrapper;
    info.hookCookie = (void *)hook;

    return HookMgrAddEx(GetBootStageHookMgr(), &info);
}

static void SetLogLevelFunc(const char *value)
{
    unsigned int level;
    int ret = StringToUint(value, &level);
    PLUGIN_CHECK(ret == 0, return, "Failed make %s to unsigned int", value);
    PLUGIN_LOGI("Set log level is %d", level);
    SetInitLogLevel(level);
}

static int CmdSetLogLevel(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    UNUSED(name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid input args");
    const char *value = strrchr(argv[0], '.');
    PLUGIN_CHECK(value != NULL, return -1, "Failed get \'.\' from string %s", argv[0]);
    SetLogLevelFunc(value + 1);
    return 0;
}

static int InitCmd(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    // process cmd by name
    PLUGIN_LOGI("InitCmd %s argc %d", name, argc);
    if (argc > 1 && strcmp(argv[0], "setloglevel") == 0) {
        SetLogLevelFunc(argv[1]);
    }
    return 0;
}

static int ParamSetInitCmdHook(const HOOK_INFO *hookInfo, void *cookie)
{
    AddCmdExecutor("setloglevel", CmdSetLogLevel);
    AddCmdExecutor("initcmd", InitCmd);
    return 0;
}

static int DumpTrigger(const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    InitLog(INIT_INFO, INIT_LOG_DOMAIN, INIT_LOG_TAG, fmt, vargs);
    va_end(vargs);
    return 0;
}

static void DumpServiceHook(void)
{
    // check and dump all jobs
    char dump[8] = {0}; // 8 len
    uint32_t len = sizeof(dump);
    (void)SystemReadParam("persist.init.debug.dump.trigger", dump, &len);
    PLUGIN_LOGV("boot dump trigger %s", dump);
    if (strcmp(dump, "1") == 0) {
        SystemDumpTriggers(1, DumpTrigger);
    }
    return;
}

static void InitLogLevelFromPersist(void)
{
    char logLevel[2] = {0}; // 2 is set param "persist.init.debug.loglevel" value length.
    uint32_t len = sizeof(logLevel);
    int ret = SystemReadParam(INIT_DEBUG_LEVEL, logLevel, &len);
    INIT_INFO_CHECK(ret == 0, return, "Can not get log level from param, keep the original loglevel.");
    SetLogLevelFunc(logLevel);
    return;
}

static int InitDebugHook(const HOOK_INFO *info, void *cookie)
{
    UNUSED(info);
    UNUSED(cookie);
    InitLogLevelFromPersist();
    DumpServiceHook();
    return 0;
}

// clear extend memory
static int BootCompleteCmd(const HOOK_INFO *hookInfo, void *executionContext)
{
    PLUGIN_LOGI("boot start complete");
    UNUSED(hookInfo);
    UNUSED(executionContext);

    // clear hook
    HookMgrDel(GetBootStageHookMgr(), INIT_GLOBAL_INIT, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_PRE_PARAM_SERVICE, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_PARAM_LOAD_FILTER, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_PRE_PARAM_LOAD, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_PRE_CFG_LOAD, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_SERVICE_PARSE, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_POST_CFG_LOAD, NULL);
    HookMgrDel(GetBootStageHookMgr(), INIT_JOB_PARSE, NULL);
    // clear cmd
    RemoveCmdExecutor("loadSelinuxPolicy", -1);

    PluginExecCmdByName("init_trace", "stop");
    // uninstall module of inittrace
    InitModuleMgrUnInstall("inittrace");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    HookMgrAdd(GetBootStageHookMgr(), INIT_BOOT_COMPLETE, 0, BootCompleteCmd);
    InitAddGlobalInitHook(0, ParamSetInitCmdHook);
    // Depends on parameter service
    InitAddPostPersistParamLoadHook(0, InitDebugHook);
}
