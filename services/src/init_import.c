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
#include <unistd.h>
#include "cJSON.h"
#include "init_cmds.h"
#include "init_log.h"
#include "init_read_cfg.h"
#include "securec.h"

#define IMPORT_ARR_NAME_IN_JSON "import"

#ifndef OHOS_LITE
static int ExtractCfgFile(char **cfgFile, char *content)
{
    if ((!cfgFile) || (!content)) {
        return -1;
    }
    size_t cfgFileLen = strlen(content) + MAX_PARAM_VALUE_LEN + 1;
    if ((*cfgFile = malloc(cfgFileLen)) == NULL) {
        INIT_LOGW("Failed to allocate memory to import cfg file. err = %d\n", errno);
        return -1;
    }
    int ret = GetParamValue(content, *cfgFile, cfgFileLen);
    return ret;
}
#endif

void ParseAllImports(cJSON *root)
{
    cJSON *importAttr = cJSON_GetObjectItemCaseSensitive(root, IMPORT_ARR_NAME_IN_JSON);
    char *cfgFile = NULL;
    if (!cJSON_IsArray(importAttr)) {
        return;
    }
    int importAttrSize = cJSON_GetArraySize(importAttr);

    for (int i = 0; i < importAttrSize; i++) {
        cJSON *importItem = cJSON_GetArrayItem(importAttr, i);
        if (!cJSON_IsString(importItem)) {
            INIT_LOGE("Invalid type of import item. should be string\n");
            return;
        }
        char *importContent = cJSON_GetStringValue(importItem);
        if (importContent == NULL) {
            INIT_LOGE("cannot get import config file\n");
            return;
        }
// Only OHOS L2 support parameter.
#ifndef OHOS_LITE
        if (ExtractCfgFile(&cfgFile, importContent) < 0) {
            INIT_LOGW("Failed to import from %s\n", importContent);
            if (cfgFile != NULL) {
                free(cfgFile);
                cfgFile = NULL;
            }
            continue;
        }
#else
        cfgFile = importContent;
#endif
        INIT_LOGI("Import %s...\n", cfgFile);
        ParseInitCfg(cfgFile);
        // Do not forget to free memory.
        free(cfgFile);
        cfgFile = NULL;
    }
     INIT_LOGD("parse import file done\n");
    return;
}
