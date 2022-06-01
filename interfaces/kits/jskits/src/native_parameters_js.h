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
#ifndef NATIVE_PARAMETERS_JS_H
#define NATIVE_PARAMETERS_JS_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unistd.h>

#include "beget_ext.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "param_wrapper.h"
#include "parameter.h"

#define PARAM_JS_DOMAIN (BASE_DOMAIN + 0xc)
#define PARAM_JS_LOGI(fmt, ...) STARTUP_LOGI(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)
#define PARAM_JS_LOGE(fmt, ...) STARTUP_LOGE(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)
#define PARAM_JS_LOGV(fmt, ...) STARTUP_LOGV(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)
#define PARAM_JS_LOGW(fmt, ...) STARTUP_LOGW(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)

#define PARAM_JS_CHECK(retCode, exper, ...) \
    if (!(retCode)) {                       \
        PARAM_JS_LOGE(__VA_ARGS__);   \
        exper;                              \
    }

EXTERN_C_START
napi_value GetWatcher(napi_env env, napi_callback_info info);
napi_value ParamWait(napi_env env, napi_callback_info info);
napi_value RegisterWatcher(napi_env env, napi_value exports);
EXTERN_C_END

#endif // NATIVE_PARAMETERS_JS_H