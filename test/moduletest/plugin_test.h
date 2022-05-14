/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef PLUGIN_TEST_
#define PLUGIN_TEST_
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

#include "init_log.h"
#include "securec.h"

#ifndef PLUGIN_DOMAIN
#define PLUGIN_DOMAIN (BASE_DOMAIN + 7)
#endif
#define READ_DURATION 100000
#define PLUGIN_LABEL "PLUGIN"
#define PLUGIN_LOGI(fmt, ...) STARTUP_LOGI(PLUGIN_DOMAIN, PLUGIN_LABEL, fmt, ##__VA_ARGS__)
#define PLUGIN_LOGE(fmt, ...) STARTUP_LOGE(PLUGIN_DOMAIN, PLUGIN_LABEL, fmt, ##__VA_ARGS__)
#define PLUGIN_LOGV(fmt, ...) STARTUP_LOGV(PLUGIN_DOMAIN, PLUGIN_LABEL, fmt, ##__VA_ARGS__)

#define PLUGIN_CHECK(ret, exper, ...)          \
    if (!(ret)) {                              \
        PLUGIN_LOGE(__VA_ARGS__);              \
        exper;                                 \
    }
#define PLUGIN_ONLY_CHECK(ret, exper, ...)     \
    if (!(ret)) {                              \
        exper;                                 \
    }
#endif
