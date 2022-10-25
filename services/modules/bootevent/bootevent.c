/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <stdbool.h>
#include "init_module_engine.h"
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

#define BOOT_EVENT_PARA_PREFIX      "bootevent."
#define BOOT_EVENT_PARA_PREFIX_LEN  10
#define BOOT_EVENT_TIMESTAMP_MAX_LEN  50
#define BOOT_EVENT_FILEPATH_MAX_LEN  60
#define BOOT_EVENT_FINISH  2
#define SECTOMSEC  1000000
#define SECTONSEC  1000000000
#define MSECTONSEC  1000
#define SAVEINITBOOTEVENTMSEC  100000
#define BOOTEVENT_OUTPUT_PATH "/data/service/el0/startup/init/"
static int g_bootEventNum = 0;

// check bootevent enable
static int g_bootEventEnable = 1;

enum {
    BOOTEVENT_FORK,
    BOOTEVENT_READY,
    BOOTEVENT_MAX
};

typedef struct tagBOOT_EVENT_PARAM_ITEM {
    ListNode    node;
    char  *paramName;
    int pid;
    struct timespec timestamp[BOOTEVENT_MAX];
} BOOT_EVENT_PARAM_ITEM;

static ListNode bootEventList = {&bootEventList, &bootEventList};

static int BootEventParaListCompareProc(ListNode *node, void *data)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
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
    for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
        extData = AddServiceExtData(serviceName, i, NULL, sizeof(BOOT_EVENT_PARAM_ITEM));
        if (extData != NULL) {
            break;
        }
    }
    if (extData == NULL) {
        return -1;
    }
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)extData->data;
    OH_ListInit(&item->node);
    for (int i = 0; i < BOOTEVENT_MAX; i++) {
        item->timestamp[i].tv_nsec = 0;
        item->timestamp[i].tv_sec = 0;
    }
    item->paramName = strdup(paramName);
    if (item->paramName == NULL) {
        DelServiceExtData(serviceName, extData->dataId);
        INIT_LOGI("strdup failed");
        return -1;
    }
    OH_ListAddTail(&bootEventList, (ListNode *)&item->node);
    return 0;
}

