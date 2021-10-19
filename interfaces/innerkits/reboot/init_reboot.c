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
#include "init_reboot.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_log.h"
#include "param.h"
#include "securec.h"
#include "sys_param.h"

// Refer to parameter limit, value size should not bigger than 96
#define MAX_REBOOT_OPTION_SIZE PARAM_VALUE_LEN_MAX

int DoReboot(const char *option)
{
    uid_t uid = getuid();
    uid_t euid = geteuid();
    if (uid != 0 || euid != 0) {
        INIT_LOGE("User or effective user MUST be root, abort!");
        return -1;
    }
    char value[MAX_REBOOT_OPTION_SIZE];
    if (option == NULL || strlen(option) == 0) {
        if (snprintf_s(value, MAX_REBOOT_OPTION_SIZE, strlen("reboot") + 1, "%s", "reboot") < 0) {
            INIT_LOGE("reboot options too large, overflow");
            return -1;
        }
        if (SystemSetParameter("sys.powerctrl", value) != 0) {
            INIT_LOGE("Set parameter to trigger reboot command \" %s \" failed", value);
            return -1;
        }
        return 0;
    }
    int length = strlen(option);
    if (length > MAX_REBOOT_OPTION_SIZE) {
        INIT_LOGE("Reboot option \" %s \" is too large, overflow", option);
        return -1;
    }
    if (snprintf_s(value, MAX_REBOOT_OPTION_SIZE, MAX_REBOOT_OPTION_SIZE - 1, "%s%s", "reboot,", option) < 0) {
        INIT_LOGE("Failed to copy boot option \" %s \"", option);
        return -1;
    }
    if (SystemSetParameter("sys.powerctrl", value) != 0) {
        INIT_LOGE("Set parameter to trigger reboot command \" %s \" failed", value);
        return -1;
    }
    return 0;
}

