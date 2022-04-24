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

#ifndef STARTUP_WATCHER_UTILS_H
#define STARTUP_WATCHER_UTILS_H
#include <iostream>

#include "init_log.h"
#include "iremote_broker.h"
#include "iremote_proxy.h"

#include "securec.h"

namespace OHOS {
namespace init_param {
#define UNUSED(x) (void)(x)
#ifndef WATCHER_DOMAIN
#define WATCHER_DOMAIN (BASE_DOMAIN + 3)
#endif
#define WATCHER_LABEL "PARAM_WATCHER"
#define WATCHER_LOGI(fmt, ...) STARTUP_LOGI(WATCHER_DOMAIN, WATCHER_LABEL, fmt, ##__VA_ARGS__)
#define WATCHER_LOGE(fmt, ...) STARTUP_LOGE(WATCHER_DOMAIN, WATCHER_LABEL, fmt, ##__VA_ARGS__)
#define WATCHER_LOGV(fmt, ...) STARTUP_LOGV(WATCHER_DOMAIN, WATCHER_LABEL, fmt, ##__VA_ARGS__)

#define WATCHER_CHECK(retCode, exper, ...) \
    if (!(retCode)) {                    \
        WATCHER_LOGE(__VA_ARGS__);         \
        exper;                           \
    }
} // namespace init_param
} // namespace OHOS
#endif // STARTUP_WATCHER_UTILS_H
