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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include "begetctl.h"
#include "shell.h"
#include "shell_utils.h"

static BShellHandle g_handle = NULL;
struct termios terminalState;
static void signalHandler(int signal)
{
    tcsetattr(0, TCSAFLUSH, &terminalState);
    demoExit();
    exit(0);
}

static int32_t ShellInput(char *data, int32_t len)
{
    for (int32_t i = 0; i < len; i++) {
        data[i] = getchar();
    }
    return len;
}

BShellHandle GetShellHandle(void)
{
    if (g_handle == NULL) {
        BShellInfo info = {PARAM_SHELL_DEFAULT_PROMPT, ShellInput};
        BShellEnvInit(&g_handle, &info);
    }
    return g_handle;
}

#ifndef STARTUP_INIT_TEST
int main(int argc, char *args[])
{
    (void)signal(SIGINT, signalHandler);
    (void)signal(SIGKILL, signalHandler);
    if (tcgetattr(0, &terminalState)) {
        return -1;
    }
    struct termios tio;
    if (tcgetattr(0, &tio)) {
        return -1;
    }
    tio.c_lflag &= ~(ECHO | ICANON | ISIG);
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 1;
    tcsetattr(0, TCSAFLUSH, &tio);

    BSH_LOGV("BShellEnvStart %d", argc);
    do {
        BShellHandle handle = GetShellHandle();
        if (handle == NULL) {
            printf("Failed to get shell handle \n");
            return 0;
        }
        const ParamInfo *param = BShellEnvGetReservedParam(handle, PARAM_REVERESD_NAME_CURR_PARAMETER);
        BSH_CHECK(param != NULL && param->type == PARAM_STRING, break, "Failed to get reversed param");
        BShellEnvSetParam(handle, param->name, param->desc, param->type, (void *)"");
        if (argc > 1) {
            int ret = SetParamShellPrompt(handle, args[1]);
            if (ret != 0) {
                break;
            }
        }
        BShellParamCmdRegister(handle, 1);
#ifdef INIT_TEST
        BShellCmdRegister(handle, 1);
#endif
        BShellEnvStart(handle);
        BShellEnvLoop(handle);
    } while (0);
    demoExit();
    tcsetattr(0, TCSAFLUSH, &terminalState);
    return 0;
}
#endif
