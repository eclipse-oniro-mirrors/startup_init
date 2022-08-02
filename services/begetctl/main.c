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

static BShellHandle g_handle = NULL;
BShellHandle GetShellHandle(void)
{
    if (g_handle == NULL) {
        BShellInfo info = {PARAM_SHELL_DEFAULT_PROMPT, NULL};
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
    if (number >= 1 && strcmp(args[0], "devctl") == 0) {
        if (memcpy_s(args[0], strlen(args[0]), "reboot", strlen("reboot")) != 0) {
            printf("Failed to copy\n");
        }
    }
    BShellHandle handle = GetShellHandle();
    if (handle == NULL) {
        printf("Failed to get shell handle \n");
        return 0;
    }

    BShellParamCmdRegister(handle, 0);
#ifdef INIT_TEST
    BShellCmdRegister(handle, 0);
#endif
    BShellEnvDirectExecute(handle, number, args);
    demoExit();
    return 0;
}
