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

#ifndef __HVB_SM3_H__
#define __HVB_SM3_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hvb_sm3.h"

int hvb_sm3_init(struct sm3_ctx_t *hash_ctx)
{
    char *hashValue = getenv("HASH_VALUE");

    printf("[Replace]:hvb_sm3_init in\n");

    if ((hashValue == NULL) || (strcmp(hashValue, "InitFail") == 0)) {
        return -1;
    }

    return SM3_OK;
}

int hvb_sm3_update(struct sm3_ctx_t *hash_ctx, const void *msg, uint32_t msg_len)
{
    char *hashValue = getenv("HASH_VALUE");

    printf("[Replace]:hvb_sm3_update in\n");

    if ((hashValue == NULL) || (strcmp(hashValue, "UpdateFail") == 0)) {
        return -1;
    }

    return SM3_OK;
}

int hvb_sm3_final(struct sm3_ctx_t *hash_ctx, uint8_t *out, uint32_t *out_len)
{
    char *hashValue = getenv("HASH_VALUE");

    printf("[Replace]:hvb_sm3_final in\n");

    if ((hashValue == NULL) || (strcmp(hashValue, "FinalFail") == 0)) {
        return -1;
    }

    out[0] = 1;
    return SM3_OK;
}

#endif
