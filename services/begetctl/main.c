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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "begetctl.h"

struct CMD_LIST_ST {
    struct CMD_LIST_ST *next;
    const char *name;
    BegetCtlCmdPtr cmd;
};

static struct CMD_LIST_ST *m_cmdList = NULL;

int BegetCtlCmdAdd(const char *name, BegetCtlCmdPtr cmd)
{
    if (name == NULL) {
        return -1;
    }

    struct CMD_LIST_ST *item = (struct CMD_LIST_ST *)malloc(sizeof(struct CMD_LIST_ST));
    if (item == NULL) {
        return -1;
    }

    item->name = strdup(name);
    if (item->name == NULL) {
        free((void *)item);
        return -1;
    }
    item->cmd = cmd;

    item->next = m_cmdList;
    m_cmdList = item;

    return 0;
}

static const struct CMD_LIST_ST *BegetCtlCmdFind(const char *name)
{
    const struct CMD_LIST_ST *item = m_cmdList;

    while (item != NULL) {
        if (strcmp(item->name, name) == 0) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

static void BegetCtlUsage(const char *command)
{
    const struct CMD_LIST_ST *item = m_cmdList;
    int notFirst = 0;

    while (item != NULL) {
        if (notFirst) {
            printf(" ");
        }
        notFirst = 1;
        printf("%s", item->name);
        item = item->next;
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *last = strrchr(argv[0], '/');

    // Get the first ending command name
    if (last != NULL) {
        last = last + 1;
    } else {
        last = argv[0];
    }

    // If it is begetctl with subcommand name, try to do subcommand first
    if ((argc > 1) && (strcmp(last, "begetctl") == 0)) {
        argc = argc - 1;
        argv = argv + 1;
        last = argv[0];
    }

    // Match the command
    const struct CMD_LIST_ST *cmd = BegetCtlCmdFind(last);
    if (cmd == NULL) {
        BegetCtlUsage(last);
        return 0;
    }

    // Do command
    return cmd->cmd(argc, argv);
}
