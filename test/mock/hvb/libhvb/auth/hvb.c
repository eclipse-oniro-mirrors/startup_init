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
#include "hvb.h"

static struct hvb_cert_data g_testCert[1] = { 0 };

static struct hvb_verified_data g_testVd = {
    .num_loaded_certs = 1,
    .certs = &g_testCert[0],
};

enum hvb_errno hvb_chain_verify(struct hvb_ops *ops, const char *rvt_ptn, const char *const *hash_ptn_list,
    struct hvb_verified_data **out_vd)
{
    char *verifyValue = getenv("VERIFY_VALUE");

    printf("[Replace]:hvb_chain_verify in\n");

    if ((verifyValue == NULL) || (strcmp(verifyValue, "Fail") == 0)) {
        *out_vd = NULL;
        return HVB_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(verifyValue, "PartFail") == 0) {
        *out_vd = &g_testVd;
        return HVB_ERROR_UNSUPPORTED_VERSION;
    }
    if (strcmp(verifyValue, "Succeed") == 0) {
        *out_vd = &g_testVd;
        return HVB_OK;
    }

    return HVB_OK;
}

void hvb_chain_verify_data_free(struct hvb_verified_data *vd)
{
    vd = NULL;
    return;
}
