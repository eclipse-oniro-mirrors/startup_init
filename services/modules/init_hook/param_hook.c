
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
#include "init_service.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"

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
        {"ohos.servicectrl.clear", "clear", "clear" }
    };
    *size = ARRAY_LENGTH(installParam);
    return installParam;
}

const ParamCmdInfo *GetStartupPowerCtl(size_t *size)
{
    static const ParamCmdInfo powerCtrlArg[] = {
        {"reboot,shutdown", "reboot.shutdown", "reboot "},
        {"reboot,updater", "reboot.updater", "reboot "},
        {"reboot,flashd", "reboot.flashd", "reboot "},
        {"reboot,loader", "reboot.loader", "reboot "},
        {"reboot,charge", "reboot.charge", "reboot "},
        {"reboot", "reboot", "reboot "},
    };
    *size = ARRAY_LENGTH(powerCtrlArg);
    return powerCtrlArg;
}

const ParamCmdInfo *GetOtherSpecial(size_t *size)
{
    static const ParamCmdInfo other[] = {
        {"bootevent.", "bootevent.", "bootevent"},
    };
    *size = ARRAY_LENGTH(other);
    return other;
}
