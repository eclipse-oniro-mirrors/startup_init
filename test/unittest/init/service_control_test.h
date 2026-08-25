/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef STARTUP_INIT_SERVICE_CONTROL_TEST_H
#define STARTUP_INIT_SERVICE_CONTROL_TEST_H

#include <stddef.h>

#include "service_control.h"

#ifdef STARTUP_INIT_TEST

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SUPPORT_SA_MULTI_USER
typedef int (*ByUserWaitParamFunc)(const char *name, const char *value, int waitTimeout);
typedef int (*ByUserSetParamFunc)(const char *name, const char *value);

int TestInnerkitHasByUserCtrlBlankChar(const char *value);
int TestBuildByUserCtrlValue(char *buffer, size_t size, const char *serviceName, int32_t userId,
    const char *extArgv[], int extArgc);
int TestBuildByUserCtrlValueWithNullArgs(char *buffer, size_t size);
int TestSetByUserProcessParamWithNullName(void);
int TestGetByUserProcessInfo(const char *serviceName, int32_t userId, char *nameBuffer,
    char *valueBuffer, ServiceStatus status);
int TestServiceWaitForStatusByUserId(const char *serviceName, int32_t userId, ServiceStatus status,
    int waitTimeout);
void TestSetByUserWaitParamFunc(ByUserWaitParamFunc func);
void TestSetByUserSetParamFunc(ByUserSetParamFunc func);
#endif

#ifdef __cplusplus
}
#endif

#endif

#endif
