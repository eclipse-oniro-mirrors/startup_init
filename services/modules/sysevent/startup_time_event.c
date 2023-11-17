/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "sys_event.h"

#include <inttypes.h>
#include <time.h>
#include <sys/time.h>

#include "bootevent.h"
#include "init_module_engine.h"
#include "init_param.h"
#include "plugin_adapter.h"
#include "securec.h"
#include "init_utils.h"

typedef struct {
    char *buffer;
    uint32_t bufferLen;
    uint32_t currLen;
} EventArgs;

static int GetServiceName(const char *paramName, char *buffer, size_t buffSize)
{
    size_t len = strlen(paramName);
    size_t i = 0;
    for (size_t index = strlen("bootevent."); index < len; index++) {
        PLUGIN_CHECK(i <= buffSize, return -1);
        if (*(paramName + index) == '.') {
            break;
        }
        buffer[i++] = *(paramName + index);
    }
    return (int)i;
}

static int TraversalEvent(ListNode *node, void *root)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
    if (item->flags != BOOTEVENT_TYPE_SERVICE) {
        return 0;
    }
    EventArgs *args = (EventArgs *)root;
    int len = GetServiceName(item->paramName, args->buffer + args->currLen, args->bufferLen - args->currLen);
    PLUGIN_CHECK(len > 0 && (((uint32_t)len + args->currLen) < args->bufferLen), return -1,
        "Failed to format service name %s", item->paramName);
    args->currLen += (uint32_t)len;

    len = sprintf_s(args->buffer + args->currLen, args->bufferLen - args->currLen, ",%u:%u,%u:%u;",
        (uint32_t)item->timestamp[BOOTEVENT_FORK].tv_sec,
        (uint32_t)(item->timestamp[BOOTEVENT_FORK].tv_nsec / USTONSEC),
        (uint32_t)item->timestamp[BOOTEVENT_READY].tv_sec,
        (uint32_t)(item->timestamp[BOOTEVENT_READY].tv_nsec / USTONSEC));
    PLUGIN_CHECK(len > 0 && (((uint32_t)len + args->currLen) < args->bufferLen), return -1,
        "Failed to format service time %s", item->paramName);
    args->currLen += (uint32_t)len;
    return 0;
}

static void InsertBootTimeParam(char *buffer, const char *name)
{
    char bufKernel[MAX_BUFFER_LEN] = {0};
    char buf[MAX_BUFFER_LEN] = {0};
    char bootName[MAX_BUFFER_LEN] = {0};
    uint32_t bufLen = MAX_BUFFER_LEN;
    uint32_t kernelLen = MAX_BUFFER_LEN;
    int len = sprintf_s(bootName, MAX_BUFFER_LEN, "ohos.boot.time.%s", name);
    PLUGIN_CHECK(len > 0 && ((uint32_t)len < MAX_BUFFER_LEN), return, "Failed to format boot name ");
    int ret = SystemReadParam(bootName, buf, &bufLen);
    PLUGIN_CHECK(ret == 0, return, "Failed to read boot time ");
    char time[MAX_BUFFER_LEN] = {0};
    if (strcmp(name, "kernel") == 0) {
        len = sprintf_s(time, MAX_BUFFER_LEN, ";kernel,0,%s", buf);
    } else {
        int result = SystemReadParam("ohos.boot.time.kernel", bufKernel, &kernelLen);
        if (result == 0) {
            len = sprintf_s(time, MAX_BUFFER_LEN, ";init,%s,%s", bufKernel, buf);
        }
    }
    PLUGIN_CHECK(len > 0 && ((uint32_t)len < MAX_BUFFER_LEN), return, "Failed to format boot time ");
    ret = strcat_s(buffer, MAX_BUFFER_FOR_EVENT + PARAM_VALUE_LEN_MAX + MAX_BUFFER_LEN + 1, time);
    PLUGIN_CHECK(ret == 0, return, "Failed to format boot time ");
}

PLUGIN_STATIC void ReportBootEventComplete(ListNode *events)
{
    PLUGIN_CHECK(events != NULL, return, "Invalid events");
    struct timespec curr = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &curr);
    PLUGIN_CHECK(ret == 0, return);

    char *buffer = (char *)calloc(MAX_BUFFER_FOR_EVENT + PARAM_VALUE_LEN_MAX, 1);
    PLUGIN_CHECK(buffer != NULL, return, "Failed to get memory for sys event ");
    EventArgs args = { buffer, MAX_BUFFER_FOR_EVENT, 0 };
    OH_ListTraversal(events, (void *)&args, TraversalEvent, 0);
    if ((args.currLen > 1) && (args.currLen < MAX_BUFFER_FOR_EVENT)) {
        buffer[args.currLen - 1] = '\0';
    }
    InsertBootTimeParam(buffer, "kernel");
    InsertBootTimeParam(buffer, "init");

    StartupTimeEvent startupTime = {};
    startupTime.event.type = STARTUP_TIME;
    startupTime.totalTime = curr.tv_sec;
    startupTime.totalTime = startupTime.totalTime * MSECTONSEC;
    startupTime.totalTime += curr.tv_nsec / USTONSEC;
    startupTime.detailTime = buffer;
    char *reason = buffer + MAX_BUFFER_FOR_EVENT;
    uint32_t size = PARAM_VALUE_LEN_MAX;
    ret = SystemReadParam("ohos.boot.bootreason", reason, &size);
    if (ret == 0) {
        startupTime.reason = reason;
        startupTime.firstStart = 1;
    } else {
        startupTime.reason = "";
        startupTime.firstStart = 0;
    }
    PLUGIN_LOGI("SysEventInit %u.%u detailTime len %u '%s'",
        (uint32_t)curr.tv_sec, (uint32_t)(curr.tv_nsec / USTONSEC), args.currLen, startupTime.detailTime);
    ReportSysEvent(&startupTime.event);
    free(buffer);
}

#ifndef STARTUP_INIT_TEST // do not install
MODULE_CONSTRUCTOR(void)
{
    ReportBootEventComplete(GetBootEventList());
}
#endif
