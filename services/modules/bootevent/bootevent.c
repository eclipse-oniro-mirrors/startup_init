/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include "bootevent.h"

#include <stdbool.h>
#include "init_module_engine.h"
#include "init_group_manager.h"
#include "init_cmdexecutor.h"
#include "trigger_manager.h"
#include "init_log.h"
#include "plugin_adapter.h"
#include "init_hook.h"
#include "init_service.h"
#include "bootstage.h"
#include "securec.h"
#include "init_utils.h"
#include "init_cmds.h"
#include "config_policy_utils.h"

#ifdef WITH_SELINUX
#include <policycoreutils.h>
#endif

static int GetBootEventEnable(void)
{
    char bootEventOpen[6] = ""; // 6 is length of bool value
    uint32_t len = sizeof(bootEventOpen);
    SystemReadParam("persist.init.bootevent.enable", bootEventOpen, &len);
    if (strcmp(bootEventOpen, "true") == 0 || strcmp(bootEventOpen, "1") == 0) {
        return 1;
    }
    return 0;
}

static int g_bootEventNum = 0;

static ListNode bootEventList = {&bootEventList, &bootEventList};

static int BootEventParaListCompareProc(ListNode *node, void *data)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
    if (strncmp(item->paramName, BOOT_EVENT_PARA_PREFIX, BOOT_EVENT_PARA_PREFIX_LEN) != 0) {
        return -1;
    }
    if (strcmp(item->paramName + BOOT_EVENT_PARA_PREFIX_LEN, (const char *)data) == 0) {
        return 0;
    }
    return -1;
}

static int ParseBooteventCompareProc(ListNode *node, void *data)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
    if (strcmp(item->paramName, (const char *)data) == 0) {
        return 0;
    }
    return -1;
}

static int AddBootEventItem(BOOT_EVENT_PARAM_ITEM *item, const char *paramName)
{
    OH_ListInit(&item->node);
    for (int i = 0; i < BOOTEVENT_MAX; i++) {
        item->timestamp[i].tv_nsec = 0;
        item->timestamp[i].tv_sec = 0;
    }
    item->paramName = strdup(paramName);
    if (item->paramName == NULL) {
        return -1;
    }
    item->flags = BOOTEVENT_TYPE_SERVICE;
    OH_ListAddTail(&bootEventList, (ListNode *)&item->node);
    g_bootEventNum++;
    return 0;
}

static int AddBootEventItemByName(const char *paramName)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)calloc(1, sizeof(BOOT_EVENT_PARAM_ITEM));
    if (item == NULL) {
        return -1;
    }

    return AddBootEventItem(item, paramName);
}

static void SetServiceBooteventHookMgr(const char *serviceName, const char *paramName, int state)
{
#ifndef STARTUP_INIT_TEST
    SERVICE_BOOTEVENT_CTX context;
    context.serviceName = serviceName;
    context.reserved = paramName;
    context.state = state;
    HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_BOOTEVENT, (void*)(&context), NULL);
#endif
}


static int AddServiceBootEvent(const char *serviceName, const char *paramName)
{
    ServiceExtData *extData = NULL;
    ListNode *found = NULL;
    if (strncmp(paramName, BOOT_EVENT_PARA_PREFIX, BOOT_EVENT_PARA_PREFIX_LEN) != 0) {
        return -1;
    }
    found = OH_ListFind(&bootEventList, (void *)paramName, ParseBooteventCompareProc);
    if (found != NULL) {
        return -1;
    }
    // Find an empty bootevent data position
    for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
        extData = AddServiceExtData(serviceName, i, NULL, sizeof(BOOT_EVENT_PARAM_ITEM));
        if (extData != NULL) {
            break;
        }
    }

    INIT_CHECK(extData != NULL, return -1);

    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)extData->data;

    if (AddBootEventItem(item, paramName) != 0) {
        DelServiceExtData(serviceName, extData->dataId);
        return -1;
    }

    SetServiceBooteventHookMgr(serviceName, paramName, 1);
    return 0;
}

static void AddInitBootEvent(const char *bootEventName)
{
    BOOT_EVENT_PARAM_ITEM *found = NULL;
    found = (BOOT_EVENT_PARAM_ITEM *)OH_ListFind(&bootEventList, (void *)bootEventName, ParseBooteventCompareProc);
    if (found != NULL) {
        (void)clock_gettime(CLOCK_MONOTONIC, &(found->timestamp[BOOTEVENT_READY]));
        return;
    }

    BOOT_EVENT_PARAM_ITEM *item = calloc(1, sizeof(BOOT_EVENT_PARAM_ITEM));
    INIT_CHECK(item != NULL, return);

    OH_ListInit(&item->node);

    (void)clock_gettime(CLOCK_MONOTONIC, &(item->timestamp[BOOTEVENT_FORK]));

    item->paramName = strdup(bootEventName);
    INIT_CHECK(item->paramName != NULL, free(item);
        return);

    item->flags = BOOTEVENT_TYPE_JOB;
    OH_ListAddTail(&bootEventList, (ListNode *)&item->node);
    return;
}

