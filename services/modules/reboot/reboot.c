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

#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/syscall.h>

#include "reboot_adp.h"
#include "init_cmdexecutor.h"
#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "securec.h"

static int DoRoot_(const char *jobName, int type)
{
    // by job to stop service and unmount
    if (jobName != NULL) {
        DoJobNow(jobName);
    }
#ifndef STARTUP_INIT_TEST
    return reboot(type);
#else
    return 0;
#endif
}

static int DoReboot(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    UNUSED(name);
    UNUSED(argc);
    UNUSED(argv);
    // clear misc
    (void)UpdateMiscMessage(NULL, "reboot", NULL, NULL);
    return DoRoot_("reboot", RB_AUTOBOOT);
}

static int DoRebootPanic(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    UNUSED(name);
    UNUSED(argc);
    UNUSED(argv);
    // clear misc
    (void)UpdateMiscMessage(NULL, "reboot", NULL, NULL);
    DoJobNow("reboot");
#ifndef STARTUP_INIT_TEST
    FILE *panic = fopen("/proc/sysrq-trigger", "wb");
    if (panic == NULL) {
        return reboot(RB_AUTOBOOT);
    }
    if (fwrite((void *)"c", 1, 1, panic) != 1) {
        (void)fclose(panic);
        PLUGIN_LOGI("fwrite to panic failed");
        return -1;
    }
    (void)fclose(panic);
#endif
    return 0;
}

static int DoRebootShutdown(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    UNUSED(name);
    UNUSED(argc);
    UNUSED(argv);
    // clear misc
    (void)UpdateMiscMessage(NULL, "reboot", NULL, NULL);
    return DoRoot_("reboot", RB_POWER_OFF);
}

static int DoRebootUpdater(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
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
    UNUSED(id);
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
    UNUSED(id);
    UNUSED(name);
    UNUSED(argc);
    UNUSED(argv);
    int ret = UpdateMiscMessage(NULL, "charge", "charge:", "boot_charge");
    if (ret == 0) {
        return DoRoot_("reboot", RB_AUTOBOOT);
    }
    return ret;
}

static int DoRebootSuspend(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    UNUSED(name);
    UNUSED(argc);
    UNUSED(argv);
    return DoRoot_("suspend", RB_AUTOBOOT);
}

static int DoRebootOther(int id, const char *name, int argc, const char **argv)
{
    UNUSED(id);
    UNUSED(name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter argc %d", argc);
    const char *cmd = strstr(argv[0], "reboot,");
    PLUGIN_CHECK(cmd != NULL, return -1, "Invalid parameter argc %s", argv[0]);
    PLUGIN_LOGI("DoRebootOther argv %s", argv[0]);
    return syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
        LINUX_REBOOT_CMD_RESTART2, cmd + strlen("reboot,"));
}

static void RebootAdpInit(void)
{
    // add default reboot cmd
    (void)AddCmdExecutor("reboot", DoReboot);
    (void)AddCmdExecutor("reboot.other", DoRebootOther);
    AddRebootCmdExecutor("shutdown", DoRebootShutdown);
    AddRebootCmdExecutor("flashd", DoRebootFlashed);
    AddRebootCmdExecutor("updater", DoRebootUpdater);
    AddRebootCmdExecutor("charge", DoRebootCharge);
    AddRebootCmdExecutor("suspend", DoRebootSuspend);
    AddRebootCmdExecutor("panic", DoRebootPanic);
    (void)AddCmdExecutor("panic", DoRebootPanic);
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("Reboot adapter plug-in init now ...");
    RebootAdpInit();
}

MODULE_DESTRUCTOR(void)
{
    PLUGIN_LOGI("Reboot adapter plug-in exit now ...");
}
