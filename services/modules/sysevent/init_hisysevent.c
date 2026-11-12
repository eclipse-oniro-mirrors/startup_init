/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <time.h>
#include <stdbool.h>
#include <string.h>

#include "init_hisysevent.h"
#include "hisysevent_c.h"
#include "plugin_adapter.h"
#include "securec.h"
#include "init_log.h"
#include "init_utils.h"

#ifdef STARTUP_INIT_TEST
/* In test builds, hisysevent_c.h defines OH_HiSysEvent_Write as a macro.
 * Command-line -D would conflict with that macro (Werror=redefine), so we
 * undef it here and redirect to the stub function instead.
 * The stub is linked from test/unittest/modules/init_hisysevent_stub.cpp.
 */
#undef OH_HiSysEvent_Write
int StubOhHiSysEventWrite(const char *domain, const char *eventName,
    int type, const void *params, size_t paramCount);
#define OH_HiSysEvent_Write StubOhHiSysEventWrite
#endif

#define STARTUP_DOMAIN "INIT"

#define REPORT_RATE_LIMIT_WINDOW_S    3600
#define REPORT_RATE_LIMIT_MAX_COUNT   4
#define MAX_TRACKED_SERVICES          256

#define SERVICE_NAME_MAX_LEN          32

typedef struct {
    char serviceName[SERVICE_NAME_MAX_LEN];
    time_t windowStartSec;
    uint32_t count;
} ServiceReportState;

static ServiceReportState g_reportStates[MAX_TRACKED_SERVICES];
static uint32_t g_reportStateCount = 0;

/* Copy service name with truncation at dst_size - 1 bytes. Both the rate limit
 * table and the report path use the same truncation strategy, so an over-length
 * name neither bypasses the limit nor drops the event.
 */
static void CopyTruncatedServiceName(char *dst, size_t dstSize, const char *src)
{
    size_t len = strnlen(src, dstSize - 1);
    if (memcpy_s(dst, dstSize, src, len) != 0) {
        dst[0] = '\0';
        return;
    }
    dst[len] = '\0';
}

/* Pick a reusable slot when the table is full: prefer a slot whose rate limit
 * window has expired; otherwise evict the slot with the oldest window start
 * (LRU). Returns the slot index, or -1 only when the table is empty.
 */
static int FindReusableSlotIndex(time_t nowSec)
{
    int expiredIdx = -1;
    int oldestIdx = -1;
    time_t oldestStart = 0;
    for (uint32_t i = 0; i < g_reportStateCount; i++) {
        if (nowSec - g_reportStates[i].windowStartSec > REPORT_RATE_LIMIT_WINDOW_S) {
            expiredIdx = (int)i;
            break;
        }
        if (oldestIdx == -1 || g_reportStates[i].windowStartSec < oldestStart) {
            oldestStart = g_reportStates[i].windowStartSec;
            oldestIdx = (int)i;
        }
    }
    return (expiredIdx != -1) ? expiredIdx : oldestIdx;
}

static bool CheckRateLimit(const char *serviceName)
{
    if (serviceName == NULL) {
        return true;
    }
    struct timespec now = {0};
    clock_gettime(CLOCK_MONOTONIC, &now);
    char truncName[SERVICE_NAME_MAX_LEN] = {0};
    CopyTruncatedServiceName(truncName, sizeof(truncName), serviceName);
    for (uint32_t i = 0; i < g_reportStateCount; i++) {
        if (strcmp(g_reportStates[i].serviceName, truncName) == 0) {
            if (now.tv_sec - g_reportStates[i].windowStartSec > REPORT_RATE_LIMIT_WINDOW_S) {
                g_reportStates[i].windowStartSec = now.tv_sec;
                g_reportStates[i].count = 1;
                return true;
            }
            g_reportStates[i].count++;
            return g_reportStates[i].count <= REPORT_RATE_LIMIT_MAX_COUNT;
        }
    }
    int slotIdx = -1;
    if (g_reportStateCount < MAX_TRACKED_SERVICES) {
        slotIdx = (int)g_reportStateCount;
        g_reportStateCount++;
    } else {
        /* Table is full: recycle an expired slot or evict the oldest one so the
         * new service stays under rate limiting instead of failing open.
         */
        slotIdx = FindReusableSlotIndex(now.tv_sec);
        if (slotIdx < 0) {
            return true;
        }
    }
    CopyTruncatedServiceName(g_reportStates[slotIdx].serviceName,
        sizeof(g_reportStates[slotIdx].serviceName), serviceName);
    g_reportStates[slotIdx].windowStartSec = now.tv_sec;
    g_reportStates[slotIdx].count = 1;
    return true;
}