#define BOOT_EVENT_BOOT_COMPLETED "bootevent.boot.completed"

static void BootEventDestroy(ListNode *node)
{
    BOOT_EVENT_PARAM_ITEM *bootEvent = (BOOT_EVENT_PARAM_ITEM *)node;
    INIT_CHECK(bootEvent->paramName == NULL, free((void *)bootEvent->paramName));
    free((void *)bootEvent);
}

static int AddItemToJson(cJSON *root, const char *name, double startTime, int pid, double durTime)
{
    cJSON *obj = cJSON_CreateObject(); // release obj at traverse done
    INIT_CHECK_RETURN_VALUE(obj != NULL, -1);
    cJSON_AddStringToObject(obj, "name", name);
    cJSON_AddNumberToObject(obj, "ts", startTime);
    cJSON_AddStringToObject(obj, "ph", "X");
    cJSON_AddNumberToObject(obj, "pid", pid);
    cJSON_AddNumberToObject(obj, "tid", pid);
    cJSON_AddNumberToObject(obj, "dur", durTime);
    cJSON_AddItemToArray(root, obj);
    return 0;
}

static int BootEventTraversal(ListNode *node, void *root)
{
    static int start = 0;
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
    double forkTime = item->timestamp[BOOTEVENT_FORK].tv_sec * MSECTONSEC +
        (double)item->timestamp[BOOTEVENT_FORK].tv_nsec / USTONSEC;
    double readyTime = item->timestamp[BOOTEVENT_READY].tv_sec * MSECTONSEC +
        (double)item->timestamp[BOOTEVENT_READY].tv_nsec / USTONSEC;
    double durTime = readyTime - forkTime;
    if (item->pid == 0) {
        if (durTime < SAVEINITBOOTEVENTMSEC) {
            return 0;
        }
        item->pid = 1; // 1 is init pid
    }
    if (start == 0) {
        // set trace start time 0
        INIT_CHECK_RETURN_VALUE(AddItemToJson((cJSON *)root, item->paramName, 0,
            1, 0) == 0, -1);
        start++;
    }
    INIT_CHECK_RETURN_VALUE(AddItemToJson((cJSON *)root, item->paramName, forkTime,
        item->pid, durTime > 0 ? durTime : 0) == 0, -1);
    return 0;
}

static int SaveServiceBootEvent()
{
    INIT_CHECK(GetBootEventEnable(), return 0);

    CheckAndCreatFile(BOOTEVENT_OUTPUT_PATH "bootup.trace", S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#ifdef WITH_SELINUX
    (void)RestoreconRecurse(BOOTEVENT_OUTPUT_PATH);
#endif

    FILE *tmpFile = fopen(BOOTEVENT_OUTPUT_PATH "bootup.trace", "wr");
    INIT_CHECK_RETURN_VALUE(tmpFile != NULL, -1);
    cJSON *root = cJSON_CreateArray();
    INIT_CHECK(root != NULL, (void)fclose(tmpFile);
        return -1);

    OH_ListTraversal(&bootEventList, (void *)root, BootEventTraversal, 0);
    char *buff = cJSON_Print(root);
    if (buff == NULL) {
        cJSON_Delete(root);
        (void)fclose(tmpFile);
        return -1;
    }
    INIT_CHECK_ONLY_ELOG(fprintf(tmpFile, "%s\n", buff) >= 0, "save boot event file failed");
    free(buff);
    cJSON_Delete(root);
    (void)fflush(tmpFile);
    (void)fclose(tmpFile);
    return 0;
}

static void ReportSysEvent(void)
{
    INIT_CHECK(GetBootEventEnable(), return);
#ifndef STARTUP_INIT_TEST
    InitModuleMgrInstall("eventmodule");
    InitModuleMgrUnInstall("eventmodule");
#endif
    return;
}

static void BootCompleteClearAll(void)
{
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        if (node->data.service == NULL) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
            ServiceExtData *extData = GetServiceExtData(node->name, i);
            if (extData == NULL) {
                return;
            }
            free(((BOOT_EVENT_PARAM_ITEM *)extData->data)->paramName);
            OH_ListRemove(&((BOOT_EVENT_PARAM_ITEM *)extData->data)->node);
            DelServiceExtData(node->name, i);
        }
    }

    // clear init boot event
    OH_ListRemoveAll(&bootEventList, BootEventDestroy);
    g_bootEventNum = 0;
}

