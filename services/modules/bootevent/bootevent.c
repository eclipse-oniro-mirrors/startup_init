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
#include "trigger_manager.h"
#include "init_log.h"
#include "plugin_adapter.h"
#include "init_hook.h"
#include "init_service.h"
#include "bootstage.h"
#include "securec.h"
#include "init_utils.h"

#define BOOT_EVENT_PARA_PREFIX      "bootevent."
#define BOOT_EVENT_PARA_PREFIX_LEN  10
#define BOOT_EVENT_TIMESTAMP_MAX_LEN  50
#define BOOT_EVENT_FILEPATH_MAX_LEN  60
#define SECTOMSEC  1000000
#define SECTONSEC  1000000000
#define MSECTONSEC  1000
#define BOOTEVENT_OUTPUT_PATH "/data/service/el0/startup/init/"
static int g_bootEventNum = 0;

enum {
    BOOTEVENT_FORK,
    BOOTEVENT_READY,
    BOOTEVENT_MAX
};

typedef struct tagBOOT_EVENT_PARAM_ITEM {
    ListNode    node;
    char  *paramName;
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

#define BOOT_EVENT_BOOT_COMPLETED "bootevent.boot.completed"

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
    INIT_LOGI("All bootevents are fired, boot complete now ...");
    SystemWriteParam(BOOT_EVENT_BOOT_COMPLETED, "true");
    return;
}

#define BOOT_EVENT_FIELD_NAME "bootevents"
static void ServiceParseBootEventHook(SERVICE_PARSE_CTX *serviceParseCtx)
{
    int cnt;
    cJSON *bootEvents = cJSON_GetObjectItem(serviceParseCtx->serviceNode, BOOT_EVENT_FIELD_NAME);

    // No bootevents in config file
    if (bootEvents == NULL) {
        return;
    }
    SERVICE_INFO_CTX ctx = {0};
    ctx.serviceName = serviceParseCtx->serviceName;
    HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_CLEAR, (void *)&ctx, NULL);
    // Single bootevent in config file
    if (!cJSON_IsArray(bootEvents)) {
        if (AddServiceBootEvent(serviceParseCtx->serviceName,
            cJSON_GetStringValue(bootEvents)) != 0) {
            INIT_LOGI("Add service bootevent failed %s", serviceParseCtx->serviceName);
            return;
        }
        g_bootEventNum++;
        return;
    }

    // Multiple bootevents in config file
    cnt = cJSON_GetArraySize(bootEvents);
    for (int i = 0; i < cnt; i++) {
        cJSON *item = cJSON_GetArrayItem(bootEvents, i);
        if (AddServiceBootEvent(serviceParseCtx->serviceName,
            cJSON_GetStringValue(item)) != 0) {
            INIT_LOGI("Add service bootevent failed %s", serviceParseCtx->serviceName);
            continue;
        }
        g_bootEventNum++;
    }
}

static int DoBootEventCmd(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    // argv[0] samgr.ready.true
    BootEventParaFireByName(argv[0]);
    return 0;
}

static int AddItemToJson(cJSON *root, const char *name, double startime, int tid, double durTime)
{
    cJSON *obj = cJSON_CreateObject(); // release obj at traverse done
    INIT_CHECK_RETURN_VALUE(obj != NULL, -1);
    cJSON_AddStringToObject(obj, "name", name);
    cJSON_AddNumberToObject(obj, "ts", startime);
    cJSON_AddStringToObject(obj, "ph", "X");
    cJSON_AddNumberToObject(obj, "pid", 0);
    cJSON_AddNumberToObject(obj, "tid", tid);
    cJSON_AddNumberToObject(obj, "dur", durTime);
    cJSON_AddItemToArray(root, obj);
    return 0;
}

static int BooteventTraversal(ListNode *node, void *root)
{
    static int tid = 1; // 1 bootevent start num
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
    double forkTime = item->timestamp[BOOTEVENT_FORK].tv_sec * SECTOMSEC +
        (double)item->timestamp[BOOTEVENT_FORK].tv_nsec / MSECTONSEC;
    double readyTime = item->timestamp[BOOTEVENT_READY].tv_sec * SECTOMSEC +
        (double)item->timestamp[BOOTEVENT_READY].tv_nsec / MSECTONSEC;
    double durTime = readyTime - forkTime;
    if (tid == 1) {
        // set time datum is 0
        INIT_CHECK_RETURN_VALUE(AddItemToJson((cJSON *)root, item->paramName, 0,
            1, 0) == 0, -1);
    }
    INIT_CHECK_RETURN_VALUE(AddItemToJson((cJSON *)root, item->paramName, forkTime,
        tid++, durTime > 0 ? durTime : 0) == 0, -1);
    return 0;
}

