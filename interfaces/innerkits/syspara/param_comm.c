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
#ifdef LITEOS_SUPPORT
#include "hal_sys_param.h"
#endif
#include "parameter.h"
#include "sysparam_errno.h"
#ifdef USE_MBEDTLS
#include "mbedtls/sha256.h"
#endif

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
            return EC_INVALID;
        case PARAM_CODE_INVALID_VALUE:
            return SYSPARAM_INVALID_VALUE;
        case PARAM_CODE_NOT_FOUND:
        case PARAM_CODE_NODE_EXIST:
            return SYSPARAM_NOT_FOUND;
        case DAC_RESULT_FORBIDED:
            return SYSPARAM_PERMISSION_DENIED;
        case PARAM_CODE_REACHED_MAX:
        case PARAM_CODE_FAIL_CONNECT:
        case PARAM_CODE_INVALID_SOCKET:
        case PARAM_CODE_NOT_SUPPORT:
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
    return GetSystemError(ret);
}

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
        *paramHolder = res;
    }
    return *paramHolder;
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

#ifdef USE_MBEDTLS
static int GetSha256Value(const char *input, char *udid, int udidSize)
{
    if (input == NULL) {
        return EC_FAILURE;
    }
    char buf[DEV_BUF_LENGTH] = { 0 };
    unsigned char hash[HASH_LENGTH] = { 0 };

    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    mbedtls_sha256_update(&context, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish(&context, hash);

    for (size_t i = 0; i < HASH_LENGTH; i++) {
        unsigned char value = hash[i];
        memset_s(buf, DEV_BUF_LENGTH, 0, DEV_BUF_LENGTH);
        int len = sprintf_s(buf, sizeof(buf), "%02X", value);
        if (len > 0 && strcat_s(udid, udidSize, buf) != 0) {
            return EC_FAILURE;
        }
    }
    return EC_SUCCESS;
}
#else
static int GetSha256Value(const char *input, char *udid, int udidSize)
{
    (void)input;
    (void)udid;
    (void)udidSize;
    return EC_FAILURE;
}
#endif

INIT_LOCAL_API const char *GetSerial_(void)
{
#ifdef LITEOS_SUPPORT
    return HalGetSerial();
#else
    static char *ohosSerial = NULL;
    if (ohosSerial == NULL) {
        BEGET_CHECK((ohosSerial = (char *)calloc(1, PARAM_VALUE_LEN_MAX)) != NULL, return NULL);
    }
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemGetParameter("ohos.boot.sn", ohosSerial, &len);
    BEGET_CHECK(ret == 0, return NULL);
    return ohosSerial;
#endif
}

INIT_LOCAL_API int GetDevUdid_(char *udid, int size)
{
    if (size < UDID_LEN || udid == NULL) {
        return EC_FAILURE;
    }

    uint32_t len = (uint32_t)size;
    int ret = SystemGetParameter("const.product.udid", udid, &len);
    BEGET_CHECK(ret != 0, return ret);

    const char *manufacture = GetManufacture_();
    const char *model = GetProductModel_();
    const char *sn = GetSerial_();
    if (manufacture == NULL || model == NULL || sn == NULL) {
        return -1;
    }
    int tmpSize = strlen(manufacture) + strlen(model) + strlen(sn) + 1;
    if (tmpSize <= 1 || tmpSize > DEV_BUF_MAX_LENGTH) {
        return -1;
    }
    char *tmp = NULL;
    BEGET_CHECK((tmp = (char *)malloc(tmpSize)) != NULL, return -1);

    (void)memset_s(tmp, tmpSize, 0, tmpSize);
    if ((strcat_s(tmp, tmpSize, manufacture) != 0) || (strcat_s(tmp, tmpSize, model) != 0) ||
        (strcat_s(tmp, tmpSize, sn) != 0)) {
        free(tmp);
        return -1;
    }

    ret = GetSha256Value(tmp, udid, size);
    free(tmp);
    return ret;
}

INIT_LOCAL_API const char *GetFullName_(void)
{
    static const char *fillname = NULL;
    return GetProperty("const.ohos.fullname", &fillname);
}
