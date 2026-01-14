/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include "init_service.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include "param_manager.h"
#endif

#include "init_group_manager.h"
#include "init.h"
#include "init_log.h"
#include "init_param.h"
#include "init_utils.h"
#include "securec.h"
#ifdef INIT_SUPPORT_ACCESS_TOKEN
#include "token_setproc.h"
#include "nativetoken_kit.h"
#endif
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "service_control.h"

#define MIN_IMPORTANT_LEVEL (-20)
#define MAX_IMPORTANT_LEVEL 19
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
typedef int(*FuncType)(int, char *argv[]);
#endif

static bool g_enableSandbox = false;

static void WriteOomScoreAdjToService(Service *service)
{
    if (service == NULL) {
        return;
    }
    if (IsOnDemandService(service)) {
        char pidAdjPath[30];
        const char* content = "-900";
        int len = sprintf_s(pidAdjPath, sizeof(pidAdjPath), "/proc/%d/oom_score_adj", service->pid);
        if (len <= 0) {
            INIT_LOGE("Service(%s): format pidAdjPath (pid:%d) failed.", service->name, service->pid);
            return;
        }
        int fd = open(pidAdjPath, O_RDWR);
        if (fd < 0) {
            INIT_LOGE("Service(%s): open path %s failed.", service->name, pidAdjPath);
            return;
        }
        int ret = write(fd, content, strlen(content));
        if (ret < 0) {
            INIT_LOGE("Service(%s): write content(%s) to path(%s) failed.", service->name, content, pidAdjPath);
        }
        close(fd);
    }
}

void NotifyServiceChange(Service *service, int status)
{
    INIT_LOGV("Notify service %s change from %d to %d", service->name, service->status, status);
    service->status = status;
    INIT_CHECK(status != SERVICE_IDLE, return);
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    int ret = snprintf_s(paramName, sizeof(paramName), sizeof(paramName) - 1,
        "%s.%s", STARTUP_SERVICE_CTL, service->name);
    INIT_ERROR_CHECK(ret > 0, return, "Failed to format service name %s.", service->name);
    char statusStr[MAX_INT_LEN] = {0};
    ret = snprintf_s(statusStr, sizeof(statusStr), sizeof(statusStr) - 1, "%d", status);
    INIT_ERROR_CHECK(ret > 0, return, "Failed to format service status %s.", service->name);
    SystemWriteParam(paramName, statusStr);

    // write pid
    ret = snprintf_s(paramName, sizeof(paramName), sizeof(paramName) - 1,
        "%s.%s.pid", STARTUP_SERVICE_CTL, service->name);
    INIT_ERROR_CHECK(ret > 0, return, "Failed to format service pid name %s.", service->name);
    ret = snprintf_s(statusStr, sizeof(statusStr), sizeof(statusStr) - 1,
        "%d", (service->pid == -1) ? 0 : service->pid);
    INIT_ERROR_CHECK(ret > 0, return, "Failed to format service pid %s.", service->name);
    if (status == SERVICE_STARTED) {
        WriteOomScoreAdjToService(service);
    }
    SystemWriteParam(paramName, statusStr);
}

int IsForbidden(const char *fieldStr)
{
    UNUSED(fieldStr);
    return 0;
}

int SetImportantValue(Service *service, const char *attrName, int value, int flag)
{
    UNUSED(attrName);
    UNUSED(flag);
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "Set service attr failed! null ptr.");
    if (value >= MIN_IMPORTANT_LEVEL && value <= MAX_IMPORTANT_LEVEL) { // -20~19
        service->attribute |= SERVICE_ATTR_IMPORTANT;
        service->importance = value;
    } else {
        INIT_LOGE("Importance level = %d, is not between -20 and 19, error", value);
        return SERVICE_FAILURE;
    }
    return SERVICE_SUCCESS;
}

#ifdef INIT_FEATURE_SUPPORT_SASPAWN
int InitServiceBySaspawn(Service *service, const ServiceArgs *pathArgs)
{
    int ret = 0;
    ret = memset_s(g_procProcessName->longProcName, g_procProcessName->longProcNameLen,
        0, g_procProcessName->longProcNameLen);
    if (ret != EOK) {
        INIT_LOGE("saspawn memset_s failed %d", errno);
        return SERVICE_FAILURE;
    }
    ret = strncpy_s(g_procProcessName->longProcName, g_procProcessName->longProcNameLen,
        service->name, strlen(service->name));
    if (ret != EOK) {
        INIT_LOGE("saspawn strncpy_s failed %d", errno);
        return SERVICE_FAILURE;
    }
    void* handle = dlopen(SERVICES_SASPAWN_LIBRARY_NAME, RTLD_LAZY);
    if (handle != NULL) {
        FuncType startSA = (FuncType)dlsym(handle, SERVICES_SASPAWN_FUNCTION_NAME);
        if (startSA == NULL) {
            INIT_LOGE("saspawn dlsym error: %s", dlerror());
            return SERVICE_FAILURE;
            FreeInitResource();
        }
        int argvValue = pathArgs->count - 1;
        ret = startSA(argvValue, pathArgs->argv);
        INIT_LOGI("saspawn complete, ret = %d", ret);
        return SERVICE_SUCCESS;
    } else {
        INIT_LOGE("saspawn dlopen error: %s", dlerror());
        return SERVICE_FAILURE;
    }
}

