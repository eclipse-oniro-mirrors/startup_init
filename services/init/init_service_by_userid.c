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

#include "init_service_by_userid.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "param/init_param.h"
#include "securec.h"
#include "service_control.h"
#include "init_utils.h"
#if defined(ENABLE_HOOK_MGR)
#include "bootstage.h"
#include "hookmgr.h"
#endif
#include "init_hisysevent.h"

#define BY_USER_CTRL_VALUE_MAX PARAM_VALUE_LEN_MAX
#define BY_USER_SERVICE_NAME_MAX 54
#define BY_USER_EXT_ARG_MAX 4
#define BY_USER_EXT_ARG_LEN_MAX PARAM_VALUE_LEN_MAX
#define BY_USER_PARAM_KEY_LEN_MAX 128
#define BY_USER_PARAM_VALUE_LEN_MAX 32
#define BY_USER_CHILD_EXEC_FAILURE_EXIT_CODE 126
#define CATEGORY_SEG3_OFFSET 768
#define CATEGORY_MASK 0xff

#ifndef BY_USER_STAT
#define BY_USER_STAT stat
#endif
#ifndef BY_USER_SYSTEM_WRITE_PARAM
#define BY_USER_SYSTEM_WRITE_PARAM SystemWriteParam
#endif
#ifndef MAX_SERVICE_NAME_LEN
#define MAX_SERVICE_NAME_LEN BY_USER_SERVICE_NAME_MAX
#endif
#ifndef MAX_INSTANCE_KEY_LEN
#define MAX_INSTANCE_KEY_LEN (MAX_SERVICE_NAME_LEN + 16)
#endif
#ifndef MAX_MCS_LABEL_LEN
#define MAX_MCS_LABEL_LEN 128
#endif

typedef struct {
    char serviceName[BY_USER_SERVICE_NAME_MAX];
    int32_t userId;
    int extArgc;
    char extArgs[BY_USER_EXT_ARG_MAX][BY_USER_EXT_ARG_LEN_MAX];
} ByUserCtrlArgs;

struct ServiceByUserId {
    struct ServiceByUserId *next;
    const Service *templateService;
    char serviceName[MAX_SERVICE_NAME_LEN];
    char instanceKey[MAX_INSTANCE_KEY_LEN];
    int32_t userId;
    int pid;
    int status;
    InitErrno lastErrno;
    char mcs[MAX_MCS_LABEL_LEN];
};

static ServiceByUserId *g_serviceByUserIdHead = NULL;

static ServiceByUserId *FindServiceByUserId(const char *serviceName, int32_t userId);
static ServiceByUserId *GetOrCreateServiceByUserId(const Service *templateService, const char *serviceName,
    int32_t userId);
static void RemoveServiceByUserIdInstance(ServiceByUserId *target);
static int StartServiceInstanceByUserId(const ByUserCtrlArgs *args);

static ServiceArgs *BuildRuntimePathArgs(const Service *service, const ByUserCtrlArgs *args,
    ServiceArgs *runtimePathArgs)
{
    if (service == NULL || args == NULL || runtimePathArgs == NULL || service->pathArgs.count <= 0 ||
        service->pathArgs.argv == NULL || service->pathArgs.argv[0] == NULL) {
        return NULL;
    }
    int baseArgc = 0;
    while (baseArgc < service->pathArgs.count && service->pathArgs.argv[baseArgc] != NULL) {
        baseArgc++;
    }

    runtimePathArgs->count = baseArgc + args->extArgc;
    runtimePathArgs->argv = (char **)calloc(runtimePathArgs->count + 1, sizeof(char *));
    if (runtimePathArgs->argv == NULL) {
        runtimePathArgs->count = 0;
        return NULL;
    }

    int argc = 0;
    for (; argc < baseArgc; argc++) {
        runtimePathArgs->argv[argc] = strdup(service->pathArgs.argv[argc]);
        if (runtimePathArgs->argv[argc] == NULL) {
            FreeStringVector(runtimePathArgs->argv, runtimePathArgs->count);
            runtimePathArgs->argv = NULL;
            runtimePathArgs->count = 0;
            return NULL;
        }
    }
    for (int extArgc = 0; extArgc < args->extArgc; extArgc++) {
        runtimePathArgs->argv[argc + extArgc] = strdup(args->extArgs[extArgc]);
        if (runtimePathArgs->argv[argc + extArgc] == NULL) {
            FreeStringVector(runtimePathArgs->argv, runtimePathArgs->count);
            runtimePathArgs->argv = NULL;
            runtimePathArgs->count = 0;
            return NULL;
        }
    }
    runtimePathArgs->argv[runtimePathArgs->count] = NULL;
    return runtimePathArgs;
}