#ifdef STARTUP_INIT_TEST
/* Test-only hook: clear the rate limit table so cases are independent. */
void ResetRateLimitState(void)
{
    g_reportStateCount = 0;
}
#endif

void ReportStartupInitReport(int64_t count)
{
    HiSysEventParam params[] = {
        {
            .name = "NUMBER_CFG",
            .t = HISYSEVENT_INT64,
            .v = { .i64 = count },
            .arraySize = 0,
        }
    };
    int ret = OH_HiSysEvent_Write(STARTUP_DOMAIN, "STARTUP_INIT_REPORT",
        HISYSEVENT_STATISTIC, params, sizeof(params) / sizeof(params[0]));
    PLUGIN_ONLY_LOG(ret == 0, "Failed to write event ret %d", ret);
}

void ReportServiceStart(char *serviceName, int64_t pid, int64_t spawnMode)
{
    if (!IsBootCompleted()) {
        return;
    }
    if (serviceName == NULL) {
        INIT_LOGE("serviceName is NULL");
        return;
    }
    if (!CheckRateLimit(serviceName)) {
        return;
    }
    HiSysEventParam params[] = {
        {
            .name = "SERVICE_NAME",
            .t = HISYSEVENT_STRING,
            .v = { .s = serviceName },
            .arraySize = 0,
        },
        {
            .name = "SERVICE_PID",
            .t = HISYSEVENT_INT64,
            .v = { .i64 = pid },
            .arraySize = 0,
        },
        {
            .name = "SERVICE_IS_SASPAWN",
            .t = HISYSEVENT_INT64,
            .v = { .i64 = spawnMode },
            .arraySize = 0,
        }
    };
    int ret = OH_HiSysEvent_Write(STARTUP_DOMAIN, "PROCESS_START",
        HISYSEVENT_BEHAVIOR, params, sizeof(params) / sizeof(params[0]));
    PLUGIN_ONLY_LOG(ret == 0, "Failed to write event ret %d", ret);
}

void ReportStartupReboot(const char *argv)
{
    if (argv == NULL) {
        INIT_LOGE("ReportStartupReboot failed, argv is NULL");
        return;
    }
    char mode[32] = {0};
    if (strcpy_s(mode, sizeof(mode), argv) != 0) {
        INIT_LOGE("failed copy argv");
        return;
    }
    HiSysEventParam params[] = {
        {
            .name = "REBOOT_MODE",
            .t = HISYSEVENT_STRING,
            .v = { .s = mode },
            .arraySize = 0,
        }
    };
    int ret = OH_HiSysEvent_Write(STARTUP_DOMAIN, "STARTUP_REBOOT",
        HISYSEVENT_BEHAVIOR, params, sizeof(params) / sizeof(params[0]));
    PLUGIN_ONLY_LOG(ret == 0, "Failed to write event ret %d", ret);
}

void ReportChildProcessExit(const char *serviceName, int pid, int err, int64_t spawnMode)
{
    if (!IsBootCompleted()) {
        return;
    }
    if (serviceName == NULL) {
        INIT_LOGE("ReportChildProcessExit failed, serviceName is NULL");
        return;
    }
    if (!CheckRateLimit(serviceName)) {
        return;
    }
    char tempServiceName[SERVICE_NAME_MAX_LEN] = {0};
    CopyTruncatedServiceName(tempServiceName, sizeof(tempServiceName), serviceName);
    HiSysEventParam params[] = {
        {
            .name = "PROCESS_NAME",
            .t = HISYSEVENT_STRING,
            .v = { .s = tempServiceName },
            .arraySize = 0,
        },
        {
            .name = "PID",
            .t = HISYSEVENT_INT64,
            .v = { .i64 = pid },
            .arraySize = 0,
        },
        {
            .name = "EXIT_CODE",
            .t = HISYSEVENT_INT64,
            .v = { .i64 = err },
            .arraySize = 0,
        },
        {
            .name = "EXIT_IS_SASPAWN",
            .t = HISYSEVENT_INT64,
            .v = { .i64 = spawnMode },
            .arraySize = 0,
        }
    };
    int ret = OH_HiSysEvent_Write(STARTUP_DOMAIN, "CHILD_PROCESS_EXIT",
        HISYSEVENT_BEHAVIOR, params, sizeof(params) / sizeof(params[0]));
    PLUGIN_ONLY_LOG(ret == 0, "Failed to write event ret %d", ret);
}
