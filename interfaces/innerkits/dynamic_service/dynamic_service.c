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

#include "dynamic_service.h"

#include "hilog/log.h"
#include "parameter.h"

#undef LOG_TAG
#undef LOG_DOMAIN
#define LOG_TAG "Init"
#define LOG_DOMAIN 0xD000719

int32_t StartDynamicProcess(const char *name)
{
    if (name == NULL) {
        HILOG_ERROR(LOG_CORE, "Start dynamic service failed, service name is null.");
        return -1;
    }
    if (SetParameter("ohos.ctl.start", name) != 0) {
        HILOG_ERROR(LOG_CORE, "Set param for {public}%s failed.\n", name);
        return -1;
    }
    return 0;
}

int32_t StopDynamicProcess(const char *name)
{
    if (name == NULL) {
        HILOG_ERROR(LOG_CORE, "Stop dynamic service failed, service is null.\n");
        return -1;
    }
    if (SetParameter("ohos.ctl.stop", name) != 0) {
        HILOG_ERROR(LOG_CORE, "Set param for {public}%s failed.\n", name);
        return -1;
    }
    return 0;
}

