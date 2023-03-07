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
#include "native_parameters_js.h"
#include "sysparam_errno.h"

static constexpr int ARGC_NUMBER = 2;
static constexpr int ARGC_THREE_NUMBER = 3;
static constexpr int MAX_NAME_LENGTH = 128;
static constexpr int MAX_VALUE_LENGTH = PARAM_CONST_VALUE_LEN_MAX;

using StorageAsyncContext = struct StorageAsyncContext {
    napi_env env = nullptr;
    napi_async_work work = nullptr;

    char key[MAX_NAME_LENGTH] = { 0 };
    size_t keyLen = 0;
    char value[MAX_VALUE_LENGTH] = { 0 };
    size_t valueLen = 0;
    napi_deferred deferred = nullptr;
    napi_ref callbackRef = nullptr;

    int status = -1;
    std::string getValue;
};

using StorageAsyncContextPtr = StorageAsyncContext *;

static int GetErrorInfo(int status, std::string &errMsg)
{
    switch (status) {
        case EC_FAILURE:
        case EC_SYSTEM_ERR:
        case SYSPARAM_SYSTEM_ERROR:
            errMsg = "System internal error including out of memory, deadlock etc";
            return -SYSPARAM_SYSTEM_ERROR;
        case EC_INVALID:
        case SYSPARAM_INVALID_INPUT:
            errMsg = "Input parameter is missing or invalid";
            return -SYSPARAM_INVALID_INPUT;
        case SYSPARAM_PERMISSION_DENIED:
            errMsg = "System permission operation permission denied";
            return -SYSPARAM_PERMISSION_DENIED;
        case SYSPARAM_NOT_FOUND:
            errMsg = "System parameter can not be found";
            return -SYSPARAM_NOT_FOUND;
        case SYSPARAM_INVALID_VALUE:
            errMsg = "System parameter value is invalid";
            return -SYSPARAM_INVALID_VALUE;
        default:
            errMsg = "System internal error including out of memory, deadlock etc";
            return -SYSPARAM_SYSTEM_ERROR;
    }
    return 0;
}

static napi_value BusinessErrorCreate(napi_env env, int status)
{
    std::string errMsg = "";
    int ret = GetErrorInfo(status, errMsg);
    PARAM_JS_LOGV("BusinessErrorCreate status %d err %d msg: %s", status, ret, errMsg.c_str());
    napi_value code = nullptr;
    napi_create_int32(env, ret, &code);
    napi_value msg = nullptr;
    napi_create_string_utf8(env, errMsg.c_str(), NAPI_AUTO_LENGTH, &msg);
    napi_value businessError = nullptr;
    napi_create_error(env, nullptr, msg, &businessError);
    napi_set_named_property(env, businessError, "code", code);
    return businessError;
}

#define PARAM_NAPI_ASSERT(env, assertion, result, info)         \
    do {                                                        \
        if (!(assertion)) {                                     \
            napi_value d_err = BusinessErrorCreate(env, result);   \
            napi_throw(env, d_err);                             \
            return nullptr;                                     \
        }                                                       \
    } while (0)

static int GetParamString(napi_env env, napi_value arg, char *buffer, size_t maxBuff, size_t *keySize)
{
    (void)napi_get_value_string_utf8(env, arg, nullptr, maxBuff - 1, keySize);
    if (*keySize >= maxBuff || *keySize == 0) {
        return SYSPARAM_INVALID_INPUT;
    }
    (void)napi_get_value_string_utf8(env, arg, buffer, maxBuff - 1, keySize);
    return 0;
}

