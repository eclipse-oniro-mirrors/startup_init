
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "init_hook.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"

/**
系统参数转化规则
    1，ohos.ctl.start.{start|stop} = servicename
        转化后系统参数，用来进行dac/mac校验
            ohos.servicectrl.{servicename}
        对应的处理命令
            start
    2，ohos.startup.powerctrl = reboot,[bootcharge | shutdown | flashd | updater]
        转化后系统参数，用来进行dac/mac校验
            ohos.servicectrl.reboot.[bootcharge | shutdown | flashd | updater]
        对应的处理命令
            reboot.[bootcharge | shutdown | flashd | updater]
    3，ohos.servicectrl.{cmdName} = {arg}
        转化后系统参数，用来进行dac/mac校验
            ohos.servicectrl.{cmd}
        对应的处理命令
            cmd
    4，普通系统参数，根据定义的ParamCmdInfo进行处理
        {xxxx.xxxx.xxxx, xxxx.xxxx.xxxx, cmdname}
        例如：
            xxxx.xxxx.xxxx = {args}
            转化后系统参数，用来进行dac/mac校验
                xxxx.xxxx.xxxx.{args}
            对应的处理命令
                cmdname
            命令输入参数(跳过replace部分的剩余值)
                xxxx.xxxx.xxxx.{args} + strlen(name)
*/

const ParamCmdInfo *GetServiceStartCtrl(size_t *size)
{
    static const ParamCmdInfo ctrlParam[] = {
        {"ohos.ctl.start", "start", "start "},
        {"ohos.ctl.stop", "stop", "stop "},
    };
    *size = ARRAY_LENGTH(ctrlParam);
    return ctrlParam;
}

const ParamCmdInfo *GetServiceCtl(size_t *size)
{
    static const ParamCmdInfo installParam[] = {
        {"ohos.servicectrl.install", "install", "install" },
        {"ohos.servicectrl.uninstall", "uninstall", "uninstall" },
        {"ohos.servicectrl.bootchart", "bootchart", "bootchart" },
        {"ohos.servicectrl.init_trace", "init_trace", "init_trace" },
        {"ohos.servicectrl.timer_start", "timer_start", "timer_start " },
        {"ohos.servicectrl.timer_stop", "timer_stop", "timer_stop" },
        {"ohos.servicectrl.cmd", "cmd", "initcmd"},
    };
    *size = ARRAY_LENGTH(installParam);
    return installParam;
}

#ifdef OHOS_LITE
const ParamCmdInfo *GetStartupPowerCtl(size_t *size)
{
    static const ParamCmdInfo powerCtrlArg[] = {
        {"reboot,shutdown", "reboot.shutdown", "reboot.shutdown"},
        {"reboot,updater", "reboot.updater", "reboot.updater"},
        {"reboot,flashd", "reboot.flashd", "reboot.flashd"},
        {"reboot,charge", "reboot.charge", "reboot.charge"},
        {"reboot", "reboot", "reboot"},
    };
    *size = ARRAY_LENGTH(powerCtrlArg);
    return powerCtrlArg;
}
#endif

const ParamCmdInfo *GetOtherSpecial(size_t *size)
{
    static const ParamCmdInfo other[] = {
        {"bootevent.", "bootevent.", "bootevent"},
        {"persist.init.debug.", "persist.init.debug.", "setloglevel"},
        // for hitrace start, need interrupt init trace
        {"debug.hitrace.enable.state", "debug.hitrace.enable.state.", "init_trace"},
    };
    *size = ARRAY_LENGTH(other);
    return other;
}
