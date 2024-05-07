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

#include "parameter.h"
#include "param_wrapper.h"
#include "sysparam_errno.h"
#include "system_parameter_ffi.h"

namespace OHOS {
namespace Parameter {

const int PARA_FAILURE = -1;
const int PARA_SUCCESS = 0;
static constexpr int MAX_VALUE_LENGTH = PARAM_CONST_VALUE_LEN_MAX;

extern "C" {
    char* MallocCString(const char* origin)
    {
        if (origin == nullptr) {
            return nullptr;
        }
        auto len = strlen(origin) + 1;
        char* res = static_cast<char*>(malloc(sizeof(char) * len));
        if (res == nullptr) {
            return nullptr;
        }
        return std::char_traits<char>::copy(res, origin, len);
    }

    static int GetErrorInfo(int status)
    {
        switch (status) {
            case EC_FAILURE:
            case EC_SYSTEM_ERR:
            case SYSPARAM_SYSTEM_ERROR:
                return -SYSPARAM_SYSTEM_ERROR;
            case EC_INVALID:
            case SYSPARAM_INVALID_INPUT:
                return -SYSPARAM_INVALID_INPUT;
            case SYSPARAM_PERMISSION_DENIED:
                return -SYSPARAM_PERMISSION_DENIED;
            case SYSPARAM_NOT_FOUND:
                return -SYSPARAM_NOT_FOUND;
            case SYSPARAM_INVALID_VALUE:
                return -SYSPARAM_INVALID_VALUE;
            default:
                return -SYSPARAM_SYSTEM_ERROR;
        }
        return 0;
    }

    RetDataCString FfiOHOSSysTemParameterGet(const char* key, const char* def)
    {
        RetDataCString ret = {.code = PARA_FAILURE, .data = nullptr};
        std::vector<char> value(MAX_VALUE_LENGTH, 0);
        int status = GetParameter(key, (strlen(def) == 0) ? nullptr : def, value.data(), MAX_VALUE_LENGTH);
        if (status < PARA_SUCCESS) {
            ret.code = GetErrorInfo(status);
        } else {
            ret.code = PARA_SUCCESS;
            ret.data = MallocCString(value.data());
        }
        return ret;
    }

    int32_t FfiOHOSSysTemParameterSet(const char* key, const char* value)
    {
        int32_t ret = SetParameter(key, value);
        if (ret != 0) {
            return GetErrorInfo(ret);
        }
        return ret;
    }
}
}
}