static void SetCallbackWork(napi_env env, StorageAsyncContextPtr asyncContext)
{
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSStartupSet", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            StorageAsyncContext *asyncContext = reinterpret_cast<StorageAsyncContext *>(data);
            asyncContext->status = SetParameter(asyncContext->key, asyncContext->value);
            PARAM_JS_LOGV("JSApp set status: %d, key: '%s', value: '%s'.",
                asyncContext->status, asyncContext->key, asyncContext->value);
        },
        [](napi_env env, napi_status status, void *data) {
            StorageAsyncContext *asyncContext = reinterpret_cast<StorageAsyncContext *>(data);
            napi_value result[ARGC_NUMBER] = { 0 };
            if (asyncContext->status == 0) {
                napi_get_undefined(env, &result[0]);
                napi_get_undefined(env, &result[1]);
            } else {
                result[0] = BusinessErrorCreate(env, asyncContext->status);
                napi_get_undefined(env, &result[1]);
            }

            if (asyncContext->deferred) {
                if (asyncContext->status == 0) {
                    napi_resolve_deferred(env, asyncContext->deferred, result[1]);
                } else {
                    napi_reject_deferred(env, asyncContext->deferred, result[0]);
                }
            } else {
                napi_value callback = nullptr;
                napi_value callResult = nullptr;
                napi_get_reference_value(env, asyncContext->callbackRef, &callback);
                napi_call_function(env, nullptr, callback, ARGC_NUMBER, result, &callResult);
                napi_delete_reference(env, asyncContext->callbackRef);
            }
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        reinterpret_cast<void *>(asyncContext), &asyncContext->work);
    napi_queue_async_work(env, asyncContext->work);
}

static napi_value Set(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE_NUMBER;
    napi_value argv[ARGC_THREE_NUMBER] = { nullptr };
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    PARAM_NAPI_ASSERT(env, argc >= ARGC_NUMBER, SYSPARAM_INVALID_INPUT, "requires 2 parameter");
    StorageAsyncContextPtr asyncContext = new StorageAsyncContext();
    asyncContext->env = env;
    for (size_t i = 0; i < argc; i++) {
        napi_valuetype valueType = napi_null;
        napi_typeof(env, argv[i], &valueType);
        int ret = 0;
        if (i == 0 && valueType == napi_string) {
            ret = GetParamString(env, argv[i], asyncContext->key, MAX_NAME_LENGTH, &asyncContext->keyLen);
        } else if (i == 1 && valueType == napi_string) {
            ret = GetParamString(env, argv[i], asyncContext->value, MAX_VALUE_LENGTH, &asyncContext->valueLen);
        } else if (i == ARGC_NUMBER && valueType == napi_function) {
            napi_create_reference(env, argv[i], 1, &asyncContext->callbackRef);
        } else {
            ret = SYSPARAM_INVALID_INPUT;
        }
        if (ret != 0) {
            delete asyncContext;
            ret = (i == 1) ? SYSPARAM_INVALID_VALUE : ret;
            napi_value err = BusinessErrorCreate(env, ret);
            napi_throw(env, err);
            return nullptr;
        }
    }
    PARAM_JS_LOGV("JSApp set key: %s(%d), value: %s(%d).",
        asyncContext->key, asyncContext->keyLen, asyncContext->value, asyncContext->valueLen);

    napi_value result = nullptr;
    if (asyncContext->callbackRef == nullptr) {
        napi_create_promise(env, &asyncContext->deferred, &result);
    } else {
        napi_get_undefined(env, &result);
    }

    SetCallbackWork(env, asyncContext);
    return result;
}

static napi_value SetSync(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_NUMBER;
    napi_value args[ARGC_NUMBER] = { nullptr };
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    PARAM_NAPI_ASSERT(env, argc == ARGC_NUMBER, SYSPARAM_INVALID_INPUT, "Wrong number of arguments");
    napi_valuetype valuetype0 = napi_null;
    NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));
    napi_valuetype valuetype1 = napi_null;
    NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));
    PARAM_NAPI_ASSERT(env, valuetype0 == napi_string && valuetype1 == napi_string,
        SYSPARAM_INVALID_INPUT, "Wrong argument type. string expected.");

    size_t keySize = 0;
    std::vector<char> keyBuf(MAX_NAME_LENGTH, 0);
    int ret = GetParamString(env, args[0], keyBuf.data(), MAX_NAME_LENGTH, &keySize);
    if (ret != 0) {
        napi_value err = BusinessErrorCreate(env, SYSPARAM_INVALID_INPUT);
        napi_throw(env, err);
        return nullptr;
    }
    std::vector<char> value(MAX_VALUE_LENGTH, 0);
    size_t valueSize = 0;
    ret = GetParamString(env, args[1], value.data(), MAX_VALUE_LENGTH, &valueSize);
    if (ret != 0) {
        napi_value err = BusinessErrorCreate(env, SYSPARAM_INVALID_VALUE);
        napi_throw(env, err);
        return nullptr;
    }
    ret = SetParameter(keyBuf.data(), value.data());
    PARAM_JS_LOGV("JSApp SetSync result:%d, key: '%s'.", ret, keyBuf.data());
    napi_value napiValue = nullptr;
    if (ret != 0) { // set failed
        napi_value err = BusinessErrorCreate(env, ret);
        napi_throw(env, err);
    } else {
        napi_get_undefined(env, &napiValue);
    }
    return napiValue;
}

