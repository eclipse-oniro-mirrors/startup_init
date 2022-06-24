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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "begetctl.h"
#include "control_fd.h"
#include "init_utils.h"
#include "securec.h"

#define MODULE_CTL_CMD_ARGS 2
static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    char combinedArgs[MAX_BUFFER_LEN];

    if (argc < MODULE_CTL_CMD_ARGS) {
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    if (argc > MODULE_CTL_CMD_ARGS) {
        (void)snprintf_s(combinedArgs, sizeof(combinedArgs),
                sizeof(combinedArgs) - 1, "%s:%s", argv[1], argv[2]);
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_MODULEMGR, combinedArgs, "FIFO");
    } else {
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_MODULEMGR, argv[1], "FIFO");
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"modulectl", main_cmd, "dump all modules installed", "modulectl list", NULL},
        {"modulectl", main_cmd, "install or uninstall specified module",
                                "modulectl install|uninstall moduleName", NULL},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegitsterCmd(GetShellHandle(), &infos[i]);
    }
}
