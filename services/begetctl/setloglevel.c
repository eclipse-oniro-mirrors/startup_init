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
#include <stdlib.h>

#include "begetctl.h"
#include "init_log.h"
#include "init_utils.h"
#include "init_param.h"
#include "securec.h"
#include "shell_utils.h"

static int32_t SetInitLogLevelFromParam(BShellHandle shell, int argc, char **argv)
{
    if (argc != 2) { // 2 is set log level parameter number
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    errno = 0;
    unsigned int level = strtoul(argv[1], 0, 10); // 10 is decimal
    if (errno != 0) {
        printf("Failed to transform %s to unsigned int. \n", argv[1]);
        return -1;
    }
    if ((level >= INIT_DEBUG) && (level <= INIT_FATAL)) {
        int ret = SystemSetParameter("persist.init.debug.loglevel", argv[1]);
        if (ret != 0) {
            printf("Failed to set log level by param \"persist.init.debug.loglevel\" %s. \n", argv[1]);
        } else {
            printf("Success to set log level by param \"persist.init.debug.loglevel\" %s. \n", argv[1]);
        }
    } else {
        printf("Set init log level in invailed parameter %s. \n", argv[1]);
    }
    return 0;
}

static int32_t GetInitLogLevelFromParam(BShellHandle shell, int argc, char **argv)
{
    char logLevel[2] = {0}; // 2 is set param "persist.init.debug.loglevel" value length.
    uint32_t len = sizeof(logLevel);
    int ret = SystemReadParam("persist.init.debug.loglevel", logLevel, &len);
    if (ret == 0) {
        printf("Success to get init log level: %s from param \"persist.init.debug.loglevel\". \n", logLevel);
    } else {
        printf("Failed to get init log level from param, keep the system origin log level. \n");
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"setloglevel", SetInitLogLevelFromParam, "set init log level 0:debug, 1:info, 2:warning, 3:err, 4:fatal",
            "setloglevel level", NULL},
        {"getloglevel", GetInitLogLevelFromParam, "get init log level 0:debug, 1:info, 2:warning, 3:err, 4:fatal",
            "getloglevel", NULL},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}