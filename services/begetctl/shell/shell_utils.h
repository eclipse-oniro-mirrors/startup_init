/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BSHELL_UTILS_
#define BSHELL_UTILS_
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

#include "init_log.h"
#include "securec.h"

#define BSH_LOG_FILE "paramshell.log"
#define BSH_LABEL "SHELL"
#define BSH_LOGI(fmt, ...) STARTUP_LOGI(BSH_LOG_FILE, BSH_LABEL, fmt, ##__VA_ARGS__)
#define BSH_LOGE(fmt, ...) STARTUP_LOGE(BSH_LOG_FILE, BSH_LABEL, fmt, ##__VA_ARGS__)
#define BSH_LOGV(fmt, ...) STARTUP_LOGV(BSH_LOG_FILE, BSH_LABEL, fmt, ##__VA_ARGS__)

#define BSH_CHECK(ret, exper, ...) \
    if (!(ret)) { \
        BSH_LOGE(__VA_ARGS__); \
        exper; \
    }
#define BSH_ONLY_CHECK(ret, exper, ...) \
    if (!(ret)) { \
        exper; \
    }
#endif