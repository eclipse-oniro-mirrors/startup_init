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
#include "service_control.h"

#define MIN_IMPORTANT_LEVEL (-20)
#define MAX_IMPORTANT_LEVEL 19

void NotifyServiceChange(Service *service, int status)
{
    int size = 0;
    const InitArgInfo *statusMap = GetServieStatusMap(&size);
    INIT_ERROR_CHECK(statusMap != NULL && size > status, service->status = status;
        return, "Service status error %d", status);
    INIT_LOGI("NotifyServiceChange %s %s to %s", service->name,
        statusMap[service->status].name, statusMap[status].name);
    service->status = status;
    if (status == SERVICE_IDLE) {
        return;
    }
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    int ret = snprintf_s(paramName, sizeof(paramName), sizeof(paramName) - 1,
        "%s.%s", STARTUP_SERVICE_CTL, service->name);
    if (ret >= 0) {
        SystemWriteParam(paramName, statusMap[status].name);
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
        if (setpriority(PRIO_PROCESS, 0, service->importance) != 0) {
            INIT_LOGE("setpriority failed for %s, importance = %d, err=%d",
                service->name, service->importance, errno);
                _exit(0x7f); // 0x7f: user specified
        }
    }
    INIT_CHECK_ONLY_ELOG(unsetenv("UV_THREADPOOL_SIZE") == 0, "set UV_THREADPOOL_SIZE error : %d.", errno);
#ifdef SUPPORT_PROFILER_HIDEBUG
    do {
        if (access("/system/lib/libhidebug.so", F_OK) != 0) {
            INIT_LOGE("access failed, errno = %d\n", errno);
            break;
        }
        void* handle = dlopen("/system/lib/libhidebug.so", RTLD_LAZY);
        if (handle == NULL) {
            INIT_LOGE("Failed to dlopen libhidebug.so, %s\n", dlerror());
            break;
        }
        bool (* initParam)();
        initParam = (bool (*)())dlsym(handle, "InitEnvironmentParam");
        if (initParam == NULL) {
            INIT_LOGE("Failed to dlsym InitEnvironmentParam, %s\n", dlerror());
            dlclose(handle);
            break;
        }
        bool ret = (*initParam)(service->name);
        if (!ret) {
            INIT_LOGE("init parameters failed.\n");
        }
        dlclose(handle);
    } while (0);
#endif
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
    INIT_LOGI("%s: token id %lld, set token id result %d", service->name, service->tokenId, ret);
    return ret == 0 ? SERVICE_SUCCESS : SERVICE_FAILURE;
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
            if (strlen(service->apl) == 0) {
                (void)strncpy_s(service->apl, sizeof(service->apl), "system_core", sizeof(service->apl) - 1);
            }
            uint64_t tokenId = GetAccessTokenId(service->name, (const char **)service->capsArgs.argv,
                service->capsArgs.count, service->apl);
            if (tokenId  == 0) {
                INIT_LOGE("Get totken id %lld of service \' %s \' failed", tokenId, service->name);
            }
            service->tokenId = tokenId;
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
}
