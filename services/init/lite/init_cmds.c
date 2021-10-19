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

#include <stdlib.h>
#include <string.h>

#include "init.h"
#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

int GetParamValue(const char *symValue, unsigned int symLen, char *paramValue, unsigned int paramLen)
{
    return (strncpy_s(paramValue, paramLen, symValue, symLen) == EOK) ? 0 : -1;
}

static void DoExec(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(ctx);
    // format: exec /xxx/xxx/xxx xxx
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("DoExec: failed to fork child process to exec \"%s\"", cmdContent);
        return;
    }
    if (pid == 0) {
        struct CmdArgs *subCtx = GetCmdArg(cmdContent, " ", SUPPORT_MAX_ARG_FOR_EXEC);
        if (subCtx == NULL || subCtx->argv[0] == NULL) {
            INIT_LOGE("DoExec: invalid arguments :%s", cmdContent);
            _exit(0x7f);
        }
        int ret = execve(subCtx->argv[0], subCtx->argv, NULL);
        if (ret == -1) {
            INIT_LOGE("DoExec: execute \"%s\" failed: %d.", cmdContent, errno);
        }
        FreeCmdArg(subCtx);
        _exit(0x7f);
    }
    return;
}

static const struct CmdTable g_cmdTable[] = { { "exec ", 1, 10, DoExec } };

const struct CmdTable *GetCmdTable(int *number)
{
    *number = (int)ARRAY_LENGTH(g_cmdTable);
    return g_cmdTable;
}
