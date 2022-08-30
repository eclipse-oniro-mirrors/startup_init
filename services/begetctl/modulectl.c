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

#include "begetctl.h"
#include "control_fd.h"
#include "init_utils.h"
#include "init_param.h"
#include "securec.h"
#include "shell_utils.h"

#define MODULE_CTL_CMD_ARGS 2
static int32_t ModuleInstallCmd(BShellHandle shell, int32_t argc, char *argv[])
{
    if (argc != MODULE_CTL_CMD_ARGS) {
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    BSH_LOGV("ModuleInstallCmd %s %s \n", argv[0], argv[1]);
    char combinedArgs[MAX_BUFFER_LEN];
    int ret = sprintf_s(combinedArgs, sizeof(combinedArgs), "ohos.servicectrl.%s", argv[0]);
    BSH_CHECK(ret > 0, return -1, "Invalid buffer");
    combinedArgs[ret] = '\0';
    SystemSetParameter(combinedArgs, argv[1]);
    return 0;
}

static int ModuleDisplayCmd(BShellHandle shell, int argc, char **argv)
{
    if (argc < 1) {
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_MODULEMGR, argv[0]);
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"modulectl", ModuleDisplayCmd, "dump all modules installed",
            "modulectl list", "modulectl list"},
        {"modulectl", ModuleInstallCmd, "install specified module",
            "modulectl install moduleName", "modulectl install"},
        {"modulectl", ModuleInstallCmd, "uninstall specified module",
            "modulectl uninstall moduleName", "modulectl uninstall"},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