static void WriteBooteventSysParam(const char *paramName)
{
    char buf[64];
    long long uptime;
    char name[PARAM_NAME_LEN_MAX];

    uptime = GetUptimeInMicroSeconds(NULL);

    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%lld", uptime);
    snprintf_s(name, sizeof(name), sizeof(name) - 1, "ohos.boot.time.%s", paramName);
    SystemWriteParam(name, buf);
}

static int BootEventParaFireByName(const char *paramName)
{
    BOOT_EVENT_PARAM_ITEM *found = NULL;

    char *bootEventValue = strrchr(paramName, '.');
    INIT_CHECK(bootEventValue != NULL, return 0);
    bootEventValue[0] = '\0';

    WriteBooteventSysParam(paramName);

    found = (BOOT_EVENT_PARAM_ITEM *)OH_ListFind(&bootEventList, (void *)paramName, BootEventParaListCompareProc);
    if (found == NULL) {
        return 0;
    }

    // Already fired
    if (found->timestamp[BOOTEVENT_READY].tv_sec > 0) {
        return 0;
    }
    INIT_CHECK_RETURN_VALUE(clock_gettime(CLOCK_MONOTONIC,
        &(found->timestamp[BOOTEVENT_READY])) == 0, 0);

    g_bootEventNum--;
    SetServiceBooteventHookMgr(NULL, paramName, 2); // 2: bootevent service has ready
    // Check if all boot event params are fired
    if (g_bootEventNum > 0) {
        return 0;
    }
    // All parameters are fired, set boot completed now ...
    INIT_LOGI("All boot events are fired, boot complete now ...");
    SystemWriteParam(BOOT_EVENT_BOOT_COMPLETED, "true");
    SaveServiceBootEvent();
    // report complete event
    ReportSysEvent();
    BootCompleteClearAll();
#ifndef STARTUP_INIT_TEST
    HookMgrExecute(GetBootStageHookMgr(), INIT_BOOT_COMPLETE, NULL, NULL);
#endif
    RemoveCmdExecutor("bootevent", -1);
    return 1;
}

#define BOOT_EVENT_FIELD_NAME "bootevents"
static void ServiceParseBootEventHook(SERVICE_PARSE_CTX *serviceParseCtx)
{
    int cnt;
    cJSON *bootEvents = cJSON_GetObjectItem(serviceParseCtx->serviceNode, BOOT_EVENT_FIELD_NAME);

    // No boot events in config file
    if (bootEvents == NULL) {
        return;
    }
    SERVICE_INFO_CTX ctx = {0};
    ctx.serviceName = serviceParseCtx->serviceName;
    HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_CLEAR, (void *)&ctx, NULL);
    // Single boot event in config file
    if (!cJSON_IsArray(bootEvents)) {
        if (AddServiceBootEvent(serviceParseCtx->serviceName,
            cJSON_GetStringValue(bootEvents)) != 0) {
            INIT_LOGI("Add service bootEvent failed %s", serviceParseCtx->serviceName);
            return;
        }
        return;
    }

    // Multiple boot events in config file
    cnt = cJSON_GetArraySize(bootEvents);
    for (int i = 0; i < cnt; i++) {
        cJSON *item = cJSON_GetArrayItem(bootEvents, i);
        if (AddServiceBootEvent(serviceParseCtx->serviceName,
            cJSON_GetStringValue(item)) != 0) {
            INIT_LOGI("Add service bootEvent failed %s", serviceParseCtx->serviceName);
            continue;
        }
    }
}

static int g_finished = 0;
static int DoBootEventCmd(int id, const char *name, int argc, const char **argv)
{
    if (g_finished) {
        return 0;
    }

    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    if (strcmp(argv[0], "init") == 0) {
        if (argc < 2) { // 2 args
            return 0;
        }
        AddInitBootEvent(argv[1]);
    } else {
        // argv[0] samgr.ready.true
        g_finished = BootEventParaFireByName(argv[0]);
    }
    return 0;
}

static void AddReservedBooteventsByFile(const char *name)
{
    char buf[MAX_PATH_LEN];

    FILE *file = fopen(name, "r");
    if (file == NULL) {
        return;
    }

    while (fgets((void *)buf, sizeof(buf) - 1, file)) {
        buf[sizeof(buf) - 1] = '\0';
        char *end = strchr(buf, '\r');
        if (end != NULL) {
            *end = '\0';
        }
        end = strchr(buf, '\n');
        if (end != NULL) {
            *end = '\0';
        }
        INIT_LOGI("Got priv-app bootevent: %s", buf);
        AddBootEventItemByName(buf);
    }
    fclose(file);
}

static void AddReservedBootevents(void) {
    CfgFiles *files = GetCfgFiles("etc/init/priv_app.bootevents");
    for (int i = MAX_CFG_POLICY_DIRS_CNT - 1; files && i >= 0; i--) {
        if (files->paths[i]) {
            AddReservedBooteventsByFile(files->paths[i]);
        }
    }
    FreeCfgFiles(files);
}

