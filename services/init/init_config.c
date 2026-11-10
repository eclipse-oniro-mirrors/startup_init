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
#include "init_group_manager.h"

static void ParseAllImports(const cJSON *root, int depth);

#define IMPORT_MAX_LEVEL 100

InitContextType GetConfigContextType(const char *cfgName)
{
    static const char *vendorDir[] = {
        "/vendor/etc/init/", "/chipset/etc/init/", "/chip_prod/etc/init/"
    };

    for (size_t j = 0; j < ARRAY_LENGTH(vendorDir); j++) {
        if (strncmp(vendorDir[j], cfgName, strlen(vendorDir[j])) == 0) {
            return INIT_CONTEXT_CHIPSET;
        }
    }
    return INIT_CONTEXT_MAIN;
}

static void ParseInitCfgContents(const char *cfgName, const cJSON *root, int depth)
{
    INIT_ERROR_CHECK(root != NULL, return, "Root is null");
    ConfigContext context = { INIT_CONTEXT_MAIN };
    context.type = GetConfigContextType(cfgName);
    INIT_LOGV("Parse %s configs in context %d", cfgName, context.type);
    ParseAllServices(root, &context);
    // parse jobs
    ParseAllJobs(root, &context);
    // parse imports
    ParseAllImports(root, depth);
}

int ParseInitCfg(const char *configFile, void *context)
{
    int depth = 0;
    INIT_CHECK(context == NULL, depth = *(int *)context);
    INIT_ERROR_CHECK(depth < IMPORT_MAX_LEVEL, return -1,
        "Import level too deep, max level is %d", IMPORT_MAX_LEVEL);
    INIT_LOGV("Parse init configs from %s", configFile);
    char *fileBuf = ReadFileToBuf(configFile);
    INIT_ERROR_CHECK(fileBuf != NULL, return -1, "Cfg error, %s not found", configFile);

    cJSON *fileRoot = cJSON_Parse(fileBuf);
    INIT_ERROR_CHECK(fileRoot != NULL, free(fileBuf);
        return -1, "Cfg error, failed to parse json %s ", configFile);

    ParseInitCfgContents(configFile, fileRoot, depth);
    cJSON_Delete(fileRoot);
    free(fileBuf);
    return 0;
}

static void ParseAllImports(const cJSON *root, int depth)
{
    INIT_ERROR_CHECK(depth < IMPORT_MAX_LEVEL, return,
        "Import level too deep, max level is %d", IMPORT_MAX_LEVEL);
    char *tmpParamValue = calloc(PARAM_VALUE_LEN_MAX + 1, sizeof(char));
    INIT_ERROR_CHECK(tmpParamValue != NULL, return, "Failed to alloc memory for param");

    cJSON *importAttr = cJSON_GetObjectItemCaseSensitive(root, "import");
    if (!cJSON_IsArray(importAttr)) {
        free(tmpParamValue);
        return;
    }
    int importAttrSize = cJSON_GetArraySize(importAttr);
    int nextDepth = depth + 1;
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
        ParseInitCfg(tmpParamValue, &nextDepth);
    }
    free(tmpParamValue);
    return;
}

static void RunLegacyBootModeConfig(void)
{
    // parse cfg
    char buffer[32] = {0}; // 32 reason max leb
    uint32_t len = sizeof(buffer);
    SystemReadParam("ohos.boot.mode", buffer, &len);
    INIT_LOGI("ohos.boot.mode %s", buffer);
    int maintenance = InRepairMode();
    if ((strcmp(buffer, "charger_mode") == 0) || (GetBootModeFromMisc() == GROUP_CHARGE)) {
        ParseInitCfg(INIT_CONFIGURATION_FILE, NULL);
        ReadFileInDir(OTHER_CHARGE_PATH, ".cfg", ParseInitCfg, NULL);
        ParseCfgByPriority(INIT_CFG_FIEL_PATH);
    } else if (strcmp(buffer, "charger") == 0) {
        ReadFileInDir(OTHER_CHARGE_PATH, ".cfg", ParseInitCfg, NULL);
    } else if (IsPenglaiMode()) {
        INIT_LOGI("enter penglai mode");
        ParseCfgByPriority(PENGLAI_CFG_FILE_PATH);
    } else if (InRescueMode() == 0) {
        ReadFileInDir(INIT_RESCUE_MODE_PATH, ".cfg", ParseInitCfg, NULL);
    } else if (maintenance == MAINTENANCE_RECOVERY_TYPE || maintenance == MAINTENANCE_RECOVERY_COMPLETE_TYPE) {
        ReadFileInDir(MAINTENANCE_RECOVERY_PATH, ".cfg", ParseInitCfg, NULL);
    } else if (InUpdaterMode() == 0) {
        ParseInitCfg(INIT_CONFIGURATION_FILE, NULL);
        ParseCfgByPriority(INIT_CFG_FIEL_PATH);
    } else {
        ReadFileInDir("/etc", ".cfg", ParseInitCfg, NULL);
    }
}

