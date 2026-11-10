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

#ifndef BASE_STARTUP_INIT_INIT_CONFIG_TEST_WRAPPER_H
#define BASE_STARTUP_INIT_INIT_CONFIG_TEST_WRAPPER_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_MODE_MOCK_MAX_ACTIONS 8
#define BOOT_MODE_MOCK_ACTION_LEN 64

extern bool g_mockBootModeEnv;
extern char g_mockBootMode[32];
extern const char *MOCK_MISC_REASON;
extern int g_mockUpdater;
extern int g_mockCfgLoaded;
#define BOOT_MODE_MOCK_CMDLINE_MAX 8
typedef struct {
    const char *paramName;
    const char *value;
} BootModeMockCmdline;
extern BootModeMockCmdline g_mockCmdline[BOOT_MODE_MOCK_CMDLINE_MAX];
void MockCmdlineParamSet(const char *paramName, const char *value);

extern const char *MOCK_BOOT_MODE_CFG;
extern char g_mockCfgActions[BOOT_MODE_MOCK_MAX_ACTIONS][BOOT_MODE_MOCK_ACTION_LEN];
extern int g_mockCfgActionCount;

void EnterBootModeMock(const char *mode);
void ExitBootModeMock(void);

#ifdef __cplusplus
}
#endif

#endif // BASE_STARTUP_INIT_INIT_CONFIG_TEST_WRAPPER_H
