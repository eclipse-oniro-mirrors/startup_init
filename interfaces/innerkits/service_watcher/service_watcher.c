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
#include "service_watcher.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "beget_ext.h"
#include "init_utils.h"
#include "parameter.h"
#include "securec.h"
#include "service_control.h"
#include "sysparam_errno.h"

static void ServiceStateChange(const char *key, const char *value, void *context)
{
    ServiceStatusChangePtr callback = (ServiceStatusChangePtr)context;
    uint32_t v = 0;
    int ret = StringToUint(value, &v);
    BEGET_ERROR_CHECK(ret == 0, return, "Failed to get value from %s", value);

    // get pid
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    ret = snprintf_s(paramName, sizeof(paramName), sizeof(paramName) - 1, "%s.pid", key);
    BEGET_ERROR_CHECK(ret != -1, return, "Failed to get format pid ret %d for %s ", ret, key);

    ServiceInfo info = {0};
    info.status = (ServiceStatus)v;
    info.pid = (pid_t)GetUintParameter(paramName, INVALID_PID);
    if (strlen(key) > strlen(STARTUP_SERVICE_CTL)) {
        callback(key + strlen(STARTUP_SERVICE_CTL) + 1, &info);
    } else {
        BEGET_LOGE("Invalid service name %s %s", key, value);
    }
}

int ServiceWatchForStatus(const char *serviceName, ServiceStatusChangePtr changeCallback)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return EC_INVALID, "Service watch failed, service is null.");
    BEGET_ERROR_CHECK(changeCallback != NULL, return EC_INVALID, "Service watch failed, callback is null.");

    char paramName[PARAM_NAME_LEN_MAX] = {0};
    BEGET_LOGI("Watcher service %s status", serviceName);
    if (snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1,
        "%s.%s", STARTUP_SERVICE_CTL, serviceName) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return EC_SYSTEM_ERR;
    }
    int ret = SystemWatchParameter(paramName, ServiceStateChange, (void *)changeCallback);
    if (ret != 0) {
        BEGET_LOGE("Failed to watcher service %s ret %d.", serviceName, ret);
        if (ret == DAC_RESULT_FORBIDED) {
            return SYSPARAM_PERMISSION_DENIED;
        }
        return EC_SYSTEM_ERR;
    }
    return 0;
}

int WatchParameter(const char *keyprefix, ParameterChgPtr callback, void *context)
{
    if (keyprefix == NULL) {
        return EC_INVALID;
    }
#ifdef NO_PARAM_WATCHER
    printf("ParameterWatcher is disabled.");
    return EC_INVALID;
#else
    int ret = SystemWatchParameter(keyprefix, callback, context);
    BEGET_CHECK_ONLY_ELOG(ret == 0, "WatchParameter failed! the errNum is %d", ret);
    return ret;
#endif
}