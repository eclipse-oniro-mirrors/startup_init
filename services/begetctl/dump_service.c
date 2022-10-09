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
#include "beget_ext.h"
#include "control_fd.h"
#include "securec.h"
#include "init_param.h"

#define DUMP_SERVICE_INFO_CMD_ARGS 2
#define DUMP_SERVICE_BOOTEVENT_CMD_ARGS 3
static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    if (argc == DUMP_SERVICE_INFO_CMD_ARGS) {
        printf("dump service info \n");
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_DUMP, argv[1]);
    } else if (argc == DUMP_SERVICE_BOOTEVENT_CMD_ARGS) {
        if (strcmp(argv[1], "bootevent") == 0) {
            printf("dump service bootevent info \n");
        } else if (strcmp(argv[1], "parameter_service") == 0) {
            printf("dump parameter service info \n");
        }
        size_t serviceNameLen = strlen(argv[1]) + strlen(argv[2]) + 2; // 2 is \0 and #
        char *cmd = (char *)calloc(1, serviceNameLen);
        BEGET_ERROR_CHECK(cmd != NULL, return 0, "failed to allocate cmd memory");
        BEGET_ERROR_CHECK(sprintf_s(cmd, serviceNameLen, "%s#%s", argv[1], argv[2]) >= 0, free(cmd);
            return 0, "dump service arg create failed");
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_DUMP, cmd);
        free(cmd);
    } else {
        BShellCmdHelp(shell, argc, argv);
    }
    return 0;
}

static int ClearBootEvent(BShellHandle shell, int argc, char **argv)
{
    return SystemSetParameter("ohos.servicectrl.clear", "bootevent");
}

static int SaveBootEvent(BShellHandle shell, int argc, char **argv)
{
    return SystemSetParameter("ohos.servicectrl.save", "bootevent");
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"dump_service", main_cmd, "dump one service info by serviceName", "dump_service serviceName", NULL},
        {"dump_service", main_cmd, "dump one service bootevent", "dump_service serviceName bootevent", NULL},
        {"dump_service", main_cmd, "dump all services info", "dump_service all", NULL},
        {"dump_service", main_cmd, "dump parameter-service trigger",
            "dump_service parameter_service trigger", "dump_service parameter_service trigger"},
        {"dump_service", main_cmd, "dump all services bootevent", "dump_service all bootevent", NULL},
        {"service", ClearBootEvent, "Clear all services bootevent", "service clear bootevent",
            "service clear bootevent"},
        {"service", SaveBootEvent, "Save all services bootevent", "service save bootevent",
            "service save bootevent"},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
