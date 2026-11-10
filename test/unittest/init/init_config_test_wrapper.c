/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "init.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "init_param.h"
#include "init_group_manager.h"

#include "init_config_test_wrapper.h"

#define BOOT_MODE_TEST_FUNC __attribute__((unused))

#define MOCK_BOPD_RUN_STATUS_PARAM "ohos.boot.bopd.run.status"

/* ---- mock 状态 ---- */
bool g_mockBootModeEnv = false;
char g_mockBootMode[32] = {0};
const char *MOCK_MISC_REASON = NULL;
BootModeMockCmdline g_mockCmdline[BOOT_MODE_MOCK_CMDLINE_MAX];
int g_mockUpdater = 1;
int g_mockCfgLoaded = 1;
const char *MOCK_BOOT_MODE_CFG = NULL;
char g_mockCfgActions[BOOT_MODE_MOCK_MAX_ACTIONS][BOOT_MODE_MOCK_ACTION_LEN];
int g_mockCfgActionCount = 0;

static void RecordCfgAction(const char *action, const char *arg)
{
    if (g_mockCfgActionCount >= BOOT_MODE_MOCK_MAX_ACTIONS) {
        return;
    }
    if (arg != NULL) {
        snprintf(g_mockCfgActions[g_mockCfgActionCount], BOOT_MODE_MOCK_ACTION_LEN, "%s:%s", action, arg);
    } else {
        snprintf(g_mockCfgActions[g_mockCfgActionCount], BOOT_MODE_MOCK_ACTION_LEN, "%s", action);
    }
    g_mockCfgActionCount++;
}

void MockCmdlineParamSet(const char *paramName, const char *value)
{
    if (paramName == NULL) {
        return;
    }
    for (size_t i = 0; i < BOOT_MODE_MOCK_CMDLINE_MAX; i++) {
        if (g_mockCmdline[i].paramName != NULL && strcmp(g_mockCmdline[i].paramName, paramName) == 0) {
            g_mockCmdline[i].value = value;
            return;
        }
    }
    if (value == NULL) {
        return;
    }
    for (size_t i = 0; i < BOOT_MODE_MOCK_CMDLINE_MAX; i++) {
        if (g_mockCmdline[i].paramName == NULL) {
            g_mockCmdline[i].paramName = paramName;
            g_mockCmdline[i].value = value;
            return;
        }
    }
}

void EnterBootModeMock(const char *mode)
{
    g_mockBootModeEnv = true;
    if (mode != NULL) {
        snprintf(g_mockBootMode, sizeof(g_mockBootMode), "%s", mode);
    } else {
        g_mockBootMode[0] = '\0';
    }
    MOCK_MISC_REASON = NULL;
    for (size_t i = 0; i < BOOT_MODE_MOCK_CMDLINE_MAX; i++) {
        g_mockCmdline[i].paramName = NULL;
        g_mockCmdline[i].value = NULL;
    }
    if (mode != NULL && strcmp(mode, "bopd") == 0) {
        MockCmdlineParamSet(MOCK_BOPD_RUN_STATUS_PARAM, "true");
    }
    g_mockUpdater = 1;
    g_mockCfgLoaded = 1;
    MOCK_BOOT_MODE_CFG = NULL;
    g_mockCfgActionCount = 0;
}

void ExitBootModeMock(void)
{
    g_mockBootModeEnv = false;
}

static int MockCmdlineLookup(const char *paramName, char *value, size_t valueLen)
{
    for (size_t i = 0; i < BOOT_MODE_MOCK_CMDLINE_MAX; i++) {
        if (g_mockCmdline[i].paramName != NULL && strcmp(paramName, g_mockCmdline[i].paramName) == 0) {
            if (g_mockCmdline[i].value == NULL) {
                return -1;
            }
            snprintf(value, valueLen, "%s", g_mockCmdline[i].value);
            return 1;
        }
    }
    return 0;
}

static int BOOT_MODE_TEST_FUNC TestGetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)
{
    if (g_mockBootModeEnv && paramName != NULL) {
        if (strcmp(paramName, BOOT_MODE_NAME) == 0) {
            snprintf(value, valueLen, "%s", g_mockBootMode);
            return 0;
        }
        int mock = MockCmdlineLookup(paramName, value, valueLen);
        if (mock != 0) {
            return (mock > 0) ? 0 : -1;
        }
    }
    return GetParameterFromCmdLine(paramName, value, valueLen);
}

static int BOOT_MODE_TEST_FUNC TestGetExactParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)
{
    if (g_mockBootModeEnv && paramName != NULL) {
        int mock = MockCmdlineLookup(paramName, value, valueLen);
        if (mock != 0) {
            return (mock > 0) ? 0 : -1;
        }
    }
    return GetExactParameterFromCmdLine(paramName, value, valueLen);
}

static int BOOT_MODE_TEST_FUNC TestGetMiscBootReason(char *reason, size_t size)
{
    if (g_mockBootModeEnv) {
        if (MOCK_MISC_REASON == NULL) {
            return -1;
        }
        snprintf(reason, size, "%s", MOCK_MISC_REASON);
        return 0;
    }
    return GetMiscBootReason(reason, size);
}

static int BOOT_MODE_TEST_FUNC TestGetBootModeFromMisc(void)
{
    if (g_mockBootModeEnv) {
        return 0;
    }
    return GetBootModeFromMisc();
}

static int BOOT_MODE_TEST_FUNC TestInUpdaterMode(void)
{
    if (g_mockBootModeEnv) {
        return g_mockUpdater;
    }
    return InUpdaterMode();
}

static int BOOT_MODE_TEST_FUNC TestReadFileInDir(const char *dirPath, const char *includeExt,
    int (*processFile)(const char *fileName, void *context), void *context)
{
    if (g_mockBootModeEnv) {
        RecordCfgAction("ReadFileInDir", dirPath);
        return 0;
    }
    return ReadFileInDir(dirPath, includeExt, processFile, context);
}

static int BOOT_MODE_TEST_FUNC TestParseCfgByPriority(const char *filePath)
{
    if (g_mockBootModeEnv) {
        RecordCfgAction("ParseCfgByPriority", filePath);
        return g_mockCfgLoaded ? 0 : -1;
    }
    return ParseCfgByPriority(filePath);
}

static char *TestReadFileToBuf(const char *configFile)
{
    if (g_mockBootModeEnv) {
        if (configFile != NULL && strcmp(configFile, BOOT_MODE_CFG_PATH) == 0) {
            if (MOCK_BOOT_MODE_CFG == NULL) {
                return NULL;
            }
            return strdup(MOCK_BOOT_MODE_CFG);
        }
        RecordCfgAction("ParseInitCfg", configFile);
        return NULL;
    }
    return ReadFileToBuf(configFile);
}

#define GetParameterFromCmdLine TestGetParameterFromCmdLine
#define GetExactParameterFromCmdLine TestGetExactParameterFromCmdLine
#define GetMiscBootReason TestGetMiscBootReason
#define GetBootModeFromMisc TestGetBootModeFromMisc
#define InUpdaterMode TestInUpdaterMode
#define ReadFileInDir TestReadFileInDir
#define ParseCfgByPriority TestParseCfgByPriority
#define ReadFileToBuf TestReadFileToBuf
#include "../../../services/init/init_config.c"
#undef GetParameterFromCmdLine
#undef GetExactParameterFromCmdLine
#undef GetMiscBootReason
#undef GetBootModeFromMisc
#undef InUpdaterMode
#undef ReadFileInDir
#undef ParseCfgByPriority
#undef ReadFileToBuf