#if defined(OHOS_LITE)
void ReadConfig(void)
{
    RunLegacyBootModeConfig();
}
#else

static const char *GetJsonStringValue(const cJSON *item, const char *key)
{
    const cJSON *value = cJSON_GetObjectItem(item, key);
    if (value == NULL || !cJSON_IsString(value)) {
        return NULL;
    }
    return value->valuestring;
}

static bool MatchCmdLineCond(const cJSON *cond, const char *bootMode)
{
    const char *param = GetJsonStringValue(cond, "param");
    const char *value = GetJsonStringValue(cond, "value");
    if (param == NULL || value == NULL) {
        return false;
    }
    bool exact = cJSON_IsTrue(cJSON_GetObjectItem(cond, "exact"));
    if (!exact && strcmp(param, BOOT_MODE_NAME) == 0) {
        return strcmp(bootMode, value) == 0;
    }
    char actual[32] = {0};
    int ret = exact ? GetExactParameterFromCmdLine(param, actual, sizeof(actual))
                    : GetParameterFromCmdLine(param, actual, sizeof(actual));
    if (ret != 0) {
        return false;
    }
    return strcmp(actual, value) == 0;
}

static bool MatchBootModeCond(const cJSON *cond, const char *bootMode)
{
    const char *type = GetJsonStringValue(cond, "type");
    if (type == NULL) {
        return false;
    }
    if (strcmp(type, "cmdline") == 0) {
        return MatchCmdLineCond(cond, bootMode);
    }
    if (strcmp(type, "misc") == 0) {
        const char *value = GetJsonStringValue(cond, "value");
        char reason[64] = {0};
        if (value == NULL || GetMiscBootReason(reason, sizeof(reason)) != 0) {
            return false;
        }
        return strcmp(reason, value) == 0;
    }
    if (strcmp(type, "updater") == 0) {
        return InUpdaterMode() != 0;
    }
    if (strcmp(type, "default") == 0) {
        return true;
    }
    INIT_LOGI("unknown condition type %s in boot mode cfg", type);
    return false;
}

static bool MatchBootModeEntry(const cJSON *entry, const char *bootMode)
{
    const cJSON *condition = cJSON_GetObjectItem(entry, "condition");
    if (condition == NULL) {
        return false;
    }
    if (cJSON_IsArray(condition)) {
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, condition) {
            if (MatchBootModeCond(item, bootMode)) {
                return true;
            }
        }
        return false;
    }
    return MatchBootModeCond(condition, bootMode);
}

static int ExecuteBootModeAction(const cJSON *action)
{
    const char *type = GetJsonStringValue(action, "action");
    if (type == NULL) {
        return -1;
    }
    const char *path = GetJsonStringValue(action, "path");
    if (strcmp(type, "parse_default_cfg") == 0) {
        const char *cfgPath = (path != NULL) ? path : INIT_CONFIGURATION_FILE;
        INIT_LOGI("boot mode action: parse_default_cfg %s", cfgPath);
        return (ParseInitCfg(cfgPath, NULL) == 0) ? 0 : -1;
    }
    if (path == NULL) {
        INIT_LOGE("boot mode action %s missing path", type);
        return -1;
    }
    if (strcmp(type, "read_dir") == 0) {
        INIT_LOGI("boot mode action: read_dir %s", path);
        return (ReadFileInDir(path, ".cfg", ParseInitCfg, NULL) == 0) ? 0 : -1;
    }
    if (strcmp(type, "parse_priority") == 0) {
        INIT_LOGI("boot mode action: parse_priority %s", path);
        return (ParseCfgByPriority(path) == 0) ? 0 : -1;
    }
    INIT_LOGI("unknown action type %s in boot mode cfg", type);
    return -1;
}

