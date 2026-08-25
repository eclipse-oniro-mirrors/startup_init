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

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beget_ext.h"
#include "init_utils.h"
#include "init_param.h"
#include "parameter.h"
#include "securec.h"

#ifdef SUPPORT_SA_MULTI_USER
#define SERVICE_CTRL_BY_USER_START "ohos.ctl.start.userid"
#define SERVICE_CTRL_BY_USER_STOP "ohos.ctl.stop.userid"
#define SERVICE_CTRL_BY_USER_VALUE_MAX PARAM_VALUE_LEN_MAX
#define SERVICE_CTRL_BY_USER_NAME_MAX 54
#define SERVICE_CTRL_BY_USER_EXT_ARG_MAX 4
#define SERVICE_CTRL_BY_USER_ID_LEN_MAX 12

typedef struct {
    const char *serviceName;
    int32_t userId;
    const char **extArgv;
    int extArgc;
} ByUserCtrlValueArgs;

static int HasByUserCtrlBlankChar(const char *value)
{
    if (value == NULL) {
        return -1;
    }
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++) {
        if (isspace(*p)) {
            return 1;
        }
    }
    return 0;
}

static int ValidateByUserServiceName(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL && serviceName[0] != '\0',
        return -1, "Invalid by-user service name.");
    BEGET_ERROR_CHECK(strlen(serviceName) < SERVICE_CTRL_BY_USER_NAME_MAX,
        return -1, "By-user service name too long.");
    BEGET_ERROR_CHECK(strchr(serviceName, '|') == NULL && HasByUserCtrlBlankChar(serviceName) == 0,
        return -1, "Invalid by-user service name.");
    return 0;
}

static int BuildByUserCtrlValue(char *buffer, size_t size, const ByUserCtrlValueArgs *args)
{
    BEGET_ERROR_CHECK(buffer != NULL && size > 0, return -1, "Invalid buffer.");
    BEGET_ERROR_CHECK(args != NULL && args->userId >= 0, return -1, "Invalid by-user args.");
    BEGET_ERROR_CHECK(ValidateByUserServiceName(args->serviceName) == 0,
        return -1, "Invalid by-user service name.");
    BEGET_ERROR_CHECK(args->extArgc >= 0 && args->extArgc <= SERVICE_CTRL_BY_USER_EXT_ARG_MAX,
        return -1, "Invalid by-user ext argc.");
    BEGET_ERROR_CHECK(args->extArgc == 0 || args->extArgv != NULL, return -1, "Invalid by-user ext argv.");

    char userId[SERVICE_CTRL_BY_USER_ID_LEN_MAX] = {0};
    int ret = snprintf_s(userId, sizeof(userId), sizeof(userId) - 1, "%d", args->userId);
    if (ret <= 0) {
        BEGET_LOGE("Failed to format by-user userId.");
        return -1;
    }
    size_t valueLen = strlen(args->serviceName) + 1 + (size_t)ret;
    for (int i = 0; i < args->extArgc; i++) {
        BEGET_ERROR_CHECK(args->extArgv[i] != NULL && strchr(args->extArgv[i], '|') == NULL &&
            HasByUserCtrlBlankChar(args->extArgv[i]) == 0,
            return -1, "Invalid by-user ext arg.");
        size_t extArgLen = strlen(args->extArgv[i]);
        BEGET_ERROR_CHECK(extArgLen < SIZE_MAX && valueLen <= SIZE_MAX - extArgLen - 1,
            return -1, "By-user control value length overflow.");
        valueLen += extArgLen + 1;
    }
    if (valueLen >= size) {
        BEGET_LOGE("By-user control value too long: %zu, max %zu.", valueLen, size - 1);
        return -1;
    }

    ret = snprintf_s(buffer, size, size - 1, "%s|%s", args->serviceName, userId);
    if (ret <= 0) {
        BEGET_LOGE("Failed to format by-user control value.");
        return -1;
    }
    size_t used = strlen(buffer);
    for (int i = 0; i < args->extArgc; i++) {
        ret = snprintf_s(buffer + used, size - used, size - used - 1, "|%s", args->extArgv[i]);
        if (ret < 0 || (size_t)ret >= size - used) {
            BEGET_LOGE("Failed to append by-user ext arg within max length %zu.", size - 1);
            return -1;
        }
        used = strlen(buffer);
    }
    return 0;
}
#endif

