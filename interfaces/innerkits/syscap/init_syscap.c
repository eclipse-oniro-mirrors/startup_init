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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "init_param.h"
#include "beget_ext.h"
#include "securec.h"
#include "systemcapability.h"

#define SYSCAP_MAX_SIZE 100
#define SYSCAP_PREFIX_NAME "SystemCapability"
#define CONST_SYSCAP_PREFIX_NAME "const.SystemCapability"

bool HasSystemCapability(const char *cap)
{
    char capName[SYSCAP_MAX_SIZE] = { 0 };
    char paramValue[PARAM_VALUE_LEN_MAX] = { 0 };
    unsigned int valueLen = PARAM_VALUE_LEN_MAX;

    if (cap == NULL) {
        BEGET_LOGE("cap input is null.");
        return false;
    }

    if (strncmp(SYSCAP_PREFIX_NAME, cap, sizeof(SYSCAP_PREFIX_NAME) - 1) == 0) {
        if (snprintf_s(capName, SYSCAP_MAX_SIZE, SYSCAP_MAX_SIZE - 1, "const.%s", cap) == -1) {
            BEGET_LOGE("Failed snprintf_s err=%d", errno);
            return false;
        }
    } else if (snprintf_s(capName, SYSCAP_MAX_SIZE, SYSCAP_MAX_SIZE - 1, CONST_SYSCAP_PREFIX_NAME".%s", cap) == -1) {
        BEGET_LOGE("Failed snprintf_s err=%d", errno);
        return false;
    }

    if (SystemGetParameter(capName, paramValue, &valueLen) != 0) {
        BEGET_LOGE("Failed get paramName.");
        return false;
    }
    
    return true;
}