static void FreeRuntimePathArgs(ServiceArgs *runtimePathArgs)
{
    if (runtimePathArgs != NULL && runtimePathArgs->argv != NULL) {
        FreeStringVector(runtimePathArgs->argv, runtimePathArgs->count);
        runtimePathArgs->argv = NULL;
        runtimePathArgs->count = 0;
    }
}

static char *SplitNextToken(char **cursor)
{
    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }

    char *token = *cursor;
    char *separator = strchr(token, '|');
    if (separator == NULL) {
        *cursor = NULL;
        return token;
    }
    *separator = '\0';
    *cursor = separator + 1;
    return token;
}

static int CopyToken(char *dest, size_t size, const char *token)
{
    if (dest == NULL || size == 0 || token == NULL || token[0] == '\0') {
        return -1;
    }

    int ret = strcpy_s(dest, size, token);
    if (ret != EOK) {
        return -1;
    }
    return 0;
}

static int CopyExtToken(char *dest, size_t size, const char *token)
{
    if (dest == NULL || size == 0 || token == NULL) {
        return -1;
    }

    int ret = strcpy_s(dest, size, token);
    if (ret != EOK) {
        return -1;
    }
    return 0;
}

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

static int DecodeUserId(const char *token, int32_t *userId)
{
    if (token == NULL || token[0] == '\0' || userId == NULL) {
        return -1;
    }

    errno = 0;
    char *end = NULL;
    long value = strtol(token, &end, 10);
    if (errno != 0 || end == token || *end != '\0' || value < 0 || value > INT32_MAX) {
        return -1;
    }
    *userId = (int32_t)value;
    return 0;
}

static int ParseByUserCtrlValue(const char *value, ByUserCtrlArgs *args)
{
    if (value == NULL || args == NULL || value[0] == '\0' || strlen(value) >= BY_USER_CTRL_VALUE_MAX) {
        return -1;
    }

    char buffer[BY_USER_CTRL_VALUE_MAX] = {0};
    if (CopyToken(buffer, sizeof(buffer), value) != 0) {
        return -1;
    }

    ByUserCtrlArgs parsed = {0};
    char *cursor = buffer;
    char *token = SplitNextToken(&cursor);
    if (CopyToken(parsed.serviceName, sizeof(parsed.serviceName), token) != 0 ||
        HasByUserCtrlBlankChar(parsed.serviceName) != 0) {
        return -1;
    }

    token = SplitNextToken(&cursor);
    if (DecodeUserId(token, &parsed.userId) != 0) {
        return -1;
    }

    while ((token = SplitNextToken(&cursor)) != NULL) {
        if (parsed.extArgc >= BY_USER_EXT_ARG_MAX ||
            HasByUserCtrlBlankChar(token) != 0 ||
            CopyExtToken(parsed.extArgs[parsed.extArgc], sizeof(parsed.extArgs[parsed.extArgc]), token) != 0) {
            return -1;
        }
        parsed.extArgc++;
    }

    *args = parsed;
    return 0;
}

static int BuildInstanceKey(char *key, size_t size, const char *serviceName, int32_t userId)
{
    if (key == NULL || size == 0 || serviceName == NULL) {
        return -1;
    }
    int ret = snprintf_s(key, size, size - 1, "%s_u%d", serviceName, userId);
    if (ret <= 0) {
        return -1;
    }
    return 0;
}

static int InitRuntimeService(Service *runtimeService, const ServiceByUserId *instance,
    const ServiceArgs *pathArgs, int pid)
{
    if (runtimeService == NULL || instance == NULL || instance->templateService == NULL) {
        return -1;
    }

    *runtimeService = *(instance->templateService);
    runtimeService->name = (char *)instance->instanceKey;
    runtimeService->mcs = instance->mcs;
    runtimeService->pid = pid;
    if (pathArgs != NULL) {
        runtimeService->pathArgs = *pathArgs;
    }
    return 0;
}

