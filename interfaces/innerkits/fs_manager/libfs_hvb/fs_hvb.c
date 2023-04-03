/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "fs_hvb.h"
#include "fs_dm.h"
#include "securec.h"
#include <stdint.h>
#include "beget_ext.h"
#include "init_utils.h"
#include <libhvb.h>
#include <libgen.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define FS_HVB_VERITY_TARGET_MAX 512
#define FS_HVB_BLANK_SPACE_ASCII 32
#define FS_HVB_SECTOR_BYTES 512
#define FS_HVB_UINT64_MAX_LENGTH 64
#define FS_HVB_HASH_ALG_STR_MAX 32

#define FS_HVB_DIGEST_SHA256_BYTES 32
#define FS_HVB_DIGEST_SHA512_BYTES 64
#define FS_HVB_DIGEST_MAX_BYTES FS_HVB_DIGEST_SHA512_BYTES
#define FS_HVB_DIGEST_STR_MAX_BYTES 128
#define FS_HVB_DEVPATH_MAX_LEN 128
#define FS_HVB_RVT_PARTITION_NAME "rvt"
#define FS_HVB_CMDLINE_PATH "/proc/cmdline"
#define FS_HVB_PARTITION_PREFIX "/dev/block/by-name/"

#define FS_HVB_RETURN_ERR_IF_NULL(__ptr)             \
    do {                                                \
        if ((__ptr) == NULL) {                          \
            BEGET_LOGE("error, %s is NULL\n", #__ptr); \
            return -1;                                  \
        }                                               \
    } while (0)

static struct hvb_verified_data *g_vd = NULL;

static char FsHvbBinToHexChar(char octet)
{
    return octet < 10 ? '0' + octet : 'a' + (octet - 10);
}

static char FsHvbHexCharToBin(char hex)
{
    return '0' <= hex && hex <= '9' ? hex - '0' : 10 + (hex - 'a');
}

static int FsHvbGetHashStr(char *str, size_t size)
{
    return FsHvbGetValueFromCmdLine(str, size, HVB_CMDLINE_HASH_ALG);
}

static int FsHvbGetCertDigstStr(char *str, size_t size)
{
    return FsHvbGetValueFromCmdLine(str, size, HVB_CMDLINE_CERT_DIGEST);
}

static int FsHvbComputeSha256(char *digest, size_t size, struct hvb_cert_data *certs, uint64_t num_certs)
{
    int ret;
    uint64_t n;
    struct hash_ctx_t ctx = {0};

    if (size < FS_HVB_DIGEST_SHA256_BYTES) {
        BEGET_LOGE("error, size=%zu", size);
        return -1;
    }

    ret = hash_ctx_init(&ctx, HASH_ALG_SHA256);
    if (ret != HASH_OK) {
        BEGET_LOGE("error 0x%x, sha256 init", ret);
        return -1;
    }

    for (n = 0; n < num_certs; n++) {
        ret = hash_calc_update(&ctx, certs[n].data.addr, certs[n].data.size);
        if (ret != HASH_OK) {
            BEGET_LOGE("error 0x%x, sha256 update", ret);
            return -1;
        }
    }

    ret = hash_calc_do_final(&ctx, NULL, 0, (uint8_t *)digest, size);
    if (ret != HASH_OK) {
        BEGET_LOGE("error 0x%x, sha256 final", ret);
        return -1;
    }

    return 0;
}

static int FsHvbHexStrToBin(char *bin, size_t bin_size, char *str, size_t str_size)
{
    size_t i;

    if ((str_size & 0x1) != 0 || bin_size < (str_size >> 1)) {
        BEGET_LOGE("error, bin_size %zu str_size %zu", bin_size, str_size);
        return -1;
    }

    for (i = 0; i < (str_size >> 1); i++) {
        bin[i] = (FsHvbHexCharToBin(str[2 * i]) << 4) | FsHvbHexCharToBin(str[2 * i + 1]);
    }

    return 0;
}

static int FsHvbCheckCertChainDigest(struct hvb_cert_data *certs, uint64_t num_certs)
{
    int ret;
    size_t digest_len = 0;
    char hashAlg[FS_HVB_HASH_ALG_STR_MAX] = {0};
    char certDigest[FS_HVB_DIGEST_MAX_BYTES] = {0};
    char computeDigest[FS_HVB_DIGEST_MAX_BYTES] = {0};
    char certDigestStr[FS_HVB_DIGEST_STR_MAX_BYTES + 1] = {0};

    ret = FsHvbGetHashStr(&hashAlg[0], sizeof(hashAlg));
    if (ret != 0) {
        return ret;
    }

    ret = FsHvbGetCertDigstStr(&certDigestStr[0], sizeof(certDigestStr));
    if (ret != 0) {
        return ret;
    }

    ret = FsHvbHexStrToBin(&certDigest[0], sizeof(certDigest), &certDigestStr[0], strlen(certDigestStr));
    if (ret != 0) {
        return ret;
    }

    if (strcmp(&hashAlg[0], "sha256") == 0) {
        digest_len = FS_HVB_DIGEST_SHA256_BYTES;
        ret = FsHvbComputeSha256(computeDigest, sizeof(computeDigest), certs, num_certs);
    } else {
        BEGET_LOGE("error, not support alg %s", &hashAlg[0]);
        return -1;
    }

    if (ret != 0) {
        BEGET_LOGE("error 0x%x, compute hash", ret);
        return -1;
    }

    ret = memcmp(&certDigest[0], &computeDigest[0], digest_len);
    if (ret != 0) {
        BEGET_LOGE("error, cert digest not match with cmdline");
        return -1;
    }

    return 0;
}

int FsHvbInit(void)
{
    enum hvb_errno rc;

    // return ok if the hvb is already initialized
    if (g_vd != NULL) {
        BEGET_LOGI("Hvb has already been inited");
        return 0;
    }

    rc = hvb_chain_verify(FsHvbGetOps(), FS_HVB_RVT_PARTITION_NAME, NULL, &g_vd);
    if (g_vd == NULL) {
        BEGET_LOGE("error 0x%x, hvb chain verify, vd is NULL", rc);
        return rc;
    }

    if (rc != HVB_OK) {
        BEGET_LOGE("error 0x%x, hvb chain verify", rc);
        hvb_chain_verify_data_free(g_vd);
        g_vd = NULL;
        return rc;
    }

    rc = FsHvbCheckCertChainDigest(g_vd->certs, g_vd->num_loaded_certs);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, cert chain hash", rc);
        hvb_chain_verify_data_free(g_vd);
        g_vd = NULL;
        return rc;
    }

    return 0;
}

