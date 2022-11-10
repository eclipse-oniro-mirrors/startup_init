/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "init_reboot.h"
#include "begetctl.h"

#define REBOOT_CMD_NUMBER 2
static int main_cmd(BShellHandle shell, int argc, char* argv[])
{
    if (argc > REBOOT_CMD_NUMBER) {
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    int ret;
    if (argc == REBOOT_CMD_NUMBER) {
        ret = DoReboot(argv[1]);
    } else {
        ret = DoReboot(NULL);
    }
    if (ret != 0) {
        printf("[reboot command] DoReboot Api return error\n");
    } else {
        printf("[reboot command] DoReboot Api return ok\n");
    }
#ifndef STARTUP_INIT_TEST
    while (1) {
        pause();
    }
#endif
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    CmdInfo infos[] = {
        {"reboot", main_cmd, "reboot system", "reboot", ""},
        {"reboot", main_cmd, "shutdown system", "reboot shutdown", ""},
        {"reboot", main_cmd, "suspend system", "reboot suspend", ""},
        {"reboot", main_cmd, "reboot and boot into updater", "reboot updater", ""},
        {"reboot", main_cmd, "reboot and boot into updater", "reboot updater[:options]", ""},
        {"reboot", main_cmd, "reboot and boot into flashd", "reboot flashd", ""},
        {"reboot", main_cmd, "reboot and boot into flashd", "reboot flashd[:options]", ""},
        {"reboot", main_cmd, "reboot and boot into charge", "reboot charge", ""},
    };
    for (size_t i = sizeof(infos) / sizeof(infos[0]); i > 0; i--) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i - 1]);
    }
}
