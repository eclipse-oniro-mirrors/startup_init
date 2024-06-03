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

inline napi_value CreateJsNumber(napi_env env, int32_t value)
{
    napi_value result = nullptr;
    napi_create_int32(env, value, &result);
    return result;
}

inline napi_value CreateJsNumber(napi_env env, uint32_t value)
{
    napi_value result = nullptr;
    napi_create_uint32(env, value, &result);
    return result;
}

inline napi_value CreateJsNumber(napi_env env, int64_t value)
{
    napi_value result = nullptr;
    napi_create_int64(env, value, &result);
    return result;
}

inline napi_value CreateJsNumber(napi_env env, double value)
{
    napi_value result = nullptr;
    napi_create_double(env, value, &result);
    return result;
}

template<class T>
napi_value CreateJsValueImpl(napi_env env, const T &value)
{
    // 默认实现，这里可以抛出异常或返回一个错误值
    return nullptr;
}

// 特化版本，针对bool类型
template<>
napi_value CreateJsValueImpl<bool>(napi_env env, const bool &value)
        {
    napi_value result;
    napi_get_boolean(env, value, &result);
    return result;
}

// 特化版本，针对算术类型
template<typename T>
napi_value CreateJsValueImpl(napi_env env, const std::enable_if_t <std::is_arithmetic_v<T>, T> &value)
{
    return CreateJsNumber(env, value);
}

// 特化版本，针对std::string
template<>
napi_value CreateJsValueImpl<std::string>(napi_env env, const std::string &value)
{
    napi_value result;
    napi_create_string_utf8(env, value.c_str(), value.length(), &result);
    return result;
}

// 特化版本，针对枚举类型
template<typename T>
napi_value CreateJsValueImpl(napi_env env, const std::enable_if_t <std::is_enum_v<T>, T> &value)
{
    using UnderlyingType = typename std::underlying_type<T>::type;
    return CreateJsNumber(env, static_cast<UnderlyingType>(value));
}

// 特化版本，针对const char*
napi_value CreateJsValueImpl(napi_env env, const char *value)
{
    napi_value result;
    (value != nullptr) ? napi_create_string_utf8(env, value, strlen(value), &result) :
    napi_get_undefined(env, &result);
    return result;
}

template<class T>
inline napi_value CreateJsValue(napi_env env, const T& value)
{
    return CreateJsValueImpl(env, value);
}

napi_value CanIUse(napi_env env, napi_callback_info info)
{
    if (env == nullptr || info == nullptr) {
        return nullptr;
    }
    napi_value undefined = CreateJsUndefined(env);

    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc != 1) {
        return undefined;
    }

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[0], &valueType);
    if (valueType != napi_string) {
        return undefined;
    }

    char syscap[SYSCAP_MAX_SIZE] = {0};

    size_t strLen = 0;
    napi_get_value_string_utf8(env, argv[0], syscap, sizeof(syscap), &strLen);

    bool ret = HasSystemCapability(syscap);
    return CreateJsValue(env, ret);
}

void InitSyscapModule(napi_env env, napi_value globalObject)
{
    const char *moduleName = "JsRuntime";
    BindNativeFunction(env, globalObject, "canIUse", moduleName, CanIUse);
}