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

#include "init_hisysevent.h"
#include "hisysevent_c.h"
#include "plugin_adapter.h"
#include "securec.h"
#include "init_log.h"
#include "init_utils.h"

#define STARTUP_DOMAIN "INIT"

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

void ReportServiceStart(char *serviceName, int64_t pid)
{
    if (!IsBootCompleted()) {
        return;
    }
    if (serviceName == NULL) {
        INIT_LOGE("serviceName is NULL");
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
        INIT_LOGE("Failed to copy argv");
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

void ReportChildProcessExit(const char *serviceName, int pid, int err)
{
    if (!IsBootCompleted()) {
        return;
    }
    if (serviceName == NULL) {
        INIT_LOGE("ReportChildProcessExit failed, serviceName is NULL");
        return;
    }
    char tempServiceName[32] = {0};
    if (strcpy_s(tempServiceName, sizeof(tempServiceName), serviceName) != 0) {
        INIT_LOGE("Failed to copy serviceName");
        return;
    }
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
        }
    };
    int ret = OH_HiSysEvent_Write(STARTUP_DOMAIN, "CHILD_PROCESS_EXIT",
        HISYSEVENT_BEHAVIOR, params, sizeof(params) / sizeof(params[0]));
    PLUGIN_ONLY_LOG(ret == 0, "Failed to write event ret %d", ret);
}