static int SaveServiceBootEvent(int id, const char *name, int argc, const char **argv)
{
    time_t nowTime = time(NULL);
    INIT_CHECK_RETURN_VALUE(nowTime > 0, -1);
    struct tm *p = localtime(&nowTime);
    INIT_CHECK_RETURN_VALUE(p != NULL, -1);
    char booteventFileName[BOOT_EVENT_FILEPATH_MAX_LEN] = "";
    INIT_CHECK_RETURN_VALUE(snprintf(booteventFileName, BOOT_EVENT_FILEPATH_MAX_LEN,
        BOOTEVENT_OUTPUT_PATH"%d%d%d-%d%d.bootevent",
        1900 + p->tm_year, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min) >= 0, -1); // 1900 is start year
    CheckAndCreatFile(booteventFileName, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    FILE *tmpFile = fopen(booteventFileName, "wr");
    INIT_CHECK_RETURN_VALUE(tmpFile != NULL, -1);
    cJSON *root = cJSON_CreateArray();
    INIT_CHECK_RETURN_VALUE(root != NULL, -1);
    OH_ListTraversal(&bootEventList, (void *)root, BooteventTraversal, 0);
    char *buff = cJSON_Print(root);
    INIT_CHECK_RETURN_VALUE(buff != NULL, -1);
    INIT_CHECK_RETURN_VALUE(fprintf(tmpFile, "%s\n", buff) >= 0, -1);
    free(buff);
    cJSON_Delete(root);
    (void)fflush(tmpFile);
    (void)fclose(tmpFile);
    return 0;
}

static int32_t g_executorId = -1;
static int ParamSetBootEventHook(const HOOK_INFO *hookInfo, void *cookie)
{
    if (g_executorId == -1) {
        g_executorId = AddCmdExecutor("bootevent", DoBootEventCmd);
        AddCmdExecutor("save.bootevent", SaveServiceBootEvent);
    }
    return 0;
}

static void DumpServiceBootEvent(SERVICE_INFO_CTX *serviceCtx)
{
    if (serviceCtx->reserved != NULL && strcmp(serviceCtx->reserved, "bootevent") != 0) {
        return;
    }
    for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
        ServiceExtData *serviceExtData = GetServiceExtData(serviceCtx->serviceName, i);
        if (serviceExtData == NULL) {
            return;
        }
        BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)serviceExtData->data;
        char booteventForkTimeStamp[BOOT_EVENT_TIMESTAMP_MAX_LEN] = "";
        char booteventReadyTimeStamp[BOOT_EVENT_TIMESTAMP_MAX_LEN] = "";
        INIT_CHECK_ONLY_RETURN(sprintf_s(booteventForkTimeStamp, BOOT_EVENT_TIMESTAMP_MAX_LEN, "%f",
            item->timestamp[BOOTEVENT_FORK].tv_sec +
            (double)item->timestamp[BOOTEVENT_FORK].tv_nsec / SECTONSEC) >= 0);
        INIT_CHECK_ONLY_RETURN(sprintf_s(booteventReadyTimeStamp, BOOT_EVENT_TIMESTAMP_MAX_LEN, "%f",
            (long)item->timestamp[BOOTEVENT_READY].tv_sec +
            (double)item->timestamp[BOOTEVENT_READY].tv_nsec / SECTONSEC) >= 0);
        printf("\t%-20.20s\t%-50s\t%-20.20s\t%-20.20s\n", serviceCtx->serviceName, item->paramName,
            booteventForkTimeStamp, booteventReadyTimeStamp);
    }
    return;
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
        }
    }
    return;
}

static void SetServiceBootEventFork(SERVICE_INFO_CTX *serviceCtx)
{
    for (int i = HOOK_ID_BOOTEVENT; i < HOOK_ID_BOOTEVENT_MAX; i++) {
        ServiceExtData *extData = GetServiceExtData(serviceCtx->serviceName, i);
        if (extData == NULL || ((BOOT_EVENT_PARAM_ITEM *)extData->data)->timestamp[BOOTEVENT_FORK].tv_sec != 0) {
            return;
        }
        INIT_CHECK_ONLY_RETURN(clock_gettime(CLOCK_MONOTONIC,
            &(((BOOT_EVENT_PARAM_ITEM *)extData->data)->timestamp[BOOTEVENT_FORK])) == 0);
    }
    return;
}

MODULE_CONSTRUCTOR(void)
{
    EnableInitLog(INIT_DEBUG);
    InitAddServiceHook(SetServiceBootEventFork, INIT_SERVICE_FORK_BEFORE);
    InitAddServiceHook(ClearServiceBootEvent, INIT_SERVICE_CLEAR);
    InitAddServiceHook(DumpServiceBootEvent, INIT_SERVICE_DUMP);
    InitAddServiceParseHook(ServiceParseBootEventHook);
    InitAddGlobalInitHook(0, ParamSetBootEventHook);
}