static int FsHvbGetCert(struct hvb_cert *cert, char *devName, struct hvb_verified_data *vd)
{
    enum hvb_errno hr;
    size_t devNameLen = strlen(devName);
    struct hvb_cert_data *p = vd->certs;
    struct hvb_cert_data *end = p + vd->num_loaded_certs;

    for (; p < end; p++) {
        if (devNameLen != strlen(p->partition_name)) {
            continue;
        }

        if (memcmp(p->partition_name, devName, devNameLen) == 0) {
            break;
        }
    }

    if (p == end) {
        BEGET_LOGE("error, can't found %s partition", devName);
        return -1;
    }

    hr = hvb_cert_parser(cert, &p->data);
    if (hr != HVB_OK) {
        BEGET_LOGE("error 0x%x, parser hvb cert", hr);
        return -1;
    }

    return 0;
}

static int FsHvbVerityTargetAppendString(char **p, char *end, char *str, size_t len)
{
    errno_t err;

    // check range for append string
    if (*p + len >= end || *p + len < *p) {
        BEGET_LOGE("error, append string overflow");
        return -1;
    }
    // append string
    err = memcpy_s(*p, end - *p, str, len);
    if (err != EOK) {
        BEGET_LOGE("error 0x%x, cp string fail", err);
        return -1;
    }
    *p += len;

    // check range for append blank space
    if (*p + 1 >= end || *p + 1 < *p) {
        BEGET_LOGE("error, append blank space overflow");
        return -1;
    }
    // append blank space
    **p = FS_HVB_BLANK_SPACE_ASCII;
    *p += 1;

    BEGET_LOGE("append string %s", str);

    return 0;
}

