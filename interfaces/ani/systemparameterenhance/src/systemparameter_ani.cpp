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

#include <ani.h>
#include <array>
#include <iostream>

#include "beget_ext.h"
#include "parameter.h"

static constexpr int MAX_VALUE_LENGTH = PARAM_CONST_VALUE_LEN_MAX;

#define PARAM_JS_DOMAIN (BASE_DOMAIN + 0xc)
#define PARAM_JS_LOGI(fmt, ...) STARTUP_LOGI(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)
#define PARAM_JS_LOGE(fmt, ...) STARTUP_LOGE(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)
#define PARAM_JS_LOGV(fmt, ...) STARTUP_LOGV(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)
#define PARAM_JS_LOGW(fmt, ...) STARTUP_LOGW(PARAM_JS_DOMAIN, "PARAM_JS", fmt, ##__VA_ARGS__)

static std::string ani2std(ani_env *env, const ani_string str)
{
    ani_size strSize;
    env->String_GetUTF8Size(str, &strSize);
   
    std::vector<char> tmp_str(strSize + 1);
    char *buffer = tmp_str.data();

    ani_size str_bytes_written = 0;
    env->String_GetUTF8(str, buffer, strSize + 1, &str_bytes_written);
    
    buffer[str_bytes_written] = '\0';
    return std::string(buffer);
}

static void ThrowParameterErr(ani_env *env, int errCode)
{
    static const char *errorClsName = "L@ohos/systemParameterEnhance/ParameterError;";
    ani_class cls {};
    if (env->FindClass(errorClsName, &cls) != ANI_OK) {
        PARAM_JS_LOGE("find class ParameterError failed");
        return;
    }
    ani_method ctor;
    if (env->Class_FindMethod(cls, "<ctor>", nullptr, &ctor) != ANI_OK) {
        PARAM_JS_LOGE("find method ParameterError.constructor failed");
        return;
    }
    ani_object errorObject;
    if (env->Object_New(cls, ctor, &errorObject, errCode) != ANI_OK) {
        PARAM_JS_LOGE("create AccessibilityError object failed");
        return;
    }
    env->ThrowError(static_cast<ani_error>(errorObject));
    return;
}

static ani_string getSync([[maybe_unused]] ani_env *env, ani_string param, ani_string def)
{
    std::string key = ani2std(env, param);
    std::string defValue = "";
    if (def != nullptr) {
        defValue = ani2std(env, def);
    }
    std::vector<char> value(MAX_VALUE_LENGTH, 0);
    int result = GetParameter(key.c_str(), defValue.c_str(), value.data(), MAX_VALUE_LENGTH);
    if (result < 0) {
        PARAM_JS_LOGI("getSync faile, key:%s, ret:%d", key.c_str(), result);
        ThrowParameterErr(env, result);
        return nullptr;
    }
    ani_string ret;
    env->String_NewUTF8(value.data(), strlen(value.data()), &ret);
    return ret;
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    PARAM_JS_LOGI("Enter systemparameter ANI_Constructor");
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        PARAM_JS_LOGE("Unsupported ANI_VERSION_1");
        return ANI_ERROR;
    }

    ani_namespace ns;
    static const char *namespaceName = "L@ohos/systemParameterEnhance/systemParameterEnhance;";
    if (ANI_OK != env->FindNamespace(namespaceName, &ns)) {
        PARAM_JS_LOGE("not found namespace %s", namespaceName);
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"getSync", "Lstd/core/String;Lstd/core/String;:Lstd/core/String;",
                             reinterpret_cast<void *>(getSync)},
    };

    if (ANI_OK != env->Namespace_BindNativeFunctions(ns, methods.data(), methods.size())) {
        PARAM_JS_LOGE("Cannot bind native methods to %s", namespaceName);
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
