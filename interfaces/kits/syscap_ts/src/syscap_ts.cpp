/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "systemcapability.h"
#include "syscap_ts.h"
#include "beget_ext.h"

#ifdef NAPI_ENABLE

static constexpr int SYSCAP_MAX_SIZE = 100;

void BindNativeFunction(napi_env env, napi_value object, const char *name,
                        const char *moduleName, napi_callback func)
{
    std::string fullName(moduleName);
    fullName += ".";
    fullName += name;
    napi_value result = nullptr;
    napi_create_function(env, fullName.c_str(), fullName.length(), func, nullptr, &result);
    napi_set_named_property(env, object, name, result);
}

inline napi_value CreateJsUndefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

inline napi_value CreateJsValue(napi_env env, const bool &value)
{
    napi_value result;
    napi_get_boolean(env, value, &result);
    return result;
}


napi_value CanIUse(napi_env env, napi_callback_info info)
{
    if (env == nullptr || info == nullptr) {
        BEGET_LOGE("get syscap failed since env or callback info is nullptr.");
        return nullptr;
    }
    napi_value undefined = CreateJsUndefined(env);

    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != 1) {
        BEGET_LOGE("get syscap failed with invalid parameter");
        return undefined;
    }

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[0], &valueType);
    if (valueType != napi_string) {
        BEGET_LOGE("%{public}s called, Params is invalid.", __func__);
        return undefined;
    }

    char syscap[SYSCAP_MAX_SIZE] = {0};

    size_t strLen = 0;
    napi_get_value_string_utf8(env, argv[0], syscap, sizeof(syscap), &strLen);

    bool ret = HasSystemCapability(syscap);
    return CreateJsValue(env, ret);
}

#endif

void InitSyscapModule(napi_env env)
{
    #ifdef NAPI_ENABLE
    napi_value globalObject = nullptr;
    napi_get_global(env, &globalObject);
    const char *moduleName = "JsRuntime";
    BindNativeFunction(env, globalObject, "canIUse", moduleName, CanIUse);
    #endif
}