static void AddInitBootEvent(const char *bootEventName)
{
    ListNode *found = NULL;
    found = OH_ListFind(&bootEventList, (void *)bootEventName, ParseBooteventCompareProc);
    if (found != NULL) {
        INIT_CHECK_ONLY_RETURN(clock_gettime(CLOCK_MONOTONIC,
            &(((BOOT_EVENT_PARAM_ITEM *)found)->timestamp[BOOTEVENT_READY])) == 0);
        return;
    }

    BOOT_EVENT_PARAM_ITEM *item = calloc(1, sizeof(BOOT_EVENT_PARAM_ITEM));
    if (item == NULL) {
        return;
    }
    OH_ListInit(&item->node);
    if (clock_gettime(CLOCK_MONOTONIC, &(item->timestamp[BOOTEVENT_FORK])) != 0) {
        free(item);
        return;
    }
    item->paramName = strdup(bootEventName);
    if (item->paramName == NULL) {
        free(item);
        return;
    }
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
    double forkTime = item->timestamp[BOOTEVENT_FORK].tv_sec * SECTOMSEC +
        (double)item->timestamp[BOOTEVENT_FORK].tv_nsec / MSECTONSEC;
    double readyTime = item->timestamp[BOOTEVENT_READY].tv_sec * SECTOMSEC +
        (double)item->timestamp[BOOTEVENT_READY].tv_nsec / MSECTONSEC;
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
    if (g_bootEventEnable == 0) {
        return 0;
    }
    time_t nowTime = time(NULL);
    INIT_CHECK_RETURN_VALUE(nowTime > 0, -1);
    struct tm *p = localtime(&nowTime);
    INIT_CHECK_RETURN_VALUE(p != NULL, -1);
    char bootEventFileName[BOOT_EVENT_FILEPATH_MAX_LEN] = "";
    INIT_CHECK_RETURN_VALUE(snprintf_s(bootEventFileName, BOOT_EVENT_FILEPATH_MAX_LEN, BOOT_EVENT_FILEPATH_MAX_LEN - 1,
        BOOTEVENT_OUTPUT_PATH"%d%d%d-%d%d.bootevent",
        1900 + p->tm_year, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min) >= 0, -1); // 1900 is start year
    CheckAndCreatFile(bootEventFileName, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    FILE *tmpFile = fopen(bootEventFileName, "wr");
    INIT_CHECK_RETURN_VALUE(tmpFile != NULL, -1);
    cJSON *root = cJSON_CreateArray();
    if (root == NULL) {
        (void)fclose(tmpFile);
        return -1;
    }
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

static void BootEventParaFireByName(const char *paramName)
{
    ListNode *found = NULL;
    char *bootEventValue = strrchr(paramName, '.');
    if (bootEventValue == NULL) {
        return;
    }
    bootEventValue[0] = '\0';

    found = OH_ListFind(&bootEventList, (void *)paramName, BootEventParaListCompareProc);
    if (found == NULL) {
        return;
    }
    if (((BOOT_EVENT_PARAM_ITEM *)found)->timestamp[BOOTEVENT_READY].tv_sec != 0) {
        return;
    }
    INIT_CHECK_ONLY_RETURN(clock_gettime(CLOCK_MONOTONIC,
        &(((BOOT_EVENT_PARAM_ITEM *)found)->timestamp[BOOTEVENT_READY])) == 0);
    g_bootEventNum--;
    // Check if all boot event params are fired
    if (g_bootEventNum > 0) {
        return;
    }
    // All parameters are fired, set boot completed now ...
    INIT_LOGI("All boot events are fired, boot complete now ...");
    SystemWriteParam(BOOT_EVENT_BOOT_COMPLETED, "true");
    g_bootEventEnable = BOOT_EVENT_FINISH;
    SaveServiceBootEvent();
    const char *clearBootEventArgv[] = {"bootevent"};
    // clear servie extra data
    PluginExecCmd("clear", ARRAY_LENGTH(clearBootEventArgv), clearBootEventArgv);
    return;
}

#define BOOT_EVENT_FIELD_NAME "bootevents"
static void ServiceParseBootEventHook(SERVICE_PARSE_CTX *serviceParseCtx)
{
    if (g_bootEventEnable == 0) {
        return;
    }
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
        g_bootEventNum++;
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
        g_bootEventNum++;
    }
}

static void AddCmdBootEvent(int argc, const char **argv)
{
    if (argc < 4) { // 4 is min args cmd boot event required
        return;
    }
    
    BOOT_EVENT_PARAM_ITEM *item = calloc(1, sizeof(BOOT_EVENT_PARAM_ITEM));
    if (item == NULL) {
        return;
    }
    OH_ListInit(&item->node);
    item->timestamp[BOOTEVENT_FORK] = ((INIT_TIMING_STAT *)argv[3])->startTime; // 3 args
    item->timestamp[BOOTEVENT_READY] = ((INIT_TIMING_STAT *)argv[3])->endTime; // 3 args
    int cmdLen = strlen(argv[1]) + strlen(argv[2]) + 1; // 2 args 1 '\0'
    item->paramName = calloc(1, cmdLen);
    if (item->paramName == NULL) {
        free(item);
        return;
    }
    for (int i = 1; i < 3; i++) { // 3 cmd content end
        INIT_CHECK_ONLY_ELOG(strcat_s(item->paramName, cmdLen, argv[i]) >= 0, "combine cmd args failed");
    }
    OH_ListAddTail(&bootEventList, (ListNode *)&item->node);
    return;
}

static int DoBootEventCmd(int id, const char *name, int argc, const char **argv)
{
    if (g_bootEventEnable == BOOT_EVENT_FINISH) {
        return 0;
    }
    // clear init boot events that recorded before persist param read
    if (g_bootEventEnable == 0) {
        const char *clearBootEventArgv[] = {"bootevent"};
        PluginExecCmd("clear", ARRAY_LENGTH(clearBootEventArgv), clearBootEventArgv);
        OH_ListRemoveAll(&bootEventList, BootEventDestroy);
        g_bootEventEnable = BOOT_EVENT_FINISH;
        return 0;
    }

    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    if (strcmp(argv[0], "init") == 0) {
        if (argc < 2) { // 2 args
            return 0;
        }
        AddInitBootEvent(argv[1]);
    } else if (strcmp(argv[0], "cmd") == 0) {
        AddCmdBootEvent(argc, argv);
    } else {
        // argv[0] samgr.ready.true
        BootEventParaFireByName(argv[0]);
    }
    return 0;
}

static int32_t g_executorId = -1;
static int ParamSetBootEventHook(const HOOK_INFO *hookInfo, void *cookie)
{
    if (g_executorId == -1) {
        g_executorId = AddCmdExecutor("bootevent", DoBootEventCmd);
    }
    return 0;
}

static void ClearServiceBootEvent(SERVICE_INFO_CTX *serviceCtx)
{
    if (serviceCtx->reserved == NULL || strcmp(serviceCtx->reserved, "bootevent") == 0) {
        for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
            ServiceExtData *extData = GetServiceExtData(serviceCtx->serviceName, i);
            if (extData == NULL) {
                return;
            }
            free(((BOOT_EVENT_PARAM_ITEM *)extData->data)->paramName);
            OH_ListRemove(&((BOOT_EVENT_PARAM_ITEM *)extData->data)->node);
            DelServiceExtData(serviceCtx->serviceName, i);
            g_bootEventNum--;
        }
        // clear service extra data first
        return;
    }
    if (strcmp(serviceCtx->reserved, "clearInitBootevent") == 0) {
        // clear init boot event
        OH_ListRemoveAll(&bootEventList, BootEventDestroy);
        g_bootEventNum = 0;
    }
    return;
}

