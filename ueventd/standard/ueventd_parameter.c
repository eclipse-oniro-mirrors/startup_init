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

#include "sys_param.h"
#include "ueventd_read_cfg.h"
#include "ueventd_utils.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"

int SetUeventDeviceParameter(const char *devNode, const char *paramValue)
{
    if (INVALIDSTRING(devNode) || INVALIDSTRING(paramValue)) {
        return -1;
    }
    struct DeviceUdevConf *config = GetDeviceUdevConfByDevNode(devNode);
    if ((config != NULL) && (config->parameter != NULL)) {
        if (SystemSetParameter(config->parameter, paramValue) != 0) {
            INIT_LOGE("[uevent][error] set device parameter %s failed", config->parameter);
            return -1;
        }
    }
    return 0;
}