static napi_value GetSync(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_NUMBER;
    napi_value args[ARGC_NUMBER] = { nullptr };
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    PARAM_NAPI_ASSERT(env, argc == 1 || argc == ARGC_NUMBER, SYSPARAM_INVALID_INPUT, "Wrong number of arguments");
    napi_valuetype valuetype0 = napi_null;
    NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));
    PARAM_NAPI_ASSERT(env, valuetype0 == napi_string,
        SYSPARAM_INVALID_INPUT, "Wrong argument type. Numbers expected.");
    if (argc == ARGC_NUMBER) {
        napi_valuetype valuetype1 = napi_null;
        NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));
        PARAM_NAPI_ASSERT(env, valuetype1 == napi_string,
            SYSPARAM_INVALID_INPUT, "Wrong argument type. string expected.");
    }

    size_t keySize = 0;
    std::vector<char> keyBuf(MAX_NAME_LENGTH, 0);
    int ret = GetParamString(env, args[0], keyBuf.data(), MAX_NAME_LENGTH, &keySize);
    if (ret != 0) {
        napi_value err = BusinessErrorCreate(env, SYSPARAM_INVALID_INPUT);
        napi_throw(env, err);
        return nullptr;
    }
    std::vector<char> defValue(MAX_VALUE_LENGTH, 0);
    size_t valueSize = 0;
    if (argc == ARGC_NUMBER) {
        ret = GetParamString(env, args[1], defValue.data(), MAX_VALUE_LENGTH, &valueSize);
        if (ret != 0) {
            napi_value err = BusinessErrorCreate(env, SYSPARAM_INVALID_INPUT);
            napi_throw(env, err);
            return nullptr;
        }
    }
    std::vector<char> value(MAX_VALUE_LENGTH, 0);
    ret = GetParameter(keyBuf.data(), (valueSize == 0) ? nullptr : defValue.data(), value.data(), MAX_VALUE_LENGTH);
    PARAM_JS_LOGV("JSApp get status: %d, key: '%s', value: '%s', defValue: '%s'.",
        ret, keyBuf.data(), value.data(), defValue.data());
    if (ret < 0) {
        napi_value err = BusinessErrorCreate(env, ret);
        napi_throw(env, err);
        return nullptr;
    }
    napi_value napiValue = nullptr;
    NAPI_CALL(env, napi_create_string_utf8(env, value.data(), strlen(value.data()), &napiValue));
    return napiValue;
}

static void GetCallbackWork(napi_env env, StorageAsyncContextPtr asyncContext)
{
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSStartupGet", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            StorageAsyncContext *asyncContext = reinterpret_cast<StorageAsyncContext *>(data);
            std::vector<char> value(MAX_VALUE_LENGTH, 0);
            asyncContext->status = GetParameter(asyncContext->key,
                (asyncContext->valueLen == 0) ? nullptr : asyncContext->value, value.data(), MAX_VALUE_LENGTH);
            asyncContext->getValue = std::string(value.begin(), value.end());
            PARAM_JS_LOGV("JSApp get asyncContext status: %d, key: '%s', value: '%s', defValue: '%s'.",
                asyncContext->status, asyncContext->key, asyncContext->getValue.c_str(), asyncContext->value);
        },
        [](napi_env env, napi_status status, void *data) {
            StorageAsyncContext *asyncContext = reinterpret_cast<StorageAsyncContext *>(data);
            napi_value result[ARGC_NUMBER] = { 0 };
            if (asyncContext->status > 0) {
                napi_get_undefined(env, &result[0]);
                napi_create_string_utf8(env,
                    asyncContext->getValue.c_str(), strlen(asyncContext->getValue.c_str()), &result[1]);
            } else {
                result[0] = BusinessErrorCreate(env, asyncContext->status);
                napi_get_undefined(env, &result[1]);
            }

            if (asyncContext->deferred) {
                if (asyncContext->status > 0) {
                    napi_resolve_deferred(env, asyncContext->deferred, result[1]);
                } else {
                    napi_reject_deferred(env, asyncContext->deferred, result[0]);
                }
            } else {
                napi_value callback = nullptr;
                napi_value callResult = nullptr;
                napi_get_reference_value(env, asyncContext->callbackRef, &callback);
                napi_call_function(env, nullptr, callback, ARGC_NUMBER, result, &callResult);
                napi_delete_reference(env, asyncContext->callbackRef);
            }
            napi_delete_async_work(env, asyncContext->work);
            delete asyncContext;
        },
        reinterpret_cast<void *>(asyncContext), &asyncContext->work);
    napi_queue_async_work(env, asyncContext->work);
}

