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

#include "init_module_engine.h"
#include "hisysevent_c.h"
#include "plugin_adapter.h"

#define STARTUP_DOMAIN "INIT"

#define KEY_TOTAL_TIME  "TOTAL_TIME"
#define KEY_DETAILED_TIME "DETAILED_TIME"
#define KEY_REASON "REASON"
#define KEY_FIRST "ISFIRST"

static void ReportStartupTimeEvent(const StartupTimeEvent *startupTime)
{
    HiSysEventParam params[] = {
        {
            .name = KEY_TOTAL_TIME,
            .t = HISYSEVENT_INT64,
            .v = { .i64 = startupTime->totalTime },
            .arraySize = 0,
        },
        {
            .name = KEY_DETAILED_TIME,
            .t = HISYSEVENT_STRING,
            .v = { .s = startupTime->detailTime },
            .arraySize = 0,
        },
        {
            .name = KEY_REASON,
            .t = HISYSEVENT_STRING,
            .v = { .s = startupTime->reason },
            .arraySize = 0,
        },
        {
            .name = KEY_FIRST,
            .t = HISYSEVENT_INT32,
            .v = { .i32 = startupTime->firstStart },
            .arraySize = 0,
        }
    };
    int ret = OH_HiSysEvent_Write(STARTUP_DOMAIN, "STARTUP_TIME",
        HISYSEVENT_BEHAVIOR, params, sizeof(params) / sizeof(params[0]));
    PLUGIN_ONLY_LOG(ret == 0, "Failed to write event ret %d", ret);
}

void ReportSysEvent(const StartupEvent *event)
{
    PLUGIN_CHECK(event != NULL, return, "Invalid events");
    if (event->type == STARTUP_TIME) {
        StartupTimeEvent *startupTime = (StartupTimeEvent *)(event);
        ReportStartupTimeEvent(startupTime);
    }
}
