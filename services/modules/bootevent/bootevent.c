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

#define BOOT_EVENT_PARA_PREFIX      "bootevent."
#define BOOT_EVENT_PARA_PREFIX_LEN  10
#define BOOT_EVENT_TIMESTAMP_MAX_LEN  50
static int bootEventNum = 0;

enum {
    BOOTEVENT_FORK,
    BOOTEVENT_READY,
    BOOTEVENT_MAX
};

typedef struct tagBOOT_EVENT_PARAM_ITEM {
    ListNode    node;
    const char  *paramName;
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

static int AddServiceBootEvent(const char *serviceName, const char *paramName)
{
    ServiceExtData *extData = NULL;
    ListNode *found = NULL;
    if (strncmp(paramName, BOOT_EVENT_PARA_PREFIX, BOOT_EVENT_PARA_PREFIX_LEN) != 0) {
        return -1;
    }
    found = OH_ListFind(&bootEventList, (void *)paramName, BootEventParaListCompareProc);
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
    bootEventNum--;
    // Check if all boot event params are fired
    if (bootEventNum > 0) {
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
    // Single bootevent in config file
    if (!cJSON_IsArray(bootEvents)) {
        if (AddServiceBootEvent(serviceParseCtx->serviceName,
            cJSON_GetStringValue(bootEvents)) != 0) {
            INIT_LOGI("Add service bootevent failed %s", serviceParseCtx->serviceName);
            return;
        }
        bootEventNum++;
        return;
    }

    // Multiple bootevents in config file
    cnt = cJSON_GetArraySize(bootEvents);
    for (int i = 0; i < cnt; i++) {
        cJSON *item = cJSON_GetArrayItem(bootEvents, i);
        if (AddServiceBootEvent(serviceParseCtx->serviceName,
            cJSON_GetStringValue(item)) != 0) {
            INIT_LOGI("Add service bootevent failed %s", serviceParseCtx->serviceName);
            return;
        }
        bootEventNum++;
    }
}

static int DoBootEventCmd(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    // argv[0] samgr.ready.true
    BootEventParaFireByName(argv[0]);
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
        INIT_CHECK_ONLY_RETURN(sprintf_s(booteventForkTimeStamp, BOOT_EVENT_TIMESTAMP_MAX_LEN, "%ld.%ld",
            (long)item->timestamp[BOOTEVENT_FORK].tv_sec, (long)item->timestamp[BOOTEVENT_FORK].tv_nsec) >= 0);
        INIT_CHECK_ONLY_RETURN(sprintf_s(booteventReadyTimeStamp, BOOT_EVENT_TIMESTAMP_MAX_LEN, "%ld.%ld",
            (long)item->timestamp[BOOTEVENT_READY].tv_sec, (long)item->timestamp[BOOTEVENT_READY].tv_nsec) >= 0);
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
