/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#include "selinux_adp.h"

#include <errno.h>

#include "init_error.h"
#include "init_hook.h"
#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "securec.h"

#include <policycoreutils.h>
#include <selinux/label.h>
#include <selinux/restorecon.h>

enum {
    CMD_LOAD_POLICY = 0,
    CMD_SET_SERVICE_CONTEXTS = 1,
    CMD_SET_SOCKET_CONTEXTS = 2,
    CMD_RESTORE_INDEX = 3,
    CMD_RESTORE_INDEX_FORCE = 4,
    CMD_RESTORE_INDEX_SKIP = 5,
};

extern char *__progname;

static int LoadSelinuxPolicy(int id, const char *name, int argc, const char **argv)
{
    int ret;
    char processContext[MAX_SECON_LEN];

    UNUSED(id);
    UNUSED(name);
    UNUSED(argc);
    UNUSED(argv);
    PLUGIN_LOGI("LoadSelinuxPolicy ");
    // load selinux policy and context
    if (LoadPolicy() < 0) {
        PLUGIN_LOGE("main, load_policy failed.");
    } else {
        PLUGIN_LOGI("main, load_policy success.");
    }

    ret = snprintf_s(processContext, sizeof(processContext), sizeof(processContext) - 1, "u:r:%s:s0", __progname);
    if (ret == -1) {
        setcon("u:r:init:s0");
    } else {
        setcon(processContext);
    }
    (void)RestoreconRecurse("/dev");
    return 0;
}

static int SetServiceContent(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(name != NULL && argc >= 1 && argv != NULL, return -1, "Invalid parameter");
    ServiceExtData *data = GetServiceExtData(argv[0], HOOK_ID_SELINUX);
    char *label = "u:r:limit_domain:s0";
    if (data != NULL) {
        label = (char *)data->data;
    } else {
        PLUGIN_LOGE("Please set secon field in service %s's cfg file, limit_domain will be blocked", argv[0]);
    }

    if (setexeccon(label) < 0) {
        PLUGIN_LOGE("Service error %d %s, failed to set secon %s.", errno, argv[0], label);
#ifndef STARTUP_INIT_TEST
        _exit(INIT_EEXEC_CONTENT);
#endif
    } else {
        PLUGIN_LOGV("Service info %s, set secon %s.", argv[0], label);
    }
    return 0;
}

static int SetSockCreateCon(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(name != NULL, return -1, "Invalid parameter");
    if (argc == 0) {
        setsockcreatecon(NULL);
        return 0;
    }
    PLUGIN_CHECK(argc >= 1 && argv != NULL, return -1, "Invalid parameter");
    ServiceExtData *data = GetServiceExtData(argv[0], HOOK_ID_SELINUX);
    if (data != NULL) {
        if (setsockcreatecon((char *)data->data) < 0) {
            PLUGIN_LOGE("failed to set socket context %s's secon (%s).", argv[0], (char *)data->data);
#ifndef STARTUP_INIT_TEST
            _exit(PROCESS_EXIT_CODE);
#endif
        }
    }

    return 0;
}

static int RestoreContentRecurse(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(name != NULL && argc >= 1 && argv != NULL, return -1, "Invalid parameter");
    PLUGIN_LOGV("RestoreContentRecurse path %s", argv[0]);
    if (RestoreconRecurse(argv[0]) && errno != 0) {
        PLUGIN_LOGE("restoreContentRecurse failed for '%s', err %d.", argv[0], errno);
    }
    return 0;
}

static int RestoreContentRecurseForce(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(name != NULL && argc >= 1 && argv != NULL, return -1, "Invalid parameter");
    PLUGIN_LOGV("RestoreContentRecurseForce path %s", argv[0]);
    if (RestoreconRecurseForce(argv[0]) && errno != 0) {
        PLUGIN_LOGE("RestoreContentRecurseForce failed for '%s', err %d.", argv[0], errno);
    }
    return 0;
}

static int RestoreContentRecurseSkipElx(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_CHECK(name != NULL && argc >= 1 && argv != NULL, return -1, "Invalid parameter");
    PLUGIN_LOGV("RestoreContentRecurseSkipElx path %s", argv[0]);
    if (RestoreconCommon(argv[0], SELINUX_RESTORECON_REALPATH |
        SELINUX_RESTORECON_RECURSE | SELINUX_RESTORECON_SKIPELX, 1) && errno != 0) {
        PLUGIN_LOGE("RestoreContentRecurseSkipElx failed for '%s', err %d.", argv[0], errno);
    }
    return 0;
}

static int32_t g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_SKIP + 1] = {0}; // 6 cmd count
static void SelinuxAdpInit(void)
{
    g_selinuxAdpCmdIds[CMD_LOAD_POLICY] = AddCmdExecutor("loadSelinuxPolicy", LoadSelinuxPolicy);
    g_selinuxAdpCmdIds[CMD_SET_SERVICE_CONTEXTS] = AddCmdExecutor("setServiceContent", SetServiceContent);
    g_selinuxAdpCmdIds[CMD_SET_SOCKET_CONTEXTS] = AddCmdExecutor("setSockCreateCon", SetSockCreateCon);
    g_selinuxAdpCmdIds[CMD_RESTORE_INDEX] = AddCmdExecutor("restoreContentRecurse", RestoreContentRecurse);
    g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_FORCE] =
        AddCmdExecutor("restoreContentRecurseForce", RestoreContentRecurseForce);
    g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_SKIP] =
        AddCmdExecutor("restoreContentRecurseSkipElx", RestoreContentRecurseSkipElx);
}

static void SelinuxAdpExit(void)
{
    if (g_selinuxAdpCmdIds[CMD_LOAD_POLICY] != -1) {
        RemoveCmdExecutor("loadSelinuxPolicy", g_selinuxAdpCmdIds[CMD_LOAD_POLICY]);
    }
    if (g_selinuxAdpCmdIds[CMD_SET_SERVICE_CONTEXTS] != -1) {
        RemoveCmdExecutor("setServiceContent", g_selinuxAdpCmdIds[CMD_SET_SERVICE_CONTEXTS]);
    }
    if (g_selinuxAdpCmdIds[CMD_SET_SOCKET_CONTEXTS] != -1) {
        RemoveCmdExecutor("setSockCreateCon", g_selinuxAdpCmdIds[CMD_SET_SOCKET_CONTEXTS]);
    }
    if (g_selinuxAdpCmdIds[CMD_RESTORE_INDEX] != -1) {
        RemoveCmdExecutor("restoreContentRecurse", g_selinuxAdpCmdIds[CMD_RESTORE_INDEX]);
    }
    if (g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_FORCE] != -1) {
        RemoveCmdExecutor("restoreContentRecurseForce", g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_FORCE]);
    }
    if (g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_SKIP] != -1) {
        RemoveCmdExecutor("restoreContentRecurseSkipElx", g_selinuxAdpCmdIds[CMD_RESTORE_INDEX_SKIP]);
    }
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("Selinux adapter plug-in init now ...");
    SelinuxAdpInit();
}

MODULE_DESTRUCTOR(void)
{
    if (getpid() != 1) {
        return;
    }
    PLUGIN_LOGI("Selinux adapter plug-in exit now ...");
    SelinuxAdpExit();
}
