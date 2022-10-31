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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "begetctl.h"
#include "init_param.h"

static int bootchartCmdEnable(BShellHandle shell, int argc, char **argv)
{
    SystemSetParameter("persist.init.bootchart.enabled", "1");
    return 0;
}

static int bootchartCmdDisable(BShellHandle shell, int argc, char **argv)
{
    SystemSetParameter("persist.init.bootchart.enabled", "0");
    return 0;
}

static int bootchartCmdStart(BShellHandle shell, int argc, char **argv)
{
    char enable[4] = {}; // 4 enable size
    uint32_t size = sizeof(enable);
    int ret = SystemGetParameter("persist.init.bootchart.enabled", enable, &size);
    if (ret != 0 || strcmp(enable, "1") != 0) {
        BShellEnvOutput(shell, "Not bootcharting\r\n");
        return 0;
    }
    SystemSetParameter("ohos.servicectrl.bootchart", "start");
    return 0;
}

static int bootchartCmdStop(BShellHandle shell, int argc, char **argv)
{
    SystemSetParameter("ohos.servicectrl.bootchart", "stop");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"bootchart", bootchartCmdEnable, "bootchart enable", "bootchart enable", "bootchart enable"},
        {"bootchart", bootchartCmdDisable, "bootchart disable", "bootchart disable", "bootchart disable"},
        {"bootchart", bootchartCmdStart, "bootchart start", "bootchart start", "bootchart start"},
        {"bootchart", bootchartCmdStop, "bootchart stop", "bootchart stop", "bootchart stop"},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
