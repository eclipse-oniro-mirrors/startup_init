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
#include "init_log.h"
#ifndef OHOS_LITE
#include "init_param.h"
#endif
#include "init_read_cfg.h"
#include "securec.h"

#define IMPORT_ARR_NAME_IN_JSON "import"

// Only OHOS l2 support parameter.
#ifndef OHOS_LITE
// Limit max length of parameter value to 96 
#define IMPORT_PARAM_VALUE_LEN 96 
// Limit max length of parameter name to 96
#define IMPORT_PARAM_NAME_LEN 96

static int ExtractCfgFile(char **cfgFile, const char *content)
{
    char *p = NULL;
    size_t contentLen = strlen(content);
    if (cfgFile == NULL || content == NULL) {
        return -1;
    }
    // without "$", which means filename without parameter.
    if ((p = strchr(content, '$')) == NULL) {
        *cfgFile = malloc(contentLen + 1);
        if (*cfgFile == NULL) {
            INIT_LOGW("Failed to allocate memory to import cfg file. err = %d\n", errno);
            return -1;
        }
        if (strncpy_s(*cfgFile, contentLen + 1, content, contentLen) != EOK) {
            INIT_LOGW("Failed to copy cfg file name.\n");
            return -1;
        }
        return 0;
    }
    size_t cfgFileLen = strlen(content) + IMPORT_PARAM_VALUE_LEN + 1;
    if ((*cfgFile = malloc(cfgFileLen)) == NULL) {
        INIT_LOGW("Failed to allocate memory to import cfg file. err = %d\n", errno);
        return -1;
    }

    // Copy head of import item.
    if (strncpy_s(*cfgFile, cfgFileLen, content, p - content) != EOK) {
        INIT_LOGE("Failed to copy head of cfg file name\n");
        return -1;
    }
    // Skip '$'
    p++;
    if (*p == '{') {
        p++;
        char *right = strchr(p, '}');
        if (right == NULL) {
            INIT_LOGE("Invalid cfg file name, miss '}'.\n");
            return -1;
        }
        if (right - p > IMPORT_PARAM_NAME_LEN) {
            INIT_LOGE("Parameter name longer than %d\n", IMPORT_PARAM_NAME_LEN);
            return -1;
        }
        char paramName[IMPORT_PARAM_NAME_LEN] = {};
        char paramValue[IMPORT_PARAM_VALUE_LEN] = {};
        unsigned int valueLen = IMPORT_PARAM_VALUE_LEN;
        if (strncpy_s(paramName, IMPORT_PARAM_NAME_LEN - 1, p, right - p) != EOK) {
            INIT_LOGE("Failed to copy parameter name\n");
            return -1;
        }
        if (SystemReadParam(paramName, paramValue, &valueLen) < 0) {
            INIT_LOGE("Failed to read parameter \" %s \"\n", paramName);
            return -1;
        }
        if (strncat_s(*cfgFile, cfgFileLen, paramValue, IMPORT_PARAM_VALUE_LEN) != EOK) {
            INIT_LOGE("Failed to concatenate parameter\n");
            return -1;
        }
        // OK, parameter was handled, now concatenate rest of import content.
        // Skip '}'
        right++;
        if (strncat_s(*cfgFile, cfgFileLen, right, contentLen - (right - content)) != EOK) {
            INIT_LOGE("Failed to concatenate rest of import content\n");
            return -1;
        }
        return 0;
    }
    INIT_LOGE("Cannot extract import config file from \" %s \"\n", content);
    return -1;
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
