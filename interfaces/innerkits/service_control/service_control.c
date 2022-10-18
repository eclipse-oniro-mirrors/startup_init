/*
* Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "service_control.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beget_ext.h"
#include "init_utils.h"
#include "init_param.h"
#include "parameter.h"
#include "securec.h"

static int StartProcess(const char *name, const char *extArgv[], int extArgc)
{
    BEGET_ERROR_CHECK(name != NULL, return -1, "Service name is null.");
    int extraArg = 0;
    if ((extArgv != NULL) && (extArgc > 0)) {
        BEGET_LOGI("Start service by extra args");
        extraArg = 1;
    }
    int ret = 0;
    if (extraArg == 1) {
        unsigned int len = 0;
        for (int i = 0; i < extArgc; i++) {
            len += strlen(extArgv[i]);
        }
        len += strlen(name) + extArgc + 1;
        char *nameValue = (char *)calloc(len, sizeof(char));
        BEGET_ERROR_CHECK(nameValue != NULL, return -1, "Failed calloc err=%d", errno);

        ret = strncat_s(nameValue, len, name, strlen(name));
        if (ret != 0) {
            free(nameValue);
            BEGET_LOGE("Failed to cat name");
            return -1;
        }
        for (int j = 0; j < extArgc; j++) {
            ret = strncat_s(nameValue, len, "|", 1);
            if (ret == 0) {
                ret = strncat_s(nameValue, len, extArgv[j], strlen(extArgv[j]));
            }
            if (ret != 0) {
                free(nameValue);
                BEGET_LOGE("Failed to cat name");
                return -1;
            }
        }
        ret = SystemSetParameter("ohos.ctl.start", nameValue);
        free(nameValue);
    } else {
        ret = SystemSetParameter("ohos.ctl.start", name);
    }
    return ret;
}

static int StopProcess(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    return SystemSetParameter("ohos.ctl.stop", serviceName);
}

static int GetCurrentServiceStatus(const char *serviceName, ServiceStatus *status)
{
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1,
        "%s.%s", STARTUP_SERVICE_CTL, serviceName) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    uint32_t value = GetUintParameter(paramName, SERVICE_IDLE);
    *status = (ServiceStatus)value;
    return 0;
}

static int RestartProcess(const char *serviceName, const char *extArgv[], int extArgc)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    ServiceStatus status = SERVICE_IDLE;
    if (GetCurrentServiceStatus(serviceName, &status) != 0) {
        BEGET_LOGE("Get service status failed.\n");
        return -1;
    }
    if (status == SERVICE_STARTED || status == SERVICE_READY) {
        if (StopProcess(serviceName) != 0) {
            BEGET_LOGE("Stop service %s failed", serviceName);
            return -1;
        }
        if (ServiceWaitForStatus(serviceName, SERVICE_STOPPED, DEFAULT_PARAM_WAIT_TIMEOUT) != 0) {
            BEGET_LOGE("Failed wait service %s stopped", serviceName);
            return -1;
        }
        if (StartProcess(serviceName, extArgv, extArgc) != 0) {
            BEGET_LOGE("Start service %s failed", serviceName);
            return -1;
        }
    } else if (status != SERVICE_STARTING) {
        if (StartProcess(serviceName, extArgv, extArgc) != 0) {
            BEGET_LOGE("Start service %s failed", serviceName);
            return -1;
        }
    } else {
        BEGET_LOGE("Current service status: %d is not support.", status);
    }
    return 0;
}

int ServiceControlWithExtra(const char *serviceName, int action, const char *extArgv[], int extArgc)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    int ret = 0;
    switch (action) {
        case START:
            ret = StartProcess(serviceName, extArgv, extArgc);
            break;
        case STOP:
            ret = StopProcess(serviceName);
            break;
        case RESTART:
            ret = RestartProcess(serviceName, extArgv, extArgc);
            break;
        default:
            BEGET_LOGE("Set service %s action %d error", serviceName, action);
            ret = -1;
            break;
    }
    return ret;
}

int ServiceControl(const char *serviceName, int action)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    int ret = ServiceControlWithExtra(serviceName, action, NULL, 0);
    return ret;
}

static int GetProcessInfo(const char *serviceName, char *nameBuffer, char *valueBuffer, ServiceStatus status)
{
    if (snprintf_s(nameBuffer, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "%s.%s",
        STARTUP_SERVICE_CTL, serviceName) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    if (snprintf_s(valueBuffer, MAX_INT_LEN, MAX_INT_LEN - 1, "%d", (int)status) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    return 0;
}

int ServiceWaitForStatus(const char *serviceName, ServiceStatus status, int waitTimeout)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    BEGET_ERROR_CHECK(waitTimeout >= 0, return -1, "Invalid timeout.");
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    char value[MAX_INT_LEN] = {0};
    int ret = GetProcessInfo(serviceName, paramName, value, status);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to get param info.");
    return (SystemWaitParameter(paramName, value, waitTimeout) != 0) ? -1 : 0;
}

int ServiceSetReady(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    char value[MAX_INT_LEN] = {0};
    int ret = GetProcessInfo(serviceName, paramName, value, SERVICE_READY);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to get param info.");
    return SystemSetParameter(paramName, value);
}

int StartServiceByTimer(const char *serviceName, uint64_t timeout)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    if (timeout == 0) {
        // start service immediately.
        return ServiceControl(serviceName, START);
    }
    // restrict timeout value, not too long.
    char value[PARAM_VALUE_LEN_MAX] = {};
    if (snprintf_s(value, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "%s|%lld", serviceName, timeout) == -1) {
        BEGET_LOGE("Failed to build parameter value");
        return -1;
    }
    return SystemSetParameter("ohos.servicectrl.timer_start", value);
}

int StopServiceTimer(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    return SystemSetParameter("ohos.servicectrl.timer_stop", serviceName);
}
