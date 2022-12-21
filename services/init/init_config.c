/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include "init.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "init_param.h"

static void ParseAllImports(const cJSON *root);

static void ParseInitCfgContents(const char *cfgName, const cJSON *root)
{
    INIT_ERROR_CHECK(root != NULL, return, "Root is null");
    ParseAllServices(root);
    // parse jobs
    ParseAllJobs(root);
    // parse imports
    ParseAllImports(root);
}

int ParseInitCfg(const char *configFile, void *context)
{
    UNUSED(context);
    INIT_LOGV("Parse init configs from %s", configFile);
    static const char *excludeCfg[] = {
        "/system/etc/init/weston.cfg"
    };
    for (int i = 0; i < (int)ARRAY_LENGTH(excludeCfg); i++) {
        if (strcmp(configFile, excludeCfg[i]) == 0) {
            INIT_LOGE("ParseInitCfg %s not support", configFile);
            return 0;
        }
    }
    char *fileBuf = ReadFileToBuf(configFile);
    INIT_ERROR_CHECK(fileBuf != NULL, return -1, "Failed to read file content %s", configFile);

    cJSON *fileRoot = cJSON_Parse(fileBuf);
    INIT_ERROR_CHECK(fileRoot != NULL, free(fileBuf);
        return -1, "Failed to parse json file %s", configFile);

    ParseInitCfgContents(configFile, fileRoot);
    cJSON_Delete(fileRoot);
    free(fileBuf);
    return 0;
}

static void ParseAllImports(const cJSON *root)
{
    char *tmpParamValue = calloc(sizeof(char), PARAM_VALUE_LEN_MAX + 1);
    INIT_ERROR_CHECK(tmpParamValue != 0, return, "Failed to alloc memory for param");

    cJSON *importAttr = cJSON_GetObjectItemCaseSensitive(root, "import");
    if (!cJSON_IsArray(importAttr)) {
        free(tmpParamValue);
        return;
    }
    int importAttrSize = cJSON_GetArraySize(importAttr);
    for (int i = 0; i < importAttrSize; i++) {
        cJSON *importItem = cJSON_GetArrayItem(importAttr, i);
        if (!cJSON_IsString(importItem)) {
            INIT_LOGE("Invalid type of import item. should be string");
            break;
        }
        char *importContent = cJSON_GetStringValue(importItem);
        if (importContent == NULL) {
            INIT_LOGE("cannot get import config file");
            break;
        }
        int ret = GetParamValue(importContent, strlen(importContent), tmpParamValue, PARAM_VALUE_LEN_MAX);
        if (ret != 0) {
            INIT_LOGE("cannot get value for %s", importContent);
            continue;
        }
        INIT_LOGI("Import %s  ...", tmpParamValue);
        ParseInitCfg(tmpParamValue, NULL);
    }
    free(tmpParamValue);
    return;
}

void ReadConfig(void)
{
    // parse cfg
    char buffer[32] = {0}; // 32 reason max leb
    uint32_t len = sizeof(buffer);
    SystemReadParam("ohos.boot.reboot_reason", buffer, &len);
    INIT_LOGV("ohos.boot.reboot_reason %s", buffer);
    if (strcmp(buffer, "poweroff_charge") == 0) {
        ParseInitCfg(INIT_CONFIGURATION_FILE, NULL);
        ReadFileInDir(OTHER_CHARGE_PATH, ".cfg", ParseInitCfg, NULL);
    } else if (InUpdaterMode() == 0) {
        ParseInitCfg(INIT_CONFIGURATION_FILE, NULL);
        ParseInitCfgByPriority();
    } else {
        ReadFileInDir("/etc", ".cfg", ParseInitCfg, NULL);
    }
}
