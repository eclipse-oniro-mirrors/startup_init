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

#include "init_group_manager.h"
#include "init.h"
#include "init_log.h"
#include "init_param.h"
#include "init_utils.h"
#include "securec.h"
#include "token_setproc.h"
#include "nativetoken_kit.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "service_control.h"

#define MIN_IMPORTANT_LEVEL (-20)
#define MAX_IMPORTANT_LEVEL 19

static bool g_enableSandbox = false;

void NotifyServiceChange(Service *service, int status)
{
    INIT_LOGV("Notify service %s change from %d to %d", service->name, service->status, status);
    service->status = status;
    INIT_CHECK(status != SERVICE_IDLE, return);
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    int ret = snprintf_s(paramName, sizeof(paramName), sizeof(paramName) - 1,
        "%s.%s", STARTUP_SERVICE_CTL, service->name);
    char statusStr[MAX_INT_LEN] = {0};
    int ret1 = snprintf_s(statusStr, sizeof(statusStr), sizeof(statusStr) - 1, "%d", status);
    if (ret >= 0 && ret1 > 0) {
        SystemWriteParam(paramName, statusStr);
    }
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

int ServiceExec(const Service *service)
{
    INIT_ERROR_CHECK(service != NULL && service->pathArgs.count > 0,
        return SERVICE_FAILURE, "Exec service failed! null ptr.");
    if (service->importance != 0) {
        INIT_ERROR_CHECK(setpriority(PRIO_PROCESS, 0, service->importance) == 0, _exit(0x7f),
            "setpriority failed for %s, importance = %d, err=%d", service->name, service->importance, errno);
    }
    OpenHidebug(service->name);
    // L2 Can not be reset env
    if (service->extraArgs.argv != NULL && service->extraArgs.count > 0) {
        INIT_CHECK_ONLY_ELOG(execv(service->extraArgs.argv[0], service->extraArgs.argv) == 0,
            "service %s execve failed! err %d.", service->name, errno);
    } else {
        INIT_CHECK_ONLY_ELOG(execv(service->pathArgs.argv[0], service->pathArgs.argv) == 0,
            "service %s execve failed! err %d.", service->name, errno);
    }
    return SERVICE_SUCCESS;
}

int SetAccessToken(const Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "service is null");
    int ret = SetSelfTokenID(service->tokenId);
    if (ret != 0) {
        INIT_LOGV("Set service %s token id %lld failed.", service->name, service->tokenId);
        return SERVICE_FAILURE;
    }
    return SERVICE_SUCCESS;
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
                "Get totken id %lld of service \' %s \' failed", tokenId, service->name);
            service->tokenId = tokenId;
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

void SetServiceEnterSandbox(const char *execPath, unsigned int attribute)
{
    if (g_enableSandbox == false) {
        return;
    }
    if ((attribute & SERVICE_ATTR_WITHOUT_SANDBOX) == SERVICE_ATTR_WITHOUT_SANDBOX) {
        return;
    }
    INIT_ERROR_CHECK(execPath != NULL, return, "Service path is null.");
    if (strncmp(execPath, "/system/bin/", strlen("/system/bin/")) == 0) {
        INIT_INFO_CHECK(EnterSandbox("system") == 0, return,
            "Service %s skip enter system sandbox.", execPath);
    } else if (strncmp(execPath, "/vendor/bin/", strlen("/vendor/bin/")) == 0) {
        INIT_INFO_CHECK(EnterSandbox("chipset") == 0, return,
            "Service %s skip enter chipset sandbox.", execPath);
    } else {
        INIT_LOGI("Service %s does not enter sandbox", execPath);
    }
    return;
}
