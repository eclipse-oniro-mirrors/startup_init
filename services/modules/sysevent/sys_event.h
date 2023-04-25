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
#ifndef PLUGIN_SYS_EVENT_H
#define PLUGIN_SYS_EVENT_H

#include <stdint.h>
#include <sys/types.h>
#include "init_module_engine.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_BUFFER_FOR_EVENT 1024
typedef enum {
    STARTUP_TIME = 0,
    STARTUP_EVENT_MAX
} StartupEventType;

typedef struct {
    StartupEventType type;
} StartupEvent;

typedef struct {
    StartupEvent event;
    int64_t totalTime;
    char *detailTime;
    char *reason;
    int firstStart;
    char *page;
} StartupTimeEvent;

void ReportSysEvent(const StartupEvent *event);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* PLUGIN_SYS_EVENT_H */
