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

#define DUMP_SERVICE_INFO_CMD_ARGS 2
static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    if (argc != DUMP_SERVICE_INFO_CMD_ARGS) {
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "dump_service") == 0) {
        printf("dump service info \n");
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_DUMP, argv[1]);
    } else {
        BShellCmdHelp(shell, argc, argv);
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"dump_service", main_cmd, "dump one service info by serviceName", "dump_service serviceName", NULL},
        {"dump_service", main_cmd, "dump all services info", "dump_service all", NULL},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegitsterCmd(GetShellHandle(), &infos[i]);
    }
}
