/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "reboot_adp.h"
#include "init_cmdexecutor.h"
#include "init_log.h"
#include "init_group_manager.h"
#include "init_modulemgr.h"

void ExecReboot(const char *value)
{
    INIT_LOGI("ExecReboot %s", value);
#ifndef STARTUP_INIT_TEST
    // install module
    InitModuleMgrInstall("rebootmodule");
    if (strstr(value, "reboot") != NULL) {
        PluginExecCmdByName("reboot", value);
    } else {
        PluginExecCmdByName(value, value);
    }
#endif
    return;
}

void clearMisc(void)
{
    (void)UpdateMiscMessage(NULL, "reboot", NULL, NULL);
}

int GetBootModeFromMisc(void)
{
    char cmd[32] = {0}; // 32 cmd length
    (void)GetRebootReasonFromMisc(cmd, sizeof(cmd));
    if (memcmp(cmd, "boot_charge", strlen("boot_charge")) == 0) {
        return GROUP_CHARGE;
    }
    return 0;
}