static int FsHvbVerityTargetAppendOctets(char **p, char *end, char *octs, size_t octs_len)
{
    size_t i;
    int rc;
    char *str = NULL;
    size_t str_len = octs_len * 2;

    str = calloc(1, str_len + 1);
    if (str == NULL) {
        BEGET_LOGE("error, calloc str fail");
        return -1;
    }

    for (i = 0; i < octs_len; i++) {
        str[2 * i] = FsHvbBinToHexChar((octs[i] >> 4) & 0xf);
        str[2 * i + 1] = FsHvbBinToHexChar(octs[i] & 0xf);
    }

    rc = FsHvbVerityTargetAppendString(p, end, str, str_len);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, append str fail", rc);
    }
    free(str);

    return rc;
}

static int FsHvbVerityTargetAppendNum(char **p, char *end, uint64_t num)
{
    int rc;
    char num_str[FS_HVB_UINT64_MAX_LENGTH] = {0};

    rc = sprintf_s(&num_str[0], sizeof(num_str), "%llu", num);
    if (rc < 0) {
        BEGET_LOGE("error 0x%x, calloc num_str", rc);
        return rc;
    }

    rc = FsHvbVerityTargetAppendString(p, end, num_str, strlen(&num_str[0]));
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, append num_str fail", rc);
    }

    return rc;
}

#define RETURN_ERR_IF_APPEND_STRING_ERR(p, end, str, str_len)                     \
    do {                                                                          \
        int __ret = FsHvbVerityTargetAppendString(p, end, str, str_len);          \
        if (__ret != 0)                                                           \
            return __ret;                                                         \
    } while (0)

#define RETURN_ERR_IF_APPEND_OCTETS_ERR(p, end, octs, octs_len)                    \
    do {                                                                          \
        int __ret = FsHvbVerityTargetAppendOctets(p, end, octs, octs_len);          \
        if (__ret != 0)                                                           \
            return __ret;                                                         \
    } while (0)

#define RETURN_ERR_IF_APPEND_DIGIT_ERR(p, end, num)                               \
    do {                                                                          \
        int __ret = FsHvbVerityTargetAppendNum(p, end, num);                      \
        if (__ret != 0)                                                           \
            return __ret;                                                         \
    } while (0)

static char *FsHvbGetHashAlgStr(unsigned int hash_algo)
{
    char *alg = NULL;

    switch (hash_algo) {
        case 0:
            alg = "sha256";
            break;

        case 1:
            alg = "sha512";
            break;

        default:
            alg = NULL;
            break;
    }

    return alg;
}

/*
 * target->paras is verity table target, format as below;
 * <version> <dev><hash_dev><data_block_size><hash_block_size>
 * <num_data_blocks><hash_start_block><algorithm><digest><salt>
 *[<#opt_params><opt_params>]
 *exp: 1 /dev/sda1 /dev/sda1 4096 4096 262144 262144 sha256 \
       xxxxx
       xxxxx
 */