static int ExecuteBootModeActions(const cJSON *actions)
{
    const cJSON *action = NULL;
    int ret = -1;
    cJSON_ArrayForEach(action, actions) {
        if (ExecuteBootModeAction(action) == 0) {
            ret = 0;
        }
    }
    return ret;
}

static void RunDefaultBootModeActions(void)
{
    INIT_LOGI("use default boot mode actions");
    (void)ParseInitCfg(INIT_CONFIGURATION_FILE, NULL);
    (void)ParseCfgByPriority(INIT_CFG_FIEL_PATH);
}

static cJSON *LoadBootModeCfg(cJSON **root)
{
    char *cfgBuf = ReadFileToBuf(BOOT_MODE_CFG_PATH);
    if (cfgBuf == NULL) {
        INIT_LOGE("failed to read boot mode cfg %s", BOOT_MODE_CFG_PATH);
        return NULL;
    }
    cJSON *cfgRoot = cJSON_Parse(cfgBuf);
    free(cfgBuf);
    if (cfgRoot == NULL) {
        INIT_LOGE("failed to parse boot mode cfg %s", BOOT_MODE_CFG_PATH);
        return NULL;
    }
    cJSON *bootModes = cJSON_GetObjectItem(cfgRoot, "boot_modes");
    if (bootModes == NULL || !cJSON_IsArray(bootModes)) {
        INIT_LOGE("boot mode cfg %s has no valid boot_modes", BOOT_MODE_CFG_PATH);
        cJSON_Delete(cfgRoot);
        return NULL;
    }
    *root = cfgRoot;
    return bootModes;
}

void ReadConfig(void)
{
    // parse cfg
    char buffer[32] = {0}; // 32 reason max leb
    int ret = GetParameterFromCmdLine(BOOT_MODE_NAME, buffer, sizeof(buffer));
    if (ret != 0) {
        INIT_LOGE("failed to get boot mode from cmdline");
    }
    INIT_LOGI("ohos.boot.mode %s", buffer);

    cJSON *root = NULL;
    cJSON *bootModes = LoadBootModeCfg(&root);
    if (bootModes == NULL) {
        INIT_LOGW("boot mode cfg unavailable, run legacy boot mode config");
        RunLegacyBootModeConfig();
        return;
    }

    cJSON *matched = NULL;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, bootModes) {
        if (MatchBootModeEntry(entry, buffer)) {
            matched = entry;
            break;
        }
    }

    if (matched == NULL) {
        INIT_LOGE("no boot mode matched, use default actions");
        cJSON_Delete(root);
        RunDefaultBootModeActions();
        return;
    }

    const char *name = GetJsonStringValue(matched, "name");
    INIT_LOGI("boot mode: %s", (name != NULL) ? name : "unknown");
    cJSON *actions = cJSON_GetObjectItem(matched, "actions");
    if (actions == NULL || !cJSON_IsArray(actions)) {
        INIT_LOGE("boot mode %s has no valid actions", (name != NULL) ? name : "unknown");
        cJSON_Delete(root);
        RunDefaultBootModeActions();
        return;
    }
    int loadRet = ExecuteBootModeActions(actions);
    if (cJSON_IsTrue(cJSON_GetObjectItem(matched, "fallbackOnEmpty")) && loadRet != 0) {
        INIT_LOGE("boot mode %s loaded no cfg (dir empty or all cfg invalid), fallback to default boot",
            (name != NULL) ? name : "unknown");
        RunDefaultBootModeActions();
    }
    cJSON_Delete(root);
}
#endif // OHOS_LITE