static int BuildByUserStatusKey(char *buffer, size_t size, const char *serviceName, int32_t userId)
{
    if (buffer == NULL || size == 0 || serviceName == NULL || userId < 0) {
        return -1;
    }
    int ret = snprintf_s(buffer, size, size - 1, "%s.%s.userid.%d", STARTUP_SERVICE_CTL, serviceName, userId);
    if (ret <= 0) {
        INIT_LOGE("Failed to build by-user status key");
        return -1;
    }
    return 0;
}

static int BuildByUserPidKey(char *buffer, size_t size, const char *serviceName, int32_t userId)
{
    if (buffer == NULL || size == 0 || serviceName == NULL || userId < 0) {
        return -1;
    }
    int ret = snprintf_s(buffer, size, size - 1, "%s.%s.userid.%d.pid", STARTUP_SERVICE_CTL, serviceName, userId);
    if (ret <= 0) {
        INIT_LOGE("Failed to build by-user pid key");
        return -1;
    }
    return 0;
}

static int ComputeMcsByUserId(int32_t userId, char *buffer, size_t size)
{
    INIT_ERROR_CHECK(userId >= 0 && buffer != NULL && size > 0, return -1, "Invalid MCS input");
    int category = CATEGORY_SEG3_OFFSET + (userId & CATEGORY_MASK);
    int ret = snprintf_s(buffer, size, size - 1, "s0:c%d", category);
    if (ret <= 0) {
        INIT_LOGE("Failed to compute MCS label");
        return -1;
    }
    return 0;
}

static void ReportByUserServiceStartInfo(Service *service, int64_t pid)
{
    if (service == NULL) {
        return;
    }
    bool isSaspawn = false;
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
    isSaspawn = ((service->attribute & SERVICE_ATTR_SASPAWN) == SERVICE_ATTR_SASPAWN);
#endif
    if (isSaspawn) {
        ReportServiceStart(service->name, pid, SERVICES_EXIT_INFO_IS_SASPAWN);
    } else if (!IsOnDemandService(service)) {
        ReportServiceStart(service->name, pid, SERVICES_EXIT_INFO_NOT_SASPAWN);
    }
}

static int WriteByUserServiceState(ServiceByUserId *instance, int status)
{
    if (instance == NULL) {
        return -1;
    }

    instance->status = status;
    char statusKey[BY_USER_PARAM_KEY_LEN_MAX] = {0};
    char pidKey[BY_USER_PARAM_KEY_LEN_MAX] = {0};
    char value[BY_USER_PARAM_VALUE_LEN_MAX] = {0};
    if (BuildByUserStatusKey(statusKey, sizeof(statusKey), instance->serviceName, instance->userId) != 0 ||
        BuildByUserPidKey(pidKey, sizeof(pidKey), instance->serviceName, instance->userId) != 0) {
        return -1;
    }

    int ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "%d", status);
    if (ret <= 0) {
        INIT_LOGE("Failed to build by-user status value");
        return -1;
    }
    ret = BY_USER_SYSTEM_WRITE_PARAM(statusKey, value);
    INIT_CHECK_ONLY_ELOG(ret == 0, "Failed to write by-user status %s=%s ret %d", statusKey, value, ret);
    int statusRet = ret;

    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "%d", (instance->pid <= 0) ? 0 : instance->pid);
    if (ret <= 0) {
        INIT_LOGE("Failed to build by-user pid value");
        return -1;
    }
    ret = BY_USER_SYSTEM_WRITE_PARAM(pidKey, value);
    INIT_CHECK_ONLY_ELOG(ret == 0, "Failed to write by-user pid %s=%s ret %d", pidKey, value, ret);
    return (statusRet != 0) ? statusRet : ret;
}

static ServiceByUserId *FindServiceByUserId(const char *serviceName, int32_t userId)
{
    if (serviceName == NULL) {
        return NULL;
    }

    ServiceByUserId *instance = g_serviceByUserIdHead;
    while (instance != NULL) {
        if (strcmp(instance->serviceName, serviceName) == 0 && instance->userId == userId) {
            return instance;
        }
        instance = instance->next;
    }
    return NULL;
}

