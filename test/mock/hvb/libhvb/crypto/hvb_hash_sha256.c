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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hvb_crypto.h"

int hash_ctx_init(struct hash_ctx_t *hash_ctx, enum hash_alg_type alg_type)
{
    char *hashValue = getenv("HASH_VALUE");

    printf("[Replace]:hash_ctx_init in\n");

    if ((hashValue == NULL) || (strcmp(hashValue, "InitFail") == 0)) {
        return -1;
    }

    return 0;
}

int hash_calc_update(struct hash_ctx_t *hash_ctx, const void *msg, uint32_t msg_len)
{
    char *hashValue = getenv("HASH_VALUE");

    printf("[Replace]:hash_calc_update in\n");

    if ((hashValue == NULL) || (strcmp(hashValue, "UpdateFail") == 0)) {
        return -1;
    }

    return 0;
}

int hash_calc_do_final(struct hash_ctx_t *hash_ctx, const void *msg, uint32_t msg_len, uint8_t *out, uint32_t out_len)
{
    char *hashValue = getenv("HASH_VALUE");

    printf("[Replace]:hash_calc_do_final in\n");

    if ((hashValue == NULL) || (strcmp(hashValue, "FinalFail") == 0)) {
        return -1;
    }

    return 0;
}