static void SetServiceBootEventFork(SERVICE_INFO_CTX *serviceCtx)
{
    if (g_bootEventEnable == 0) {
        return;
    }
    for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
        ServiceExtData *extData = GetServiceExtData(serviceCtx->serviceName, i);
        if (extData == NULL) {
            return;
        }
        if (serviceCtx->reserved != NULL) {
            ((BOOT_EVENT_PARAM_ITEM *)extData->data)->pid = *((int *)serviceCtx->reserved);
            continue;
        }
        INIT_CHECK_ONLY_RETURN(clock_gettime(CLOCK_MONOTONIC,
            &(((BOOT_EVENT_PARAM_ITEM *)extData->data)->timestamp[BOOTEVENT_FORK])) == 0);
    }
    return;
}

static int GetBootEventFlag(const HOOK_INFO *info, void *cookie)
{
    char bootEventOpen[6] = ""; // 6 is length of bool value
    uint32_t len = sizeof(bootEventOpen);
    SystemReadParam("persist.init.bootevent.enable", bootEventOpen, &len);
    if (strcmp(bootEventOpen, "true") != 0) {
        g_bootEventEnable = 0;
    }
    return 0;
}

static int RecordInitCmd(const HOOK_INFO *info, void *cookie)
{
    INIT_CMD_INFO *cmdCtx = (INIT_CMD_INFO *)cookie;
    const char *bootEventArgv[] = {"cmd", cmdCtx->cmdName, cmdCtx->cmdContent, cmdCtx->reserved};
    return DoBootEventCmd(0, NULL, ARRAY_LENGTH(bootEventArgv), bootEventArgv);
}

MODULE_CONSTRUCTOR(void)
{
    HOOK_INFO info = {INIT_CMD_RECORD, 0, RecordInitCmd, NULL};
    HookMgrAddEx(GetBootStageHookMgr(), &info);
    InitAddServiceHook(SetServiceBootEventFork, INIT_SERVICE_FORK_BEFORE);
    InitAddServiceHook(SetServiceBootEventFork, INIT_SERVICE_FORK_AFTER);
    InitAddServiceHook(ClearServiceBootEvent, INIT_SERVICE_CLEAR);
    InitAddServiceParseHook(ServiceParseBootEventHook);
    InitAddGlobalInitHook(0, ParamSetBootEventHook);
    InitAddPostPersistParamLoadHook(0, GetBootEventFlag);
}