static ServiceByUserId *GetOrCreateServiceByUserId(const Service *templateService, const char *serviceName,
    int32_t userId)
{
    if (templateService == NULL || serviceName == NULL || userId < 0) {
        return NULL;
    }

    ServiceByUserId *instance = FindServiceByUserId(serviceName, userId);
    if (instance != NULL) {
        return instance;
    }

    instance = (ServiceByUserId *)calloc(1, sizeof(ServiceByUserId));
    if (instance == NULL) {
        INIT_LOGE("Create service %s by user id %d instance failed", serviceName, userId);
        return NULL;
    }

    if (CopyToken(instance->serviceName, sizeof(instance->serviceName), serviceName) != 0 ||
        BuildInstanceKey(instance->instanceKey, sizeof(instance->instanceKey), serviceName, userId) != 0) {
        free(instance);
        return NULL;
    }

    instance->templateService = templateService;
    instance->userId = userId;
    instance->pid = -1;
    instance->status = SERVICE_IDLE;
    instance->lastErrno = INIT_OK;
    instance->next = g_serviceByUserIdHead;
    g_serviceByUserIdHead = instance;
    return instance;
}

static int ValidateByUserTemplate(const Service *service)
{
    if (service == NULL) {
        return -1;
    }
    bool unsupported = !IsOnDemandService(service) ||
        service->socketCfg != NULL || service->fileCfg != NULL ||
        service->fds != NULL || service->fdCount != 0 ||
        service->writePidArgs.count != 0 || service->restartArg != NULL ||
        service->serviceJobs.jobsName[JOB_PRE_START] != NULL ||
        (service->attribute & (SERVICE_ATTR_CRITICAL | SERVICE_ATTR_PERIOD)) != 0;
    if (unsupported) {
        INIT_LOGE("Service %s by-user template unsupported: ondemand:%d socket:%d file:%d "
            "fds:%d fdCount:%zu writepid:%d restart:%d prestart:%d critical:%d period:%d",
            service->name, IsOnDemandService(service), service->socketCfg != NULL, service->fileCfg != NULL,
            service->fds != NULL, service->fdCount, service->writePidArgs.count,
            service->restartArg != NULL, service->serviceJobs.jobsName[JOB_PRE_START] != NULL,
            (service->attribute & SERVICE_ATTR_CRITICAL) != 0,
            (service->attribute & SERVICE_ATTR_PERIOD) != 0);
        return -1;
    }
    return 0;
}

static int IsByUserServiceInvalid(ServiceByUserId *instance, const Service *service, const ServiceArgs *pathArgs)
{
    if (instance == NULL || service == NULL || pathArgs == NULL || pathArgs->count <= 0) {
        return -1;
    }
    if (service->attribute & SERVICE_ATTR_INVALID) {
        INIT_LOGE("ServiceStart invalid:%s", service->name);
        instance->lastErrno = INIT_EPARAMETER;
        return -1;
    }

    struct stat pathStat = {0};
    if (BY_USER_STAT(pathArgs->argv[0], &pathStat) != 0) {
        instance->lastErrno = INIT_EPATH;
        INIT_LOGE("ServiceStart pathArgs invalid, please check %s,%s", service->name, pathArgs->argv[0]);
        return -1;
    }
    return 0;
}

static void RunByUserChildProcess(ServiceByUserId *instance, const Service *service,
    const Service *runtimeService, ServiceArgs *pathArgs)
{
    Service childService = *runtimeService;
    childService.name = service->name;
    RunChildProcessByUserId(&childService, pathArgs, instance->userId);
    INIT_LOGE("RunChildProcessByUserId returned for service %s user %d errno %d",
        service->name, instance->userId, errno);
    _exit(BY_USER_CHILD_EXEC_FAILURE_EXIT_CODE);
}

static int CompleteByUserProcessStart(ServiceByUserId *instance, Service *runtimeService, int pid)
{
    const Service *service = instance->templateService;
    INIT_LOGI("ServiceStart by user id started %s user %d pid %d", service->name, instance->userId, pid);
    runtimeService->pid = pid;
    instance->pid = pid;
    instance->lastErrno = INIT_OK;
    ReportByUserServiceStartInfo(runtimeService, pid);
    (void)ProcessServiceAdd(runtimeService);
    return WriteByUserServiceState(instance, SERVICE_STARTED);
}

