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

#include "hvb_cert.h"

#define BLOCK_SIZE 4096

enum hvb_errno hvb_cert_parser(struct hvb_cert *cert, struct hvb_buf *cert_buf)
{
    cert->version_major = 0;
    cert->version_minor = 0;
    cert->image_original_len = BLOCK_SIZE;
    cert->image_len = BLOCK_SIZE;
    cert->rollback_location = 1;
    cert->rollback_index = 0;
    cert->verity_type = HVB_IMAGE_TYPE_HASHTREE;
    cert->hash_algo = 0;
    cert->data_block_size = BLOCK_SIZE;
    cert->hash_block_size = BLOCK_SIZE;
    return 0;
}