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

#include "begetctl_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hilog/log.h"
#include "init_utils.h"
#include "securec.h"
#include "sys_param.h"

static int StartDynamicProcess(const char *name, const char *extArgv[], int extArgc)
{
    if (name == NULL) {
        HILOG_ERROR(LOG_CORE, "Start dynamic service failed, service name is null.");
        return -1;
    }
    int extraArg = 0;
    if ((extArgv != NULL) && (extArgc > 0)) {
        HILOG_INFO(LOG_CORE, "Start service by extra args");
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
            HILOG_ERROR(LOG_CORE, "Failed calloc err=%{public}d", errno);
            return -1;
        }
        if (strncat_s(nameValue, len, name, strlen(name)) != 0) {
            HILOG_ERROR(LOG_CORE, "Failed strncat_s name err=%{public}d", errno);
            return -1;
        }
        for (int j = 0; j < extArgc; j++) {
            if (strncat_s(nameValue, len, "|", 1) != 0) {
                HILOG_ERROR(LOG_CORE, "Failed strncat_s \"|\"err=%{public}d", errno);
                return -1;
            }
            if (strncat_s(nameValue, len, extArgv[j], strlen(extArgv[j])) != 0) {
                HILOG_ERROR(LOG_CORE, "Failed strncat_s err=%{public}d", errno);
                return -1;
            }
        }
        HILOG_INFO(LOG_CORE,"nameValue is %{public}s", nameValue);
        if (SystemSetParameter("ohos.ctl.start", nameValue) != 0) {
            HILOG_ERROR(LOG_CORE, "Set param for %{public}s failed.\n", nameValue);
            free(nameValue);
            return -1;
        }
        free(nameValue);
    } else {
        if (SystemSetParameter("ohos.ctl.start", name) != 0) {
            HILOG_ERROR(LOG_CORE, "Set param for %{public}s failed.\n", name);
            return -1;
        }
    }
    return 0;
}

static int StopDynamicProcess(const char *serviceName)
{
    if (serviceName == NULL) {
        HILOG_ERROR(LOG_CORE, "Stop dynamic service failed, service is null.\n");
        return -1;
    }
    if (SystemSetParameter("ohos.ctl.stop", serviceName) != 0) {
        HILOG_ERROR(LOG_CORE, "Set param for %{public}s failed.\n", serviceName);
        return -1;
    }
    return 0;
}

static int GetCurrentServiceStatus(const char *serviceName, char *paramValue, unsigned int valueLen)
{
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "init.svc.%s", serviceName) == -1) {
        HILOG_ERROR(LOG_CORE, "Failed snprintf_s err=%{public}d", errno);
        return -1;
    }
    if (SystemGetParameter(paramName, paramValue, &valueLen) != 0) {
        HILOG_ERROR(LOG_CORE, "Failed get paramName.");
        return -1;
    }
    return 0;
}

static int RestartDynamicProcess(const char *serviceName, const char *extArgv[], int extArgc)
{
    if (serviceName == NULL) {
        HILOG_ERROR(LOG_CORE, "Restart dynamic service failed, service is null.\n");
        return -1;
    }
    char paramValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int valueLen = PARAM_VALUE_LEN_MAX;
    if (GetCurrentServiceStatus(serviceName, paramValue, valueLen) != 0) {
        HILOG_ERROR(LOG_CORE, "Get service status failed.\n");
        return -1;
    }
    if (strcmp(paramValue, "running") == 0) {
        if (StopDynamicProcess(serviceName) != 0) {
            HILOG_ERROR(LOG_CORE, "Stop service %{public}s failed", serviceName);
            return -1;
        }
        if (ServiceWaitForStatus(serviceName, "stopped", DEFAULT_PARAM_WAIT_TIMEOUT) != 0) {
            HILOG_ERROR(LOG_CORE, "Failed wait service %{public}s stopped", serviceName);
            return -1;
        }
        if (StartDynamicProcess(serviceName, extArgv, extArgc) != 0) {
            HILOG_ERROR(LOG_CORE, "Start service %{public}s failed", serviceName);
            return -1;
        }
    } else if (strcmp(paramValue, "stopped") == 0) {
        if (StartDynamicProcess(serviceName, extArgv, extArgc) != 0) {
            HILOG_ERROR(LOG_CORE, "Start service %{public}s failed", serviceName);
            return -1;
        }
    } else {
        HILOG_ERROR(LOG_CORE, "Current service status: %{public}s is not support.", paramValue);
    }
    return 0;
}

int ServiceControlWithExtra(const char *serviceName, int action, const char *extArgv[], int extArgc)
{
    if (serviceName == NULL) {
        HILOG_ERROR(LOG_CORE, "Service wait failed, service is null.\n");
        return -1;
    }
    int ret = 0;
    switch (action) {
        case START:
            ret = StartDynamicProcess(serviceName, extArgv, extArgc);
            break;
        case STOP:
            ret = StopDynamicProcess(serviceName);
            break;
        case RESTART:
            ret = RestartDynamicProcess(serviceName, extArgv, extArgc);
            break;
        default:
            HILOG_ERROR(LOG_CORE, "Set service %{public}s action %d error", serviceName, action);
            ret = -1;
            break;
    }
    return ret;
}

int ServiceControl(const char *serviceName, int action)
{
    if (serviceName == 0) {
        HILOG_ERROR(LOG_CORE, "Service getctl failed, service is null.\n");
        return -1;
    }
    int ret = ServiceControlWithExtra(serviceName, action, NULL, 0);
    return ret;
}

// Service status can set "running", "stopping", "stopped", "reseting". waitTimeout(s).
int ServiceWaitForStatus(const char *serviceName, const char *status, int waitTimeout)
{
    if (serviceName == NULL) {
        HILOG_ERROR(LOG_CORE, "Service wait failed, service is null.\n");
        return -1;
    }
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "init.svc.%s", serviceName) == -1) {
        HILOG_ERROR(LOG_CORE, "Failed snprintf_s err=%{public}d", errno);
        return -1;
    }
    if (SystemWaitParameter(paramName, status, waitTimeout) != 0) {
        HILOG_ERROR(LOG_CORE, "Wait param for %{public}s failed.\n", paramName);
        return -1;
    }
    HILOG_INFO(LOG_CORE, "Success wait");
    return 0;
}
