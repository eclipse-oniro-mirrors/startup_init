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
#include <sys/reboot.h>

#include "reboot_adp.h"
#include "init_cmdexecutor.h"
#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "securec.h"

typedef struct {
    const char *cmd;
    CmdExecutor executor;
    uint32_t cmdId;
} ModuleCmdInfo;

static int DoRoot_(const char *jobName, int type)
{
    // by job to stop service and unmount
    if (jobName != NULL) {
        DoJobNow(jobName);
    }
#ifndef STARTUP_INIT_TEST
    return reboot(type);
#endif
}

static int DoReboot(int id, const char *name, int argc, const char **argv)
{
    // clear misc
    (void)UpdateMiscMessage(NULL, "reboot", NULL, NULL);
    return DoRoot_("reboot", RB_AUTOBOOT);
}

static int DoRebootShutdown(int id, const char *name, int argc, const char **argv)
{
    // clear misc
    (void)UpdateMiscMessage(NULL, "reboot", NULL, NULL);
    return DoRoot_("reboot", RB_POWER_OFF);
}

static int DoRebootUpdater(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("DoRebootUpdater argc %d %s", argc, name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGI("DoRebootUpdater argv %s", argv[0]);
    int ret = UpdateMiscMessage(argv[0], "updater", "updater:", "boot_updater");
    if (ret == 0) {
        return DoRoot_("reboot", RB_AUTOBOOT);
    }
    return ret;
}

static int DoRebootFlashed(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("DoRebootFlashed argc %d %s", argc, name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGI("DoRebootFlashd argv %s", argv[0]);
    int ret = UpdateMiscMessage(argv[0], "flash", "flash:", "boot_flash");
    if (ret == 0) {
        return DoRoot_("reboot", RB_AUTOBOOT);
    }
    return ret;
}

static int DoRebootCharge(int id, const char *name, int argc, const char **argv)
{
    int ret = UpdateMiscMessage(NULL, "charge", "charge:", "boot_charge");
    if (ret == 0) {
        return DoRoot_("reboot", RB_AUTOBOOT);
    }
    return ret;
}

#ifdef PRODUCT_RK
#include <sys/syscall.h>
#define REBOOT_MAGIC1       0xfee1dead
#define REBOOT_MAGIC2       672274793
#define REBOOT_CMD_RESTART2 0xA1B2C3D4
static int DoRebootLoader(int id, const char *name, int argc, const char **argv)
{
    // by job to stop service and unmount
    DoJobNow("reboot");
    syscall(__NR_reboot, REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_CMD_RESTART2, "loader");
    return 0;
}
#endif

static ModuleCmdInfo g_rebootCmdIds[] = {
    {"reboot.shutdown", DoRebootShutdown, 0},
    {"reboot.flashd", DoRebootFlashed, 0},
    {"reboot.updater", DoRebootUpdater, 0},
    {"reboot.charge", DoRebootCharge, 0},
#ifdef PRODUCT_RK
    {"reboot.loader", DoRebootLoader, 0},
#endif
    {"reboot", DoReboot, 0},
};

static void RebootAdpInit(void)
{
    for (size_t i = 0; i < sizeof(g_rebootCmdIds)/sizeof(g_rebootCmdIds[0]); i++) {
        g_rebootCmdIds[i].cmdId = AddCmdExecutor(g_rebootCmdIds[i].cmd, g_rebootCmdIds[i].executor);
    }
}

static void RebootAdpExit(void)
{
    for (size_t i = 0; i < sizeof(g_rebootCmdIds)/sizeof(g_rebootCmdIds[0]); i++) {
        if (g_rebootCmdIds[i].cmdId == 0) {
            continue;
        }
        RemoveCmdExecutor(g_rebootCmdIds[i].cmd, g_rebootCmdIds[i].cmdId);
    }
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("RebootAdp init now ...");
    RebootAdpInit();
}

MODULE_DESTRUCTOR(void)
{
    PLUGIN_LOGI("RebootAdp exit now ...");
    RebootAdpExit();
}