static int StartProcess(const char *name, const char *extArgv[], int extArgc)
{
    BEGET_ERROR_CHECK(name != NULL, return -1, "Service name is null.");
    int extraArg = 0;
    if ((extArgv != NULL) && (extArgc > 0)) {
        BEGET_LOGV("Start service by extra args");
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
            BEGET_LOGE("failed cat name");
            return -1;
        }
        for (int j = 0; j < extArgc; j++) {
            ret = strncat_s(nameValue, len, "|", 1);
            if (ret == 0) {
                ret = strncat_s(nameValue, len, extArgv[j], strlen(extArgv[j]));
            }
            if (ret != 0) {
                free(nameValue);
                BEGET_LOGE("failed cat name");
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

static int TermProcess(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    return SystemSetParameter("ohos.ctl.term", serviceName);
}

#ifdef SUPPORT_SA_MULTI_USER
static int SetByUserProcessParam(const char *paramName, const ByUserCtrlValueArgs *args)
{
    BEGET_ERROR_CHECK(paramName != NULL, return -1, "By-user control param name is null.");
    char value[SERVICE_CTRL_BY_USER_VALUE_MAX] = {0};
    int ret = BuildByUserCtrlValue(value, sizeof(value), args);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to build by-user control value.");
    return SystemSetParameter(paramName, value);
}

static int StartProcessByUserId(const char *serviceName, int32_t userId, const char *extArgv[], int extArgc)
{
#ifdef OHOS_LITE
    (void)serviceName;
    (void)userId;
    (void)extArgv;
    (void)extArgc;
    BEGET_LOGE("Service start by userId is unsupported on Lite.");
    return -1;
#else
    ByUserCtrlValueArgs args = {serviceName, userId, extArgv, extArgc};
    return SetByUserProcessParam(SERVICE_CTRL_BY_USER_START, &args);
#endif
}

static int StopProcessByUserId(const char *serviceName, int32_t userId)
{
#ifdef OHOS_LITE
    (void)serviceName;
    (void)userId;
    BEGET_LOGE("Service stop by userId is unsupported on Lite.");
    return -1;
#else
    ByUserCtrlValueArgs args = {serviceName, userId, NULL, 0};
    return SetByUserProcessParam(SERVICE_CTRL_BY_USER_STOP, &args);
#endif
}
#endif

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

#ifdef SUPPORT_SA_MULTI_USER
static int GetByUserProcessInfo(const char *serviceName, int32_t userId, char *nameBuffer,
    char *valueBuffer, ServiceStatus status)
{
    if (snprintf_s(nameBuffer, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "%s.%s.userid.%d",
        STARTUP_SERVICE_CTL, serviceName, userId) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    if (snprintf_s(valueBuffer, MAX_INT_LEN, MAX_INT_LEN - 1, "%d", (int)status) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return -1;
    }
    return 0;
}

int ServiceWaitForStatusByUserId(const char *serviceName, int32_t userId, ServiceStatus status,
    int waitTimeout)
{
#ifdef OHOS_LITE
    (void)serviceName;
    (void)userId;
    (void)status;
    (void)waitTimeout;
    BEGET_LOGE("Service wait by userId is unsupported on Lite.");
    return -1;
#else
    BEGET_ERROR_CHECK(ValidateByUserServiceName(serviceName) == 0, return -1, "Invalid by-user service name.");
    BEGET_ERROR_CHECK(userId >= 0 && waitTimeout >= 0, return -1, "Invalid by-user wait args.");
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    char value[MAX_INT_LEN] = {0};
    int ret = GetByUserProcessInfo(serviceName, userId, paramName, value, status);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to get by-user param info.");
    return (SystemWaitParameter(paramName, value, waitTimeout) != 0) ? -1 : 0;
#endif
}
#endif

static int RestartProcess(const char *serviceName, const char *extArgv[], int extArgc)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    ServiceStatus status = SERVICE_IDLE;
    if (GetCurrentServiceStatus(serviceName, &status) != 0) {
        BEGET_LOGE("Get service status failed.\n");
        return -1;
    }
    BEGET_LOGE("Process service %s status: %d ", serviceName, status);
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
        case TERM:
            ret = TermProcess(serviceName);
            break;
        default:
            BEGET_LOGE("Set service %s action %d error", serviceName, action);
            ret = -1;
            break;
    }
    return ret;
}

#ifdef SUPPORT_SA_MULTI_USER
int ServiceControlWithExtraByUserId(const char *serviceName, int action, int32_t userId,
    const char *extArgv[], int extArgc)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    BEGET_ERROR_CHECK(userId >= 0, return -1, "Invalid by-user control userId.");
    int ret = 0;
    switch (action) {
        case START:
            ret = StartProcessByUserId(serviceName, userId, extArgv, extArgc);
            break;
        case STOP:
            ret = StopProcessByUserId(serviceName, userId);
            break;
        default:
            BEGET_LOGE("Set service %s user %d action %d unsupported", serviceName, userId, action);
            ret = -1;
            break;
    }
    return ret;
}
#endif

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
    BEGET_ERROR_CHECK(ret == 0, return -1, "failed get param info.");
    return (SystemWaitParameter(paramName, value, waitTimeout) != 0) ? -1 : 0;
}

int ServiceSetReady(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    char value[MAX_INT_LEN] = {0};
    int ret = GetProcessInfo(serviceName, paramName, value, SERVICE_READY);
    BEGET_ERROR_CHECK(ret == 0, return -1, "failed get param info.");
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
    if (snprintf_s(value, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "%s|%llu", serviceName, timeout) == -1) {
        BEGET_LOGE("failed build parameter value");
        return -1;
    }
    return SystemSetParameter("ohos.servicectrl.timer_start", value);
}

int StopServiceTimer(const char *serviceName)
{
    BEGET_ERROR_CHECK(serviceName != NULL, return -1, "Service name is null.");
    return SystemSetParameter("ohos.servicectrl.timer_stop", serviceName);
}