static int DoUnsetBootEventCmd(int id, const char *name, int argc, const char **argv)
{
    if ((argc < 1) || (argv[0] == NULL) || (strlen(argv[0]) <= strlen(BOOT_EVENT_PARA_PREFIX)) ||
        (strncmp(argv[0], BOOT_EVENT_PARA_PREFIX, strlen(BOOT_EVENT_PARA_PREFIX)) != 0)) {
        return INIT_EPARAMETER;
    }
    const char *eventName = argv[0] + strlen(BOOT_EVENT_PARA_PREFIX);
    BOOT_EVENT_PARAM_ITEM *item =
        (BOOT_EVENT_PARAM_ITEM *)OH_ListFind(&bootEventList, (void *)eventName, BootEventParaListCompareProc);
    PLUGIN_CHECK(item != NULL, return INIT_EPARAMETER, "item NULL");

    SystemWriteParam(argv[0], "false");
    if (g_finished != 0) {
        SystemWriteParam(BOOT_EVENT_BOOT_COMPLETED, "false");
        g_finished = 0;
    }

    item->timestamp[BOOTEVENT_READY].tv_sec = 0;
    g_bootEventNum++;
    INIT_LOGI("UnsetBootEvent %s g_bootEventNum:%d", argv[0], g_bootEventNum);
    return INIT_OK;
}

static int ParamSetBootEventHook(const HOOK_INFO *hookInfo, void *cookie)
{
    AddReservedBootevents();
    AddCmdExecutor("bootevent", DoBootEventCmd);
    AddCmdExecutor("unset_bootevent", DoUnsetBootEventCmd);
    return 0;
}

static void SetServiceBootEventFork(SERVICE_INFO_CTX *serviceCtx)
{
    BOOT_EVENT_PARAM_ITEM *item;
    for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
        ServiceExtData *extData = GetServiceExtData(serviceCtx->serviceName, i);
        if (extData == NULL) {
            return;
        }
        item = (BOOT_EVENT_PARAM_ITEM *)extData->data;
        if (serviceCtx->reserved != NULL) {
            item->pid = *((int *)serviceCtx->reserved);
        }
        INIT_CHECK_ONLY_RETURN(clock_gettime(CLOCK_MONOTONIC,
            &(item->timestamp[BOOTEVENT_FORK])) == 0);
    }
}

ListNode *GetBootEventList(void)
{
    return &bootEventList;
}

static void AddCmdBootEvent(INIT_CMD_INFO *cmdCtx)
{
    INIT_TIMING_STAT *timeStat = (INIT_TIMING_STAT *)cmdCtx->reserved;
    long long diff = InitDiffTime(timeStat);
    // If not time cost, just ignore
    if (diff < SAVEINITBOOTEVENTMSEC) {
        return;
    }
    BOOT_EVENT_PARAM_ITEM *item = calloc(1, sizeof(BOOT_EVENT_PARAM_ITEM));
    if (item == NULL) {
        return;
    }
    OH_ListInit(&item->node);
    item->timestamp[BOOTEVENT_FORK] = timeStat->startTime;
    item->timestamp[BOOTEVENT_READY] = timeStat->endTime;
    int cmdLen = strlen(cmdCtx->cmdName) + strlen(cmdCtx->cmdContent) + 1; // 2 args 1 '\0'
    item->paramName = calloc(1, cmdLen);
    if (item->paramName == NULL) {
        free(item);
        return;
    }
    INIT_CHECK_ONLY_ELOG(snprintf_s(item->paramName, cmdLen, cmdLen - 1, "%s%s",
                         cmdCtx->cmdName, cmdCtx->cmdContent) >= 0,
                         "combine cmd args failed");
    item->flags = BOOTEVENT_TYPE_CMD;
    OH_ListAddTail(&bootEventList, (ListNode *)&item->node);
}

static int RecordInitCmd(const HOOK_INFO *info, void *cookie)
{
    if (cookie == NULL) {
        return 0;
    }
    AddCmdBootEvent((INIT_CMD_INFO *)cookie);
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    // Add hook to record time-cost commands
    HOOK_INFO info = {INIT_CMD_RECORD, 0, RecordInitCmd, NULL};
    HookMgrAddEx(GetBootStageHookMgr(), &info);

    // Add hook to parse all services with bootevents
    InitAddServiceParseHook(ServiceParseBootEventHook);

    // Add hook to record start time for services with bootevents
    InitAddServiceHook(SetServiceBootEventFork, INIT_SERVICE_FORK_AFTER);

    InitAddGlobalInitHook(0, ParamSetBootEventHook);
}
