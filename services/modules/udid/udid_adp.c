/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "udid.h"
#ifndef OHOS_LITE
#include "init_module_engine.h"
#endif
#include "init_param.h"
#include "param_comm.h"
#include "securec.h"
#include "sysparam_errno.h"

#include "mbedtls/sha256.h"

#define BUF_MAX_LEN 132
#define PARAM_VALUE_MAX_LEN 36

INIT_UDID_LOCAL_API int GetSha256Value(const char *input, char *udid, uint32_t udidSize)
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

INIT_UDID_LOCAL_API int CalcDevUdid(char *udid, uint32_t size)
{
    char tmp[BUF_MAX_LEN] = {0};
    uint32_t manufactureLen = PARAM_VALUE_MAX_LEN;
    int ret = SystemReadParam("const.product.manufacturer", tmp, &manufactureLen);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Read param const.product.manufacturer failed!");

    uint32_t modelLen = PARAM_VALUE_MAX_LEN;
    ret = SystemReadParam("const.product.model", tmp + manufactureLen, &modelLen);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Read param const.product.model failed!");
    const char *serial = GetSerial_();
    BEGET_ERROR_CHECK(serial != NULL, return -1, "Read param serial failed!");
    ret = strcat_s(tmp, BUF_MAX_LEN, serial);
    BEGET_ERROR_CHECK(ret != -1, return -1, "Cat serial failed!");
    ret = GetSha256Value(tmp, udid, size);
    return ret;
}

#ifndef OHOS_LITE
INIT_UDID_LOCAL_API void SetDevUdid()
{
    BEGET_LOGI("Begin calculate udid");
    char udid[UDID_LEN] = {0};
    uint32_t size = (uint32_t)sizeof(udid);
    int ret = GetUdidFromParam(udid, size);
    if (ret != 0) {
        ret = CalcDevUdid(udid, size);
        BEGET_ERROR_CHECK(ret == 0, return, "calculate udid is failed!")
    }
    ret = SystemWriteParam("const.product.devUdid", udid);
    BEGET_ERROR_CHECK(ret == 0, return, "write param const.product.devUdid failed!");
}

MODULE_CONSTRUCTOR(void)
{
    SetDevUdid();
}
#endif
