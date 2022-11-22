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
#include "service_control.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "begetctl.h"
#include "init_utils.h"

#define SERVICE_START_NUMBER 2
#define SERVICE_CONTROL_NUMBER 3
#define CONTROL_SERVICE_POS 2
#define SERVICE_CONTROL_MAX_SIZE 50

static void ServiceControlUsage(BShellHandle shell, int argc, char **argv)
{
    BShellCmdHelp(shell, argc, argv);
    return;
}

static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    if (argc < SERVICE_START_NUMBER) {
        ServiceControlUsage(shell, argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "start_service") == 0) {
        ServiceControlWithExtra(argv[1], 0, (const char **)argv + SERVICE_START_NUMBER, argc - SERVICE_START_NUMBER);
    } else if (strcmp(argv[0], "stop_service") == 0) {
        ServiceControlWithExtra(argv[1], 1, (const char **)argv + SERVICE_START_NUMBER, argc - SERVICE_START_NUMBER);
    } else if (strcmp(argv[0], "start") == 0) {
        ServiceControlWithExtra(argv[1], 0, (const char **)argv + SERVICE_START_NUMBER, argc - SERVICE_START_NUMBER);
    } else if (strcmp(argv[0], "stop") == 0) {
        ServiceControlWithExtra(argv[1], 1, (const char **)argv + SERVICE_START_NUMBER, argc - SERVICE_START_NUMBER);
    } else if (strcmp(argv[0], "timer_start") == 0) {
        if (argc <= SERVICE_START_NUMBER) {
            ServiceControlUsage(shell, argc, argv);
            return 0;
        }
        char *timeBuffer = argv[SERVICE_START_NUMBER];
        errno = 0;
        uint64_t timeout = strtoull(timeBuffer, NULL, DECIMAL_BASE);
        if (errno != 0) {
            return -1;
        }
        StartServiceByTimer(argv[1], timeout);
    } else if (strcmp(argv[0], "timer_stop") == 0) {
        StopServiceTimer(argv[1]);
    } else {
        ServiceControlUsage(shell, argc, argv);
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"service_control", main_cmd, "stop service", "service_control stop servicename", "service_control stop"},
        {"service_control", main_cmd, "start service", "service_control start servicename", "service_control start"},
        {"stop_service", main_cmd, "stop service", "stop_service servicename", ""},
        {"start_service", main_cmd, "start service", "start_service servicename", ""},
        {"timer_start", main_cmd, "start service by timer", "timer_start servicename timeout", ""},
        {"timer_stop", main_cmd, "stop service timer", "timer_stop servicename", ""},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
