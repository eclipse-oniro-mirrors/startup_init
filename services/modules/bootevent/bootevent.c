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

#define BOOT_EVENT_PARA_PREFIX      "bootevent."
#define BOOT_EVENT_PARA_PREFIX_LEN  10
#ifdef BOOTEVENT
typedef struct tagBOOT_EVENT_PARAM_ITEM {
    ListNode    node;
    const char  *paramName;
} BOOT_EVENT_PARAM_ITEM;

static ListNode *bootEventList = NULL;

static ListNode *getBootEventParaList(bool autoCreate)
{
    if (!autoCreate) {
        return bootEventList;
    }
    if (bootEventList != NULL) {
        return bootEventList;
    }
    // Create list node
    bootEventList = (ListNode *)malloc(sizeof(ListNode));
    if (bootEventList == NULL) {
        return NULL;
    }
    OH_ListInit(bootEventList);
    return bootEventList;
}

static void BootEventParaAdd(const char *paramName)
{
    ListNode *list;
    BOOT_EVENT_PARAM_ITEM *item;

    if (paramName == NULL) {
        return;
    }

    // Only bootevent. parameters can be added
    if (strncmp(paramName, BOOT_EVENT_PARA_PREFIX, BOOT_EVENT_PARA_PREFIX_LEN) != 0) {
        return;
    }

    INIT_LOGI("Add bootevent [%s] ...", paramName);
    list = getBootEventParaList(true);
    if (list == NULL) {
        return;
    }

    // Create item
    item = (BOOT_EVENT_PARAM_ITEM *)malloc(sizeof(BOOT_EVENT_PARAM_ITEM));
    if (item == NULL) {
        return;
    }
    item->paramName = strdup(paramName);
    if (item->paramName == NULL) {
        free((void *)item);
        return;
    }

    // Add to list
    OH_ListAddTail(list, (ListNode *)item);
}

static int BootEventParaListCompareProc(ListNode *node, void *data)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;

    if (strcmp(item->paramName, (const char *)data) == 0) {
        return 0;
    }

    return -1;
}

static void BootEventParaItemDestroy(BOOT_EVENT_PARAM_ITEM *item)
{
    if (item->paramName != NULL) {
        free((void *)item->paramName);
    }
    free((void *)item);
}

#define BOOT_EVENT_BOOT_COMPLETED "bootevent.boot.completed"

static void BootEventParaFireByName(const char *paramName)
{
    ListNode *found;

    if (bootEventList == NULL) {
        return;
    }

    found = OH_ListFind(getBootEventParaList(false), (void *)paramName, BootEventParaListCompareProc);
    if (found != NULL) {
        // Remove from list
        OH_ListRemove(found);
        BootEventParaItemDestroy((BOOT_EVENT_PARAM_ITEM *)found);
    }

    // Check if all boot event params are fired
    if (OH_ListGetCnt(getBootEventParaList(false)) > 0) {
        return;
    }

    // Delete hooks for boot event
    free((void *)bootEventList);
    bootEventList = NULL;

    // All parameters are fired, set boot completed now ...
    INIT_LOGI("All bootevents are fired, boot complete now ...");
    SystemWriteParam(BOOT_EVENT_BOOT_COMPLETED, "true");
}

#define BOOT_EVENT_FIELD_NAME "bootevents"

#endif

static void ServiceParseBootEventHook(SERVICE_PARSE_CTX *serviceParseCtx)
{
    PLUGIN_LOGI("ServiceParseBootEventHook %s", serviceParseCtx->serviceName);
}

static int DoBootEventCmd(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("DoBootEventCmd argc %d %s", argc, name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    // argv[0] samgr.ready.true
    PLUGIN_LOGI("DoBootEventCmd argv %s", argv[0]);
    return 0;
}

void ClearServiceBootEventHook(SERVICE_INFO_CTX *serviceCtx)
{
    PLUGIN_LOGI("ClearServiceBootEventHook serviceName %s", serviceCtx->serviceName);
}

static int32_t g_executorId = -1;
static int ParamSetBootEventHook(const HOOK_INFO *hookInfo, void *cookie)
{
    if (g_executorId == -1) {
        g_executorId = AddCmdExecutor("bootevent", DoBootEventCmd);
        PLUGIN_LOGI("DoBootEventCmd executorId %d", g_executorId);
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    InitAddServiceParseHook(ServiceParseBootEventHook);
    InitAddGlobalInitHook(0, ParamSetBootEventHook);
    InitAddClearServiceHook(ClearServiceBootEventHook);
}
