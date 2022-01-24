/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "begetctl.h"
#include "shell.h"
#include "shell_utils.h"
#include "sys_param.h"

static BShellHandle g_handle = NULL;
static int32_t ShellOuput(const char *data, int32_t len)
{
    for (int32_t i = 0; i < len; i++) {
        putchar(*(data + i));
    }
    (void)fflush(stdout);
    return len;
}

BShellHandle GetShellHandle(void)
{
    if (g_handle == NULL) {
        BShellInfo info = {PARAM_SHELL_DEFAULT_PROMPT, NULL, ShellOuput};
        BShellEnvInit(&g_handle, &info);
    }
    return g_handle;
}

static void signalHandler(int signal)
{
    demoExit();
    exit(0);
}

int main(int argc, char *argv[])
{
    (void)signal(SIGINT, signalHandler);
    const char *last = strrchr(argv[0], '/');
    // Get the first ending command name
    if (last != NULL) {
        last = last + 1;
    } else {
        last = argv[0];
    }

    // If it is begetctl with subcommand name, try to do subcommand first
    int number = argc;
    char **args = argv;
    if ((argc > 1) && (strcmp(last, "begetctl") == 0)) {
        number = argc - 1;
        args = argv + 1;
    }
    SetInitLogLevel(0);
    BShellParamCmdRegister(g_handle, 0);
    BShellEnvDirectExecute(g_handle, number, args);
    demoExit();
    return 0;
}
