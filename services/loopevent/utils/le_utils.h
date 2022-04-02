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

#ifndef LE_UTILS_
#define LE_UTILS_
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

#include "init_log.h"
#include "list.h"
#include "securec.h"

#define LE_TEST_FLAGS(flags, flag) (((flags) & (flag)) == (flag))
#define LE_SET_FLAGS(flags, flag) ((flags) |= (flag))
#define LE_CLEAR_FLAGS(flags, flag) ((flags) &= ~(flag))

#ifndef LE_DOMAIN
#define LE_DOMAIN (BASE_DOMAIN + 4)
#endif
#define LE_LABEL "LoopEvent"
#define LE_LOGI(fmt, ...) STARTUP_LOGI(LE_DOMAIN, LE_LABEL, fmt, ##__VA_ARGS__)
#define LE_LOGE(fmt, ...) STARTUP_LOGE(LE_DOMAIN, LE_LABEL, fmt, ##__VA_ARGS__)
#define LE_LOGV(fmt, ...) STARTUP_LOGV(LE_DOMAIN, LE_LABEL, fmt, ##__VA_ARGS__)

#define LE_CHECK(ret, exper, ...)                                                                                      \
    if (!(ret)) {                                                                                                      \
        LE_LOGE(__VA_ARGS__);                                                                                          \
        exper;                                                                                                         \
    }
#define LE_ONLY_CHECK(ret, exper, ...)                                                                                 \
    if (!(ret)) {                                                                                                      \
        exper;                                                                                                         \
    }

void SetNoBlock(int fd);
#endif