static int FsHvbConstructVerityTarget(DmVerityTarget *target, char *devName, struct hvb_cert *cert)
{
    char *p = NULL;
    char *end = NULL;
    char *hashALgo = NULL;
    char devPath[FS_HVB_DEVPATH_MAX_LEN] = {0};

    target->start = 0;
    target->length = cert->image_len / FS_HVB_SECTOR_BYTES;
    target->paras = calloc(1, FS_HVB_VERITY_TARGET_MAX); // simple it, just calloc a big mem
    if (target->paras == NULL) {
        BEGET_LOGE("error, alloc target paras");
        return -1;
    }

    if (snprintf_s(&devPath[0], sizeof(devPath), sizeof(devPath) - 1, "%s%s", FS_HVB_PARTITION_PREFIX, devName) == -1) {
        BEGET_LOGE("error, snprintf_s devPath");
        return -1;
    }

    BEGET_LOGE("puck devPath=%s", &devPath[0]);
    p = target->paras;
    end = p + FS_HVB_VERITY_TARGET_MAX;

    // append <version>
    RETURN_ERR_IF_APPEND_DIGIT_ERR(&p, end, 1);
    // append <dev>
    RETURN_ERR_IF_APPEND_STRING_ERR(&p, end, &devPath[0], strlen(devPath));
    // append <hash_dev>
    RETURN_ERR_IF_APPEND_STRING_ERR(&p, end, &devPath[0], strlen(devPath));
    // append <data_block_size>
    RETURN_ERR_IF_APPEND_DIGIT_ERR(&p, end, cert->data_block_size);
    // append <hash_block_size>
    RETURN_ERR_IF_APPEND_DIGIT_ERR(&p, end, cert->hash_block_size);
    // append <num_data_blocks>
    RETURN_ERR_IF_APPEND_DIGIT_ERR(&p, end, cert->image_len / cert->data_block_size);
    // append <hash_start_block>
    RETURN_ERR_IF_APPEND_DIGIT_ERR(&p, end, cert->hashtree_offset / cert->hash_block_size);

    // append <algorithm>
    hashALgo = FsHvbGetHashAlgStr(cert->hash_algo);
    if (hashALgo == NULL) {
        BEGET_LOGE("error, hash alg %d is invalid", cert->hash_algo);
        return -1;
    }
    RETURN_ERR_IF_APPEND_STRING_ERR(&p, end, hashALgo, strlen(hashALgo));

    // append <digest>
    RETURN_ERR_IF_APPEND_OCTETS_ERR(&p, end, (char *)cert->hash_payload.digest, cert->digest_size);

    // append <salt>
    RETURN_ERR_IF_APPEND_OCTETS_ERR(&p, end, (char *)cert->hash_payload.salt, cert->salt_size);

    //remove last blank
    *(p - 1) = '\0';

    target->paras_len = strlen(target->paras);

    return 0;
}

static int FsHvbCreateVerityTarget(DmVerityTarget *target, char *devName, struct hvb_verified_data *vd)
{
    int rc;
    struct hvb_cert cert = {0};

    rc = FsHvbGetCert(&cert, devName, vd);
    if (rc != 0) {
        return rc;
    }

    rc = FsHvbConstructVerityTarget(target, devName, &cert);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, can't construct verity target", rc);
        return rc;
    }

    return rc;
}

void FsHvbDestoryVerityTarget(DmVerityTarget *target)
{
    if (target != NULL && target->paras != NULL) {
        free(target->paras);
        target->paras = NULL;
    }
}

int FsHvbSetupHashtree(FstabItem *fsItem)
{
    int rc;
    DmVerityTarget target = {0};
    char *dmDevPath = NULL;
    char *devName = NULL;

    FS_HVB_RETURN_ERR_IF_NULL(fsItem);
    FS_HVB_RETURN_ERR_IF_NULL(g_vd);

    // fsItem->deviceName is like /dev/block/platform/xxx/by-name/system
    // we just basename system
    devName = basename(fsItem->deviceName);
    if (devName == NULL) {
        BEGET_LOGE("error, get basename");
        return -1;
    }

    rc = FsHvbCreateVerityTarget(&target, devName, g_vd);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, create verity-table", rc);
        goto exit;
    }

    rc = FsDmCreateDevice(&dmDevPath, devName, &target);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, create dm-verity", rc);
        goto exit;
    }

    rc = FsDmInitDmDev(dmDevPath);
    if (rc != 0) {
        BEGET_LOGE("error 0x%x, create init dm dev", rc);
        goto exit;
    }

    free(fsItem->deviceName);
    fsItem->deviceName = dmDevPath;

exit:
    FsHvbDestoryVerityTarget(&target);

    return rc;
}

int FsHvbFinal(void)
{
    if (g_vd != NULL) {
        hvb_chain_verify_data_free(g_vd);
    }

    return 0;
}

int FsHvbGetValueFromCmdLine(char *val, size_t size, const char *key)
{
    int ret;

    FS_HVB_RETURN_ERR_IF_NULL(val);
    FS_HVB_RETURN_ERR_IF_NULL(key);

    char *buffer = ReadFileData(FS_HVB_CMDLINE_PATH);
    if (buffer == NULL) {
        BEGET_LOGE("error, read %s fail", FS_HVB_CMDLINE_PATH);
        return -1;
    }

    ret = GetProcCmdlineValue(key, buffer, val, size);
    if (ret != 0) {
        BEGET_LOGE("error 0x%x, get %s val from cmdline", ret, key);
    }

    free(buffer);

    return ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