static int StartInstanceProcessWithPathArgs(ServiceByUserId *instance, ServiceArgs *pathArgs)
{
    if (instance == NULL || instance->templateService == NULL) {
        return -1;
    }
    const Service *service = instance->templateService;
    if (instance->pid > 0) {
        return SERVICE_SUCCESS;
    }

    if (pathArgs == NULL) {
        instance->lastErrno = INIT_EPARAMETER;
        return SERVICE_FAILURE;
    }
    if (ComputeMcsByUserId(instance->userId, instance->mcs, sizeof(instance->mcs)) != 0) {
        instance->lastErrno = INIT_EPARAMETER;
        instance->mcs[0] = '\0';
        return SERVICE_FAILURE;
    }
    if (IsByUserServiceInvalid(instance, service, pathArgs) != 0) {
        return SERVICE_FAILURE;
    }

    Service runtimeService = {0};
    if (InitRuntimeService(&runtimeService, instance, pathArgs, -1) != 0) {
        instance->lastErrno = INIT_EPARAMETER;
        return SERVICE_FAILURE;
    }
    int pid = fork();
    if (pid == 0) {
        RunByUserChildProcess(instance, service, &runtimeService, pathArgs);
    }
    if (pid < 0) {
        INIT_LOGE("ServiceStart error failed to fork %d, %s", errno, service->name);
        instance->lastErrno = INIT_EFORK;
        return SERVICE_FAILURE;
    }
    return CompleteByUserProcessStart(instance, &runtimeService, pid);
}

static int StartInstanceProcess(ServiceByUserId *instance, const ByUserCtrlArgs *args)
{
    if (instance == NULL || args == NULL || instance->templateService == NULL) {
        return -1;
    }

    ServiceArgs runtimePathArgs = {0};
    ServiceArgs *pathArgs = BuildRuntimePathArgs(instance->templateService, args, &runtimePathArgs);
    if (pathArgs == NULL) {
        instance->lastErrno = INIT_EPARAMETER;
        return SERVICE_FAILURE;
    }

    int ret = StartInstanceProcessWithPathArgs(instance, pathArgs);
    FreeRuntimePathArgs(&runtimePathArgs);
    return ret;
}

static int StopInstanceProcess(ServiceByUserId *instance, int signal)
{
    if (instance == NULL) {
        return -1;
    }
    (void)WriteByUserServiceState(instance, SERVICE_STOPPING);
    const Service *service = instance->templateService;
    if (service != NULL && service->serviceJobs.jobsName[JOB_ON_STOP] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_ON_STOP]);
    }
    if (instance->pid <= 0) {
        return WriteByUserServiceState(instance, SERVICE_STOPPED);
    }

    int pid = instance->pid;
    if (kill(pid, signal) != 0) {
        if (errno == ESRCH) {
            INIT_LOGW("service %s by user id %d pid %d already exited.",
                instance->serviceName, instance->userId, pid);
            instance->pid = -1;
            (void)WriteByUserServiceState(instance, SERVICE_STOPPED);
            return SERVICE_SUCCESS;
        }
        INIT_LOGE("stop service %s by user id %d pid %d failed! err %d.",
            instance->serviceName, instance->userId, pid, errno);
        return SERVICE_FAILURE;
    }
    if (service != NULL) {
        Service runtimeService = {0};
        if (InitRuntimeService(&runtimeService, instance, NULL, pid) == 0) {
            (void)ProcessServiceDied(&runtimeService);
        }
    }
    return SERVICE_SUCCESS;
}

static void FreeServiceByUserIdInstance(ServiceByUserId *instance)
{
    if (instance == NULL) {
        return;
    }
    free(instance);
}

static void RemoveServiceByUserIdInstance(ServiceByUserId *target)
{
    if (target == NULL) {
        return;
    }
    ServiceByUserId **link = &g_serviceByUserIdHead;
    while (*link != NULL) {
        ServiceByUserId *instance = *link;
        if (instance == target) {
            *link = instance->next;
            FreeServiceByUserIdInstance(instance);
            return;
        }
        link = &instance->next;
    }
}

void StopAllServiceByUserIdInstances(void)
{
    ServiceByUserId *instance = g_serviceByUserIdHead;
    while (instance != NULL) {
        ServiceByUserId *next = instance->next;
        INIT_LOGI("StopAllServices stop by-user service %s user %d", instance->serviceName, instance->userId);
        (void)StopInstanceProcess(instance, GetKillServiceSig(instance->serviceName));
        if (instance->pid <= 0) {
            RemoveServiceByUserIdInstance(instance);
        }
        instance = next;
    }
}

static int DecodeByUserActionArgs(const char *encodedValue, const char *action, ByUserCtrlArgs *args)
{
    if (ParseByUserCtrlValue(encodedValue, args) != 0) {
        INIT_LOGE("%s service by user id failed, invalid value", action);
        return -1;
    }
    return 0;
}

