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

#include "init/service_control_test.h"

#include "init_param.h"

#ifdef SUPPORT_SA_MULTI_USER
static ByUserWaitParamFunc g_testByUserWaitParamFunc = NULL;
static ByUserSetParamFunc g_testByUserSetParamFunc = NULL;
#endif

extern int __real_SystemSetParameter(const char *name, const char *value);  // NOLINT
extern int __real_SystemWaitParameter(const char *name, const char *value, int32_t timeout);  // NOLINT

static int TestSystemSetParameter(const char *name, const char *value)
{
#ifdef SUPPORT_SA_MULTI_USER
    if (g_testByUserSetParamFunc != NULL) {
        return g_testByUserSetParamFunc(name, value);
    }
#endif
    return __real_SystemSetParameter(name, value);
}

static int TestSystemWaitParameter(const char *name, const char *value, int32_t timeout)
{
#ifdef SUPPORT_SA_MULTI_USER
    if (g_testByUserWaitParamFunc != NULL) {
        return g_testByUserWaitParamFunc(name, value, timeout);
    }
#endif
    return __real_SystemWaitParameter(name, value, timeout);
}

int __wrap_SystemSetParameter(const char *name, const char *value)  // NOLINT
{
    return TestSystemSetParameter(name, value);
}

int __wrap_SystemWaitParameter(const char *name, const char *value, int32_t timeout)  // NOLINT
{
    return TestSystemWaitParameter(name, value, timeout);
}

#define SystemSetParameter TestSystemSetParameter  // NOLINT
#define SystemWaitParameter TestSystemWaitParameter  // NOLINT
#include "../../../interfaces/innerkits/service_control/service_control.c"
#undef SystemWaitParameter
#undef SystemSetParameter

#ifdef SUPPORT_SA_MULTI_USER
int TestInnerkitHasByUserCtrlBlankChar(const char *value)
{
    return HasByUserCtrlBlankChar(value);
}

int TestBuildByUserCtrlValue(char *buffer, size_t size, const char *serviceName, int32_t userId,
    const char *extArgv[], int extArgc)
{
    ByUserCtrlValueArgs args = {serviceName, userId, extArgv, extArgc};
    return BuildByUserCtrlValue(buffer, size, &args);
}

int TestBuildByUserCtrlValueWithNullArgs(char *buffer, size_t size)
{
    return BuildByUserCtrlValue(buffer, size, NULL);
}

int TestSetByUserProcessParamWithNullName(void)
{
    ByUserCtrlValueArgs args = {"service", 100, NULL, 0};
    return SetByUserProcessParam(NULL, &args);
}

int TestGetByUserProcessInfo(const char *serviceName, int32_t userId, char *nameBuffer,
    char *valueBuffer, ServiceStatus status)
{
    return GetByUserProcessInfo(serviceName, userId, nameBuffer, valueBuffer, status);
}

int TestServiceWaitForStatusByUserId(const char *serviceName, int32_t userId, ServiceStatus status,
    int waitTimeout)
{
    return ServiceWaitForStatusByUserId(serviceName, userId, status, waitTimeout);
}

void TestSetByUserWaitParamFunc(ByUserWaitParamFunc func)
{
    g_testByUserWaitParamFunc = func;
}

void TestSetByUserSetParamFunc(ByUserSetParamFunc func)
{
    g_testByUserSetParamFunc = func;
}
#endif
