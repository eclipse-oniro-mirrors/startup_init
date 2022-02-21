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

static int32_t ShellOuput(const char *data, int32_t len)
{
    for (int32_t i = 0; i < len; i++) {
        putchar(*(data + i));
    }
    (void)fflush(stdout);
    return len;
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
        BShellInfo info = {PARAM_SHELL_DEFAULT_PROMPT, ShellInput, ShellOuput};
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

    SetInitLogLevel(0);
    BSH_LOGV("BShellEnvStart %d", argc);
    do {
        if (g_handle == NULL) {
            BShellInfo info = {PARAM_SHELL_DEFAULT_PROMPT, ShellInput, ShellOuput};
            BShellEnvInit(&g_handle, &info);
        }
        const ParamInfo *param = BShellEnvGetReservedParam(g_handle, PARAM_REVERESD_NAME_CURR_PARAMETER);
        BSH_CHECK(param != NULL && param->type == PARAM_STRING, break, "Failed to get revered param");
        BShellEnvSetParam(g_handle, param->name, param->desc, param->type, (void *)"");
        if (argc > 1) {
            int ret = SetParamShellPrompt(g_handle, args[1]);
            if (ret != 0) {
                break;
            }
        }
        BShellParamCmdRegister(g_handle, 1);
#ifdef INIT_TEST
        BShellCmdRegister(g_handle, 1);
#endif
        BShellEnvStart(g_handle);
        BShellEnvLoop(g_handle);
    } while (0);
    demoExit();
    tcsetattr(0, TCSAFLUSH, &terminalState);
    return 0;
}
#endif