static int StartServiceInstanceByUserId(const ByUserCtrlArgs *args)
{
    if (args == NULL) {
        return -1;
    }

    Service *templateService = GetServiceByName(args->serviceName);
    if (templateService == NULL) {
        INIT_LOGE("Cannot find service template %s by user id %d", args->serviceName, args->userId);
        return -1;
    }
    if (ValidateByUserTemplate(templateService) != 0) {
        return -1;
    }
    ServiceByUserId *instance = GetOrCreateServiceByUserId(templateService, args->serviceName, args->userId);
    if (instance == NULL) {
        INIT_LOGE("start service %s by user id %d failed, no instance", args->serviceName, args->userId);
        return -1;
    }
    INIT_LOGI("start service %s by user id %d ext argc %d", args->serviceName, args->userId, args->extArgc);
    return StartInstanceProcess(instance, args);
}

static int StopServiceInstanceByUserId(const ByUserCtrlArgs *args)
{
    if (args == NULL) {
        return -1;
    }

    ServiceByUserId *instance = FindServiceByUserId(args->serviceName, args->userId);
    if (instance == NULL) {
        INIT_LOGE("stop service %s by user id %d failed, no instance", args->serviceName, args->userId);
        return -1;
    }
    INIT_LOGI("stop service %s by user id %d", args->serviceName, args->userId);
    int ret = StopInstanceProcess(instance, GetKillServiceSig(instance->serviceName));
    if (ret == SERVICE_SUCCESS && instance->pid <= 0) {
        RemoveServiceByUserIdInstance(instance);
    }
    return ret;
}

ServiceByUserId *GetServiceByUserPid(pid_t pid)
{
    if (pid <= 0) {
        return NULL;
    }
    ServiceByUserId *instance = g_serviceByUserIdHead;
    while (instance != NULL) {
        if (instance->pid == pid) {
            return instance;
        }
        instance = instance->next;
    }
    return NULL;
}

void ReportServiceByUserExit(ServiceByUserId *instance, pid_t pid, int procStat)
{
    if (instance == NULL || instance->templateService == NULL) {
        return;
    }
    bool isSaspawn = false;
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
    const Service *service = instance->templateService;
    isSaspawn = ((service->attribute & SERVICE_ATTR_SASPAWN) == SERVICE_ATTR_SASPAWN);
#endif
    if (WIFSIGNALED(procStat)) {
        ReportChildProcessExit(instance->instanceKey, pid, WTERMSIG(procStat),
            isSaspawn ? SERVICES_EXIT_INFO_IS_SASPAWN : SERVICES_EXIT_INFO_NOT_SASPAWN);
    } else if (WIFEXITED(procStat) && isSaspawn) {
        ReportChildProcessExit(instance->instanceKey, pid, WEXITSTATUS(procStat), SERVICES_EXIT_INFO_IS_SASPAWN);
    }
}

int ReapServiceByUserId(pid_t pid, int procStat)
{
    ServiceByUserId *instance = GetServiceByUserPid(pid);
    if (instance == NULL) {
        return -1;
    }

    INIT_LOGI("ServiceReap by user id info %s user %d pid %d.", instance->serviceName, instance->userId, pid);
    if (WIFEXITED(procStat)) {
        instance->lastErrno = WEXITSTATUS(procStat);
    }
    const Service *service = instance->templateService;
    bool stopRequested = (instance->status == SERVICE_STOPPING);
    if (service != NULL && !stopRequested) {
        Service runtimeService = {0};
        if (InitRuntimeService(&runtimeService, instance, NULL, pid) == 0) {
            (void)ProcessServiceDied(&runtimeService);
        }
    }
    if (instance->pid == pid) {
        instance->pid = -1;
    }
    (void)WriteByUserServiceState(instance, SERVICE_STOPPED);
    INIT_LOGI("ServiceReap by user id stopped %s user %d, release instance",
        instance->serviceName, instance->userId);
    RemoveServiceByUserIdInstance(instance);
    return 0;
}

void StartServiceByUserId(const char *encodedValue)
{
    ByUserCtrlArgs args = {0};
    if (DecodeByUserActionArgs(encodedValue, "start", &args) == 0) {
        (void)StartServiceInstanceByUserId(&args);
    }
}

void StopServiceByUserId(const char *encodedValue)
{
    ByUserCtrlArgs args = {0};
    if (DecodeByUserActionArgs(encodedValue, "stop", &args) == 0) {
        (void)StopServiceInstanceByUserId(&args);
    }
}