static napi_value Get(napi_env env, napi_callback_info info)
{
    size_t argc = ARGC_THREE_NUMBER;
    napi_value argv[ARGC_THREE_NUMBER] = { nullptr };
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    PARAM_NAPI_ASSERT(env, argc >= 1, SYSPARAM_INVALID_INPUT, "requires 1 parameter");
    StorageAsyncContextPtr asyncContext = new StorageAsyncContext();
    asyncContext->env = env;
    for (size_t i = 0; i < argc; i++) {
        napi_valuetype valueType = napi_null;
        napi_typeof(env, argv[i], &valueType);

        int ret = 0;
        if (i == 0 && valueType == napi_string) {
            ret = GetParamString(env, argv[i], asyncContext->key, MAX_NAME_LENGTH, &asyncContext->keyLen);
        } else if (i == 1 && valueType == napi_string) {
            ret = GetParamString(env, argv[i], asyncContext->value, MAX_VALUE_LENGTH, &asyncContext->valueLen);
        } else if (i == 1 && valueType == napi_function) {
            napi_create_reference(env, argv[i], 1, &asyncContext->callbackRef);
            break;
        } else if (i == ARGC_NUMBER && valueType == napi_function) {
            napi_create_reference(env, argv[i], 1, &asyncContext->callbackRef);
        } else {
            ret = SYSPARAM_INVALID_INPUT;
        }
        if (ret != 0) {
            delete asyncContext;
            ret = (i == 1) ? SYSPARAM_INVALID_VALUE : ret;
            napi_value err = BusinessErrorCreate(env, ret);
            napi_throw(env, err);
            return nullptr;
        }
    }
    PARAM_JS_LOGV("JSApp Get key: '%s'(%d), def value: '%s'(%d).",
        asyncContext->key, asyncContext->keyLen, asyncContext->value, asyncContext->valueLen);

    napi_value result = nullptr;
    if (asyncContext->callbackRef == nullptr) {
        napi_create_promise(env, &asyncContext->deferred, &result);
    } else {
        napi_get_undefined(env, &result);
    }

    GetCallbackWork(env, asyncContext);
    return result;
}

EXTERN_C_START
/*
 * Module init
 */
static napi_value Init(napi_env env, napi_value exports)
{
    /*
     * Attribute definition
     */
    const napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("set", Set),
        DECLARE_NAPI_FUNCTION("setSync", SetSync),
        DECLARE_NAPI_FUNCTION("get", Get),
        DECLARE_NAPI_FUNCTION("getSync", GetSync),
#ifdef PARAM_SUPPORT_WAIT
        DECLARE_NAPI_FUNCTION("wait", ParamWait),
        DECLARE_NAPI_FUNCTION("getWatcher", GetWatcher)
#endif
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(napi_property_descriptor), desc));
#ifdef PARAM_SUPPORT_WAIT
    return RegisterWatcher(env, exports);
#else
    return exports;
#endif
}
EXTERN_C_END

/*
 * Module definition
 */
static napi_module _module_old = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = Init,
    .nm_modname = "systemParameterV9",
    .nm_priv = ((void *)0),
    .reserved = { 0 }
};

static napi_module _module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = Init,
    .nm_modname = "systemParameterEnhance",
    .nm_priv = ((void *)0),
    .reserved = { 0 }
};

/*
 * Module registration function
 */
extern "C" __attribute__((constructor)) void RegisterModule(void)
{
    napi_module_register(&_module);
    napi_module_register(&_module_old);
}
