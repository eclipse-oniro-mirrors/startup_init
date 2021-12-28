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
#include <stdlib.h>
#include <string.h>

#include "hilog/log.h"
#include "init_utils.h"
#include "securec.h"

int ServiceWatchForStatus(const char *serviceName, void *context, ServiceStatusChangePtr changeCallback)
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
    if (SystemWatchParameter(paramName, changeCallback, context) != 0) {
        HILOG_ERROR(LOG_CORE, "Wait param for %{public}s failed.\n", paramName);
        return -1;
    }
    return 0;
}