static void CloseFileResource(void)
{
    DIR *dir = opendir(SERVICES_PROC_SELF_FD);
    if (!dir) {
        INIT_LOGE("Open fd directory failed.");
        return;
    }
    INIT_LOGI("Saspawn free file descriptors.");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char *endptr = NULL;
        long fd_val = strtol(entry->d_name, &endptr, STRTOL_BASE);
        if (*endptr != '\0' || fd_val < 0 || fd_val > INT_MAX) {
            INIT_LOGE("Invalid fd entry: %s", entry->d_name);
            continue;
        }
        int fd = (int)fd_val;
        if (fd == SERVICES_STANDARD_INPUT || fd == SERVICES_STANDARD_OUTPUT || fd == SERVICES_STANDARD_ERROR) {
            continue;
        }

        char path[SERVICES_STR_LEN_MAX] = { 0 };
        int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "%s/%s", SERVICES_PROC_SELF_FD, entry->d_name);
        INIT_ERROR_CHECK(ret > 0, continue, "Path too long for fd: %s.", entry->d_name);

        char link[SERVICES_STR_LEN_MAX] = { 0 };
        int len = readlink(path, link, sizeof(link) - 1);
        if (len < 0) {
            INIT_LOGE("Failed to read symbolic link for path: %s", path);
            continue;
        }
        if (strcmp(link, SERVICES_FILE_PATH_KMSG) == 0) {
            INIT_LOGI("Failed path is kmsg: %s", link);
            continue;
        }
        close(fd);
    }
    closedir(dir);
}

static void ResetSignalResource(void)
{
    if (signal(SIGTERM, SIG_DFL) == SIG_ERR) {
        INIT_LOGE("Resetting SIGTERM handler failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
        INIT_LOGE("Resetting SIGCHLD handler failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    INIT_LOGI("Signal resource reset SIGTERM SIGCHLD ok.");
}

static void FreeInitResource(void)
{
    INIT_LOGI("Start sa resource free.");
    CloseFileResource();
    ResetSignalResource();
}
#endif

int ServiceExec(Service *service, const ServiceArgs *pathArgs)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "Exec service failed! null ptr.");
    INIT_LOGI("ServiceExec %s", service->name);
    INIT_ERROR_CHECK(pathArgs != NULL && pathArgs->count > 0,
        return SERVICE_FAILURE, "Exec service failed! null ptr.");

    if (service->importance != 0) {
        INIT_ERROR_CHECK(setpriority(PRIO_PROCESS, 0, service->importance) == 0,
            service->lastErrno = INIT_EPRIORITY;
            return SERVICE_FAILURE,
            "Service error %d %s, failed to set priority %d.", errno, service->name, service->importance);
    }
    OpenHidebug(service->name);
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
    int isSaspawn = ((service->attribute & SERVICE_ATTR_SASPAWN) == SERVICE_ATTR_SASPAWN);
    if (isSaspawn) {
        int ret = InitServiceBySaspawn(service, pathArgs);
        if (ret == SERVICE_FAILURE) {
            service->lastErrno = INIT_SASPAWN;
            return SERVICE_FAILURE;
        } else {
            return SERVICE_SUCCESS;
        }
    }
#endif
    int isCritical = (service->attribute & SERVICE_ATTR_CRITICAL);
    INIT_ERROR_CHECK(execv(pathArgs->argv[0], pathArgs->argv) == 0,
        service->lastErrno = INIT_EEXEC;
        return errno, "[startup_failed]failed to execv %d %d %s", isCritical, errno, service->name);
    return SERVICE_SUCCESS;
}

int SetAccessToken(const Service *service)
{
#ifdef INIT_SUPPORT_ACCESS_TOKEN
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "service is null");
    return SetSelfTokenID(service->tokenId);
#else
    INIT_LOGE("SetAccessToken is not supported");
    return SERVICE_FAILURE;
#endif
}

void GetAccessToken(void)
{
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        Service *service = node->data.service;
        if (service != NULL) {
            if (service->capsArgs.count == 0) {
                service->capsArgs.argv = NULL;
            }
#ifdef INIT_SUPPORT_ACCESS_TOKEN
            const char *apl = "system_basic";
            if (service->apl != NULL) {
                apl = service->apl;
            }
            NativeTokenInfoParams nativeTokenInfoParams = {
                service->capsArgs.count,
                service->permArgs.count,
                service->permAclsArgs.count,
                (const char **)service->capsArgs.argv,
                (const char **)service->permArgs.argv,
                (const char **)service->permAclsArgs.argv,
                service->name,
                apl,
            };
            uint64_t tokenId = GetAccessTokenId(&nativeTokenInfoParams);
            INIT_CHECK_ONLY_ELOG(tokenId  != 0,
                "gettotkenid failed, service \' %s \'", service->name);
            service->tokenId = tokenId;
#else
            INIT_LOGE("gettotkenid is not supported");
#endif
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
}

void IsEnableSandbox(void)
{
    char value[MAX_BUFFER_LEN] = {0};
    unsigned int len = MAX_BUFFER_LEN;
    if (SystemReadParam("const.sandbox", value, &len) == 0) {
        if (strcmp(value, "enable") == 0) {
            g_enableSandbox = true;
        }
    }
}

int SetServiceEnterSandbox(const Service *service, const char *execPath)
{
    if ((service->attribute & SERVICE_ATTR_WITHOUT_SANDBOX) == SERVICE_ATTR_WITHOUT_SANDBOX) {
        return 0;
    }
    if (g_enableSandbox == false) {
        return 0;
    }
    INIT_ERROR_CHECK(execPath != NULL, return INIT_EPARAMETER, "Service path is null.");
    int ret = 0;
    if (strncmp(execPath, "/system/bin/", strlen("/system/bin/")) == 0) {
        ret = EnterSandbox("system");
    } else if (strncmp(execPath, "/vendor/bin/", strlen("/vendor/bin/")) == 0) {
        ret = EnterSandbox("chipset");
    }
    return ret;
}
