/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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

#include "init_cmds.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

int GetParamValue(const char *symValue, unsigned int symLen, char *paramValue, unsigned int paramLen)
{
    return (strncpy_s(paramValue, paramLen, symValue, symLen) == EOK) ? 0 : -1;
}

static void DoExec(const struct CmdArgs *ctx)
{
    // format: exec /xxx/xxx/xxx xxx
    if (ctx == NULL || ctx->argv[0] == NULL) {
        INIT_LOGE("DoExec: invalid arguments");
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("DoExec: failed to fork child process to exec \"%s\"", ctx->argv[0]);
        return;
    }
    if (pid == 0) {
        int ret = execve(ctx->argv[0], ctx->argv, NULL);
        if (ret == -1) {
            INIT_LOGE("DoExec: execute \"%s\" failed: %d.", ctx->argv[0], errno);
        }
        _exit(0x7f);
    }
    return;
}

static bool CheckValidCfg(const char *path)
{
    static const char *supportCfg[] = {
        "/etc/patch.cfg",
        "/patch/fstab.cfg",
    };
    INIT_ERROR_CHECK(path != NULL, return false, "Invalid path for cfg");
    struct stat fileStat = { 0 };
    if (stat(path, &fileStat) != 0 || fileStat.st_size <= 0 || fileStat.st_size > LOADCFG_MAX_FILE_LEN) {
        return false;
    }
    size_t cfgCnt = ARRAY_LENGTH(supportCfg);
    for (size_t i = 0; i < cfgCnt; ++i) {
        if (strcmp(path, supportCfg[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void DoLoadCfg(const struct CmdArgs *ctx)
{
    char buf[LOADCFG_BUF_SIZE] = { 0 };
    size_t maxLoop = 0;
    if (!CheckValidCfg(ctx->argv[0])) {
        INIT_LOGE("CheckCfg file %s Failed", ctx->argv[0]);
        return;
    }
    char *realPath = GetRealPath(ctx->argv[0]);
    INIT_ERROR_CHECK(realPath != NULL, return, "Failed to get realpath %s", ctx->argv[0]);
    FILE *fp = fopen(realPath, "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open cfg %s error:%d", ctx->argv[0], errno);
        free(realPath);
        return;
    }

    while (fgets(buf, LOADCFG_BUF_SIZE - 1, fp) != NULL && maxLoop < LOADCFG_MAX_LOOP) {
        maxLoop++;
        int len = strlen(buf);
        if (len < 1) {
            continue;
        }
        if (buf[len - 1] == '\n') {
            buf[len - 1] = '\0'; // we replace '\n' with '\0'
        }
        const struct CmdTable *cmd = GetCmdByName(buf);
        if (cmd == NULL) {
            INIT_LOGE("Cannot support command: %s", buf);
            continue;
        }
        ExecCmd(cmd, &buf[strlen(cmd->name) + 1]);
    }
    free(realPath);
    (void)fclose(fp);
}

static const struct CmdTable g_cmdTable[] = {
    { "exec ", 1, 10, DoExec },
    { "loadcfg ", 1, 1, DoLoadCfg },
};

const struct CmdTable *GetCmdTable(int *number)
{
    *number = (int)ARRAY_LENGTH(g_cmdTable);
    return g_cmdTable;
}

void PluginExecCmdByName(const char *name, const char *cmdContent)
{
}

void PluginExecCmdByCmdIndex(int index, const char *cmdContent)
{
}

const char *PluginGetCmdIndex(const char *cmdStr, int *index)
{
    return NULL;
}

const char *GetPluginCmdNameByIndex(int index)
{
    return NULL;
}

int SetFileCryptPolicy(const char *dir)
{
    return 0;
}
