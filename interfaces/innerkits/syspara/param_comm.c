/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "param_comm.h"

#include <stdlib.h>
#include <string.h>

#include "init_param.h"
#include "parameter.h"
#include "sysparam_errno.h"
#include "securec.h"
#include "beget_ext.h"

INIT_LOCAL_API int GetSystemError(int err)
{
    switch (err) {
        case 0:
            return 0;
        case PARAM_CODE_INVALID_PARAM:
        case PARAM_CODE_INVALID_NAME:
        case PARAM_CODE_READ_ONLY:
        case PARAM_WORKSPACE_NOT_INIT:
        case PARAM_WATCHER_CALLBACK_EXIST:
        case PARAM_WATCHER_GET_SERVICE_FAILED:
            return EC_INVALID;
        case PARAM_CODE_INVALID_VALUE:
            return SYSPARAM_INVALID_VALUE;
        case PARAM_CODE_NOT_FOUND:
        case PARAM_CODE_NODE_EXIST:
            return SYSPARAM_NOT_FOUND;
        case DAC_RESULT_FORBIDED:
        case SELINUX_RESULT_FORBIDED:
            return SYSPARAM_PERMISSION_DENIED;
        case PARAM_CODE_REACHED_MAX:
        case PARAM_CODE_FAIL_CONNECT:
        case PARAM_CODE_NOT_SUPPORT:
        case PARAM_CODE_IPC_ERROR:
        case PARAM_CODE_MEMORY_MAP_FAILED:
        case PARAM_CODE_MEMORY_NOT_ENOUGH:
            return SYSPARAM_SYSTEM_ERROR;
        case PARAM_CODE_TIMEOUT:
            return SYSPARAM_WAIT_TIMEOUT;
        default:
            return SYSPARAM_SYSTEM_ERROR;
    }
}

INIT_LOCAL_API int IsValidParamValue(const char *value, uint32_t len)
{
    if ((value == NULL) || (strlen(value) + 1 > len)) {
        return 0;
    }
    return 1;
}

INIT_LOCAL_API int GetParameter_(const char *key, const char *def, char *value, uint32_t len)
{
    if ((key == NULL) || (value == NULL) || (len > (uint32_t)PARAM_BUFFER_MAX)) {
        return EC_INVALID;
    }
    uint32_t size = len;
    int ret = SystemGetParameter(key, NULL, &size);
    if (ret != 0) {
        if (def == NULL) {
            return GetSystemError(ret);
        }
        if (strlen(def) > len) {
            return EC_INVALID;
        }
        ret = strcpy_s(value, len, def);
        return (ret == 0) ? 0 : EC_FAILURE;
    } else if (size > len) {
        return EC_INVALID;
    }

    size = len;
    ret = SystemGetParameter(key, value, &size);
    BEGET_CHECK_ONLY_ELOG(ret == 0, "GetParameter_ failed! the errNum is: %d", ret);
    return GetSystemError(ret);
}

static PropertyValueProcessor g_propertyGetProcessor = NULL;

INIT_LOCAL_API const char *GetProperty(const char *key, const char **paramHolder)
{
    BEGET_CHECK(paramHolder != NULL, return NULL);
    if (*paramHolder != NULL) {
        return *paramHolder;
    }
    uint32_t len = 0;
    int ret = SystemGetParameter(key, NULL, &len);
    if (ret == 0 && len > 0) {
        char *res = (char *)malloc(len + 1);
        BEGET_CHECK(res != NULL, return NULL);
        ret = SystemGetParameter(key, res, &len);
        if (ret != 0) {
            free(res);
            return NULL;
        }
        if (g_propertyGetProcessor != NULL) {
            res = g_propertyGetProcessor(key, res);
        }
        *paramHolder = res;
    }
    return *paramHolder;
}

INIT_LOCAL_API PropertyValueProcessor SetPropertyGetProcessor(PropertyValueProcessor processor)
{
    PropertyValueProcessor prev = g_propertyGetProcessor;
    g_propertyGetProcessor = processor;
    return prev;
}

INIT_LOCAL_API const char *GetProductModel_(void)
{
    static const char *productModel = NULL;
    return GetProperty("const.product.model", &productModel);
}

INIT_LOCAL_API const char *GetManufacture_(void)
{
    static const char *productManufacture = NULL;
    return GetProperty("const.product.manufacturer", &productManufacture);
}

INIT_LOCAL_API const char *GetFullName_(void)
{
    static const char *fillname = NULL;
    return GetProperty("const.ohos.fullname", &fillname);
}
