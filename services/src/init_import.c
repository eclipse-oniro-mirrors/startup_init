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

#include "init_import.h"
#include <stdio.h>
#include "cJSON.h"
#include "init_read_cfg.h"

#define IMPORT_ARR_NAME_IN_JSON "import"

void ParseAllImports(cJSON *root)
{
    cJSON *importAttr = cJSON_GetObjectItemCaseSensitive(root, IMPORT_ARR_NAME_IN_JSON);

    if (!cJSON_IsArray(importAttr)) {
        printf("[Init] ParseAllImports, import item is not array!\n");
        return;
    }
    int importAttrSize = cJSON_GetArraySize(importAttr);

    for (int i = 0; i < importAttrSize; i++) {
        cJSON *importItem = cJSON_GetArrayItem(importAttr, i);
        if (!cJSON_IsString(importItem)) {
            printf("[Init] Invalid type of import item. should be string\n");
            return;
        }
        char *importFile = cJSON_GetStringValue(importItem);
        if (importFile == NULL) {
            printf("[Init] cannot get import config file\n");
            return;
        }
        printf("[Init] [Debug], ready to import %s...\n", importFile);
        ParseInitCfg(importFile);
    }
     printf("[Init] [Debug], parse import file done\n");
    return;
}