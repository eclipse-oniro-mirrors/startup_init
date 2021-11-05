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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/resource.h>

#include "init.h"
#include "init_log.h"
#include "init_param.h"
#include "securec.h"

#define MIN_IMPORTANT_LEVEL (-20)
#define MAX_IMPORTANT_LEVEL 19

void NotifyServiceChange(const char *serviceName, const char *change)
{
    char paramName[PARAM_NAME_LEN_MAX] = { 0 };
    INIT_CHECK_ONLY_ELOG(snprintf_s(paramName, PARAM_NAME_LEN_MAX, PARAM_NAME_LEN_MAX - 1, "init.svc.%s",
        serviceName) >= 0, "snprintf_s paramName error %d ", errno);
    SystemWriteParam(paramName, change);
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
            INIT_LOGE("setpriority failed for %s, importance = %d", service->name, service->importance);
            _exit(0x7f); // 0x7f: user specified
        }
    }
    INIT_CHECK_ONLY_ELOG(unsetenv("UV_THREADPOOL_SIZE") == 0, "set UV_THREADPOOL_SIZE error : %d.", errno);
    // L2 Can not be reset env
    INIT_CHECK_ONLY_ELOG(execv(service->pathArgs.argv[0], service->pathArgs.argv) == 0,
        "service %s execve failed! err %d.", service->name, errno);
    return SERVICE_SUCCESS;
}
