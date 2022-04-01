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
#include "securec.h"
#include "sys_param.h"

static int StartProcess(const char *name, const char *extArgv[], int extArgc)
{
    if (name == NULL) {
        BEGET_LOGE("Start ondemand service failed, service name is null.");
        return -1;
    }
    int extraArg = 0;
    if ((extArgv != NULL) && (extArgc > 0)) {
        BEGET_LOGI("Start service by extra args");
        extraArg = 1;
    }
    if (extraArg == 1) {
        unsigned int len = 0;
        for (int i = 0; i < extArgc; i++) {
            len += strlen(extArgv[i]);
        }
        len += strlen(name) + extArgc + 1;
        char *nameValue = (char *)calloc(len, sizeof(char));
        if (nameValue == NULL) {
            BEGET_LOGE("Failed calloc err=%d", errno);
            return -1;
        }
        if (strncat_s(nameValue, len, name, strlen(name)) != 0) {
            BEGET_LOGE("Failed strncat_s name err=%d", errno);
            return -1;
        }
        for (int j = 0; j < extArgc; j++) {
            if (strncat_s(nameValue, len, "|", 1) != 0) {
                BEGET_LOGE("Failed strncat_s \"|\"err=%d", errno);
                return -1;
            }
            if (strncat_s(nameValue, len, extArgv[j], strlen(extArgv[j])) != 0) {
                BEGET_LOGE("Failed strncat_s err=%d", errno);
                return -1;
            }
        }
        if (SystemSetParameter("ohos.ctl.start", nameValue) != 0) {
            BEGET_LOGE("Set param for %s failed.\n", nameValue);
            free(nameValue);
            return -1;
        }
        free(nameValue);
    } else {
        if (SystemSetParameter("ohos.ctl.start", name) != 0) {
            BEGET_LOGE("Set param for %s failed.\n", name);
            return -1;
        }
    }
    return 0;
}

static int StopProcess(const char *serviceName)
{
    if (serviceName == NULL) {
        BEGET_LOGE("Stop ondemand service failed, service is null.\n");
        return -1;
    }
    if (SystemSetParameter("ohos.ctl.stop", serviceName) != 0) {
        BEGET_LOGE("Set param for %s failed.\n", serviceName);
        return -1;
    }
    return 0;
}

static int GetCurrentServiceStatus(const char *serviceName, ServiceStatus *status)
{
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1,
        "%s.%s", STARTUP_SERVICE_CTL, serviceName) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    char paramValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int valueLen = PARAM_VALUE_LEN_MAX;
    if (SystemGetParameter(paramName, paramValue, &valueLen) != 0) {
        BEGET_LOGE("Failed get paramName.");
        return -1;
    }
    int size = 0;
    const InitArgInfo *statusMap = GetServieStatusMap(&size);
    *status = GetMapValue(paramValue, statusMap, size, SERVICE_IDLE);
    return 0;
}

static int RestartProcess(const char *serviceName, const char *extArgv[], int extArgc)
{
    if (serviceName == NULL) {
        BEGET_LOGE("Restart ondemand service failed, service is null.\n");
        return -1;
    }
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
    if (serviceName == NULL) {
        BEGET_LOGE("Service wait failed, service is null.\n");
        return -1;
    }
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
    if (serviceName == NULL) {
        BEGET_LOGE("Service getctl failed, service is null.");
        return -1;
    }
    int ret = ServiceControlWithExtra(serviceName, action, NULL, 0);
    return ret;
}

int ServiceWaitForStatus(const char *serviceName, ServiceStatus status, int waitTimeout)
{
    char *state = NULL;
    int size = 0;
    const InitArgInfo *statusMap = GetServieStatusMap(&size);
    if (((int)status < size) && (statusMap[status].value == (int)status)) {
        state = statusMap[status].name;
    }
    if (serviceName == NULL || state == NULL || waitTimeout <= 0) {
        BEGET_LOGE("Service wait failed, service name is null or status invalid %d", status);
        return -1;
    }
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "%s.%s",
        STARTUP_SERVICE_CTL, serviceName) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    if (SystemWaitParameter(paramName, state, waitTimeout) != 0) {
        BEGET_LOGE("Wait param for %s failed.", paramName);
        return -1;
    }
    BEGET_LOGI("Success wait");
    return 0;
}

int ServiceSetReady(const char *serviceName)
{
    if (serviceName == NULL) {
        BEGET_LOGE("Service wait failed, service is null.");
        return -1;
    }
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "%s.%s",
        STARTUP_SERVICE_CTL, serviceName) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    if (SystemSetParameter(paramName, "ready") != 0) {
        BEGET_LOGE("Set param for %s failed.", paramName);
        return -1;
    }
    BEGET_LOGI("Success set %s read", serviceName);
    return 0;
}

int StartServiceByTimer(const char *serviceName, uint64_t timeout)
{
    if (serviceName == NULL) {
        BEGET_LOGE("Request start serivce by timer with invalid service name");
        return -1;
    }

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

    if (SystemSetParameter("ohos.servicectrl.timer_start", value) != 0) {
        BEGET_LOGE("Failed to set parameter \' ohos.servicectrl.timer_start \' with value \' %s \'", value);
        return -1;
    }
    return 0;
}

int StopServiceTimer(const char *serviceName)
{
    if (serviceName == NULL) {
        BEGET_LOGE("Request stop serivce timer with invalid service name");
        return -1;
    }

    char value[PARAM_VALUE_LEN_MAX] = {};
    int ret = strncpy_s(value, PARAM_VALUE_LEN_MAX - 1, serviceName, strlen(serviceName));
    if (ret < 0) {
        BEGET_LOGE("Failed to copy service name to parameter");
        return -1;
    }

    if (SystemSetParameter("ohos.servicectrl", value) != 0) {
        BEGET_LOGE("Failed to set parameter \' ohos.servicectrl.timer_stop \' with value \' %s \'", value);
        return -1;
    }
    return 0;
}
