/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#ifndef PLUGIN_BOOT_EVENT_H
#define PLUGIN_BOOT_EVENT_H
#include <sys/types.h>
#include "init_module_engine.h"
#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define BOOTEVENT_TYPE_CMD      1
#define BOOTEVENT_TYPE_JOB      2
#define BOOTEVENT_TYPE_SERVICE  3

#define BOOT_EVENT_PARA_PREFIX      "bootevent."
#define BOOT_EVENT_PARA_PREFIX_LEN  10
#define BOOT_EVENT_TIMESTAMP_MAX_LEN  50
#define BOOT_EVENT_FILEPATH_MAX_LEN  60
#define BOOT_EVENT_FINISH  2
#define MSECTONSEC 1000000
#define SECTONSEC  1000000000
#define USTONSEC  1000
#define SAVEINITBOOTEVENTMSEC  100000
#define BOOTEVENT_OUTPUT_PATH "/data/log/startup/"

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
    int flags;
} BOOT_EVENT_PARAM_ITEM;

ListNode *GetBootEventList(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* PLUGIN_BOOT_EVENT_H */
