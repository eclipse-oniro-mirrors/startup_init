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

#include "param_comm.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "init_param.h"
#include "parameter.h"
#include "sysparam_errno.h"
#ifdef USE_MBEDTLS
#include "mbedtls/sha256.h"
#elif !(defined OHOS_LITE)
#include "openssl/sha.h"
#endif
#include "securec.h"

static const char *g_emptyStr = "";

int HalGetParameter(const char *key, const char *def, char *value, uint32_t len)
{
    if ((key == NULL) || (value == NULL)) {
        return EC_INVALID;
    }
    uint32_t size = len;
    int ret = SystemGetParameter(key, NULL, &size);
    if ((size > len) || (ret != 0)) {
        return strcpy_s(value, len, def);
    }
    size = len;
    return (SystemGetParameter(key, value, &size) == 0) ? EC_SUCCESS : EC_FAILURE;
}

const char *GetProperty(const char *key, const char **paramHolder)
{
    if (paramHolder == NULL) {
        return NULL;
    }
    if (*paramHolder != NULL) {
        return *paramHolder;
    }
    uint32_t len = 0;
    int ret = SystemGetParameter(key, NULL, &len);
    if (ret == 0 && len > 0) {
        char *res = (char *)malloc(len + 1);
        if (res == NULL) {
            return g_emptyStr;
        }
        ret = SystemGetParameter(key, res, &len);
        if (ret != 0) {
            free(res);
            return g_emptyStr;
        }
        *paramHolder = res;
    }
    return *paramHolder;
}

int StringToLL(const char *str, long long int *out)
{
    const char* s = str;
    while (isspace(*s)) {
        s++;
    }

    size_t len = strlen(str);
    int positiveHex = (len > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    int negativeHex = (len > 2 && s[0] == '-' && s[1] == '0' && (s[2] == 'x' || s[2] == 'X')); // 2: shorttest
    int base = (positiveHex || negativeHex) ? HEX : DECIMAL;
    char* end = NULL;
    errno = 0;
    *out = strtoll(s, &end, base);
    if (errno != 0) {
        return -1;
    }
    if (s == end || *end != '\0') {
        return -1;
    }
    return 0;
}

int StringToULL(const char *str, unsigned long long int *out)
{
    const char* s = str;
    while (isspace(*s)) {
        s++;
    }

    if (s[0] == '-') {
        return -1;
    }

    int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? HEX : DECIMAL;
    char* end = NULL;
    errno = 0;
    *out = strtoull(s, &end, base);
    if (errno != 0) {
        return -1;
    }
    if (end == s) {
        return -1;
    }
    if (*end != '\0') {
        return -1;
    }
    return 0;
}

const char *GetProductModel_(void)
{
    static const char *productModel = NULL;
    return GetProperty("const.product.model", &productModel);
}

const char *GetManufacture_(void)
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
    mbedtls_sha256_starts_ret(&context, 0);
    mbedtls_sha256_update_ret(&context, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish_ret(&context, hash);

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
#elif !(defined OHOS_LITE)
static int GetSha256Value(const char *input, char *udid, int udidSize)
{
    char buf[DEV_BUF_LENGTH] = { 0 };
    unsigned char hash[SHA256_DIGEST_LENGTH] = { 0 };
    SHA256_CTX sha256;
    if ((SHA256_Init(&sha256) == 0) || (SHA256_Update(&sha256, input, strlen(input)) == 0) ||
        (SHA256_Final(hash, &sha256) == 0)) {
        return -1;
    }

    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        unsigned char value = hash[i];
        (void)memset_s(buf, DEV_BUF_LENGTH, 0, DEV_BUF_LENGTH);
        int len = sprintf_s(buf, sizeof(buf), "%02X", value);
        if (len > 0 && strcat_s(udid, udidSize, buf) != 0) {
            return -1;
        }
    }
    return 0;
}
#else
static int GetSha256Value(const char *input, char *udid, int udidSize)
{
    return EC_FAILURE;
}
#endif

const char *GetSerial_(void)
{
    const char *serialNumberss = NULL;
    GetProperty("ohos.boot.sn", &serialNumberss);
    return serialNumberss;
}

int GetDevUdid_(char *udid, int size)
{
    if (size < UDID_LEN || udid == NULL) {
        return EC_FAILURE;
    }
    const char *manufacture = GetManufacture_();
    const char *model = GetProductModel_();
    const char *sn = GetSerial_();
    if (manufacture == NULL || model == NULL || sn == NULL) {
        return -1;
    }
    int tmpSize = strlen(manufacture) + strlen(model) + strlen(sn) + 1;
    if (tmpSize <= 0 || tmpSize > DEV_BUF_MAX_LENGTH) {
        free((void *)sn);
        return -1;
    }
    char *tmp = (char *)malloc(tmpSize);
    if (tmp == NULL) {
        free((void *)sn);
        return -1;
    }

    (void)memset_s(tmp, tmpSize, 0, tmpSize);
    if ((strcat_s(tmp, tmpSize, manufacture) != 0) || (strcat_s(tmp, tmpSize, model) != 0) ||
        (strcat_s(tmp, tmpSize, sn) != 0)) {
        free(tmp);
        free((void *)sn);
        return -1;
    }

    int ret = GetSha256Value(tmp, udid, size);
    free(tmp);
    free((void *)sn);
    return ret;
}