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

#include "bootstage.h"
#include "hookmgr.h"
#include "init_hook.h"
#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "securec.h"

#define REBOOT_NAME_PREFIX "reboot,"
#define REBOOT_CMD_PREFIX "reboot."
#define REBOOT_REPLACE_PREFIX "reboot."
#define PARAM_CMD_MAX 100

static int RebootHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    RebootHookCtx *ctx = (RebootHookCtx *)executionContext;
    InitRebootHook realHook = (InitRebootHook)hookInfo->hookCookie;
    realHook(ctx);
    return 0;
};

int InitAddRebootHook(InitRebootHook hook)
{
    HOOK_INFO info;
    info.stage = INIT_REBOOT;
    info.prio = 0;
    info.hook = RebootHookWrapper;
    info.hookCookie = (void *)hook;
    return HookMgrAddEx(GetBootStageHookMgr(), &info);
}

static ParamCmdInfo *g_rebootParamCmdInfos = NULL;
static int g_rebootParamCmdMaxNumber = 0;
static int g_rebootParamCmdValidNumber = 0;
static char *Dup2String(const char *prefix, const char *str)
{
    if (str == NULL) {
        return strdup("reboot");
    }
    size_t len = strlen(prefix) + strlen(str) + 1;
    char *tmp = calloc(1, len);
    PLUGIN_CHECK(tmp != NULL, return NULL, "Failed to alloc %s %s", prefix, str);
    int ret = sprintf_s(tmp, len, "%s%s", prefix, str);
    PLUGIN_CHECK(ret > 0, free(tmp);
        return NULL, "Failed to sprintf %s %s", prefix, str);
    return tmp;
}

static int CheckParamCmdExist(const char *cmd)
{
    if (g_rebootParamCmdInfos == NULL) {
        return 0;
    }
    char *cmdName = Dup2String(REBOOT_CMD_PREFIX, cmd);
    PLUGIN_CHECK(cmdName != NULL, return 0, "Failed to copy %s", cmd);
    for (int i = 0; i < g_rebootParamCmdValidNumber; i++) {
        if (strcmp(g_rebootParamCmdInfos[i].cmd, cmdName) == 0) {
            free(cmdName);
            return 1;
        }
    }
    free(cmdName);
    return 0;
}

static int SetParamCmdInfo(ParamCmdInfo *currInfo, CmdExecutor executor, const char *cmd)
{
    do {
        currInfo->name = Dup2String(REBOOT_NAME_PREFIX, cmd);
        PLUGIN_CHECK(currInfo->name != NULL, break, "Failed to copy %s", cmd);
        currInfo->replace = Dup2String(REBOOT_REPLACE_PREFIX, cmd);
        PLUGIN_CHECK(currInfo->replace != NULL, break, "Failed to copy %s", cmd);
        currInfo->cmd = Dup2String(REBOOT_CMD_PREFIX, cmd);
        PLUGIN_CHECK(currInfo->cmd != NULL, break, "Failed to copy %s", cmd);
        if (executor != NULL) {
            int cmdId = AddCmdExecutor(currInfo->cmd, executor);
            PLUGIN_CHECK(cmdId > 0, break, "Failed to add cmd %s", cmd);
        }
        PLUGIN_LOGV("SetParamCmdInfo '%s' '%s' '%s' ", currInfo->name, currInfo->cmd, currInfo->replace);
        currInfo = NULL;
        g_rebootParamCmdValidNumber++;
        return 0;
    } while (0);
    if (currInfo != NULL) {
        if (currInfo->name != NULL) {
            free(currInfo->name);
        }
        if (currInfo->cmd != NULL) {
            free(currInfo->cmd);
        }
        if (currInfo->replace != NULL) {
            free(currInfo->replace);
        }
    }
    return -1;
}

static int AddRebootCmdExecutor_(const char *cmd, CmdExecutor executor)
{
    PLUGIN_CHECK(g_rebootParamCmdMaxNumber <= PARAM_CMD_MAX, return -1, "Param cmd max number exceed limit");
    if (g_rebootParamCmdMaxNumber == 0 || g_rebootParamCmdMaxNumber <= g_rebootParamCmdValidNumber) {
        g_rebootParamCmdMaxNumber += 5; // inc 5 once time
        ParamCmdInfo *cmdInfos = calloc(1, sizeof(ParamCmdInfo) * g_rebootParamCmdMaxNumber);
        PLUGIN_CHECK(cmdInfos != NULL, return -1, "Failed to add reboot cmd %s", cmd);
        if (g_rebootParamCmdInfos != NULL) { // delete old
            // copy from old
            for (int i = 0; i < g_rebootParamCmdValidNumber; i++) {
                cmdInfos[i].name = g_rebootParamCmdInfos[i].name;
                cmdInfos[i].replace = g_rebootParamCmdInfos[i].replace;
                cmdInfos[i].cmd = g_rebootParamCmdInfos[i].cmd;
            }
            free(g_rebootParamCmdInfos);
        }
        g_rebootParamCmdInfos = cmdInfos;
    }
    PLUGIN_CHECK(g_rebootParamCmdValidNumber >= 0 && g_rebootParamCmdValidNumber < g_rebootParamCmdMaxNumber,
        return -1, "Param cmd number exceed limit");
    return SetParamCmdInfo(&g_rebootParamCmdInfos[g_rebootParamCmdValidNumber], executor, cmd);
}

int AddRebootCmdExecutor(const char *cmd, CmdExecutor executor)
{
    PLUGIN_CHECK(cmd != NULL && executor != NULL, return EINVAL, "Invalid input parameter");
    int ret = CheckParamCmdExist(cmd);
    if (ret != 0) {
        PLUGIN_LOGI("Cmd %s exist", cmd);
        return EEXIST;
    }
    return AddRebootCmdExecutor_(cmd, executor);
}

const ParamCmdInfo *GetStartupPowerCtl(size_t *size)
{
    RebootHookCtx context;
    context.reason = "";
    (void)HookMgrExecute(GetBootStageHookMgr(), INIT_REBOOT, (void *)(&context), NULL);
    PLUGIN_LOGI("After install reboot module");
    *size = g_rebootParamCmdValidNumber;
    return g_rebootParamCmdInfos;
}

static void InitRebootHook_(RebootHookCtx *ctx)
{
    InitModuleMgrInstall("rebootmodule");
    PLUGIN_LOGI("Install rebootmodule.");
}

MODULE_CONSTRUCTOR(void)
{
    // 执行reboot时调用，安装reboot模块
    InitAddRebootHook(InitRebootHook_);
}
