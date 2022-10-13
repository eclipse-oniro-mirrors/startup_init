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
#include <unistd.h>

#include "beget_ext.h"
#include "securec.h"
#include "init_param.h"

// Refer to parameter limit, value size should not bigger than 96
#define MAX_REBOOT_OPTION_SIZE PARAM_VALUE_LEN_MAX

#ifndef STARTUP_INIT_TEST
#define DOREBOOT_PARAM "ohos.startup.powerctrl"
#else
#define DOREBOOT_PARAM "reboot.ut"
#endif

static int DoReboot_(const char *option)
{
    char value[MAX_REBOOT_OPTION_SIZE];
    int ret = 0;
    if (option == NULL || strlen(option) == 0) {
        ret = SystemSetParameter(STARTUP_DEVICE_CTL, DEVICE_CMD_STOP);
        BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to set stop param");
        ret = SystemSetParameter(DOREBOOT_PARAM, "reboot");
        BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to set reboot param");
        return 0;
    }
    int length = strlen(option);
    BEGET_ERROR_CHECK(length <= MAX_REBOOT_OPTION_SIZE, return -1,
        "Reboot option \" %s \" is too large, overflow", option);
    ret = snprintf_s(value, MAX_REBOOT_OPTION_SIZE, MAX_REBOOT_OPTION_SIZE - 1, "reboot,%s", option);
    BEGET_ERROR_CHECK(ret >= 0, return -1, "Failed to copy boot option \" %s \"", option);

    ret = SystemSetParameter(STARTUP_DEVICE_CTL, DEVICE_CMD_STOP);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed to set stop param");
    ret = SystemSetParameter(DOREBOOT_PARAM, value);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Set parameter to trigger reboot command \" %s \" failed", value);
    return 0;
}

int DoReboot(const char *option)
{
    // check if param set ok
#ifndef STARTUP_INIT_TEST
    const int maxCount = 10;
#else
    const int maxCount = 1;
#endif
    int count = 0;
    DoReboot_(option);
    while (count < maxCount) {
        usleep(100 * 1000); // 100 * 1000 wait 100ms
        char result[10] = {0}; // 10 stop len
        uint32_t len = sizeof(result);
        int ret = SystemGetParameter(STARTUP_DEVICE_CTL, result, &len);
        if (ret == 0 && strcmp(result, DEVICE_CMD_STOP) == 0) {
            BEGET_LOGE("Success to reboot system");
            return 0;
        }
        count++;
        DoReboot_(option);
    }
    BEGET_LOGE("Failed to reboot system");
    return 0;
}
