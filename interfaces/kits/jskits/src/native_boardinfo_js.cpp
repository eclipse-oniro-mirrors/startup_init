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

#include <string>

#include "beget_ext.h"
#include "init_error.h"
#include "sysparam_errno.h"
#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"
#include "parameter.h"

#ifndef DEVICEINFO_JS_DOMAIN
#define DEVICEINFO_JS_DOMAIN (BASE_DOMAIN + 8)
#endif

#ifndef BINFO_TAG
#define BINFO_TAG "BOARDINFO_JS"
#endif

#define BINFO_LOGI(fmt, ...) STARTUP_LOGI(DEVICEINFO_JS_DOMAIN, BINFO_TAG, fmt, ##__VA_ARGS__)
#define BINFO_LOGE(fmt, ...) STARTUP_LOGE(DEVICEINFO_JS_DOMAIN, BINFO_TAG, fmt, ##__VA_ARGS__)

#define BOARD_INFO_VALUE_LEN 256

namespace OHOS {
namespace device_info {

static napi_value MakeString(napi_env env, const std::string& value)
{
    napi_value result = nullptr;
    napi_create_string_utf8(env, value.c_str(), value.size(), &result);
    return result;
}

static napi_value GetCpuId(napi_env env, napi_callback_info info)
{
    const char *v = AclGetCpuId();
    return MakeString(env, v);
}

static napi_value GetCpuArchitecture(napi_env env, napi_callback_info info)
{
    const char *v = AclGetCpuArchitecture();
    return MakeString(env, v);
}

static napi_value GetCpuVendor(napi_env env, napi_callback_info info)
{
    const char *v = AclGetCpuVendor();
    return MakeString(env, v);
}

static napi_value GetBoardSerial(napi_env env, napi_callback_info info)
{
    char buf[BOARD_INFO_VALUE_LEN] = {0};
    int ret = AclGetBoardSerial(buf, BOARD_INFO_VALUE_LEN);
    if (ret == SYSPARAM_PERMISSION_DENIED) {
        napi_throw_error(env, std::to_string(BOARD_INFO_PERMISSION_DENIED_CODE).c_str(),
            BOARD_INFO_PERMISSION_DENIED_MSG);
        return nullptr;
    }
    return MakeString(env, std::string(buf));
}

static napi_value GetBoardVendor(napi_env env, napi_callback_info info)
{
    const char *v = AclGetBoardVendor();
    return MakeString(env, v);
}

static napi_value GetBoardProductName(napi_env env, napi_callback_info info)
{
    const char *v = AclGetBoardProductName();
    return MakeString(env, v);
}

static napi_value GetBiosVendor(napi_env env, napi_callback_info info)
{
    const char *v = AclGetBiosVendor();
    return MakeString(env, v);
}

static napi_value GetBiosVersion(napi_env env, napi_callback_info info)
{
    const char *v = AclGetBiosVersion();
    return MakeString(env, v);
}

static napi_value GetBiosReleaseDate(napi_env env, napi_callback_info info)
{
    const char *v = AclGetBiosReleaseDate();
    return MakeString(env, v);
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"cpuId", nullptr, nullptr, GetCpuId, nullptr, nullptr, napi_default, nullptr},
        {"cpuArch", nullptr, nullptr, GetCpuArchitecture, nullptr, nullptr, napi_default, nullptr},
        {"cpuVendor", nullptr, nullptr, GetCpuVendor, nullptr, nullptr, napi_default, nullptr},
        {"boardSn", nullptr, nullptr, GetBoardSerial, nullptr, nullptr, napi_default, nullptr},
        {"boardVendor", nullptr, nullptr, GetBoardVendor, nullptr, nullptr, napi_default, nullptr},
        {"boardName", nullptr, nullptr, GetBoardProductName, nullptr, nullptr, napi_default, nullptr},
        {"biosVendor", nullptr, nullptr, GetBiosVendor, nullptr, nullptr, napi_default, nullptr},
        {"biosVersion", nullptr, nullptr, GetBiosVersion, nullptr, nullptr, napi_default, nullptr},
        {"biosDate", nullptr, nullptr, GetBiosReleaseDate, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(napi_property_descriptor), desc);
    return exports;
}
EXTERN_C_END

} // namespace device_info
} // namespace OHOS

static napi_module _module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = OHOS::device_info::Init,
    .nm_modname = "boardInfo",
    .nm_priv = ((void *)0),
    .reserved = { 0 }
};

extern "C" __attribute__((constructor)) void RegisterModule(void)
{
    napi_module_register(&_module);
}
