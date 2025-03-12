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
#include "parameters.h"

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

static ani_string getSync([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object,
                          ani_string param, ani_string def)
{
    std::string key = ani2std(env, param);
    std::string defValue = "";
    if (def != nullptr) {
        defValue = ani2std(env, def);
    }
    std::string value = OHOS::system::GetParameter(key, defValue);
    ani_string ret;
    env->String_NewUTF8(value.c_str(), value.size(), &ret);
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

    ani_class ns;
    static const char *className = "L@ohos/systemParameterEnhance/systemParameterEnhance;";
    if (ANI_OK != env->FindClass(className, &ns)) {
        PARAM_JS_LOGE("not found class %s", className);
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"getSync", "Lstd/core/String;Lstd/core/String;:Lstd/core/String;",
                             reinterpret_cast<void *>(getSync)},
    };

    if (ANI_OK != env->Class_BindNativeMethods(ns, methods.data(), methods.size())) {
        PARAM_JS_LOGE("Cannot bind native methods to %s", className);
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
