/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "cJSON.h"
#include "init_param.h"
#include "init_utils.h"
#include "init_log.h"
#include "init_group_manager.h"
#include "init_service_manager.h"
#include "securec.h"
#include "modulemgr.h"
#include "init_module_engine.h"

#define MAX_CMD_ARGC 10
static int g_cmdExecutorId = 0;
static int g_cmdId = 0;
int AddCmdExecutor(const char *cmdName, CmdExecutor execCmd)
{
    INIT_ERROR_CHECK(cmdName != NULL, return -1, "Invalid input param");
    INIT_LOGV("Add command '%s' executor.", cmdName);
    PluginCmd *cmd = NULL;
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_CMDS, cmdName);
    if (groupNode == NULL) {
        groupNode = AddGroupNode(NODE_TYPE_CMDS, cmdName);
        INIT_ERROR_CHECK(groupNode != NULL, return -1, "Failed to create group node");
    }
    cmd = groupNode->data.cmd;
    if (cmd == NULL) {
        cmd = (PluginCmd *)calloc(1, sizeof(PluginCmd));
        INIT_ERROR_CHECK(cmd != NULL, return -1, "Failed to create cmd condition");
        groupNode->data.cmd = cmd;
        cmd->cmdId = g_cmdId++;
        cmd->name = groupNode->name;
        OH_ListInit(&cmd->cmdExecutor);
    }
    if (execCmd == NULL) {
        return 0;
    }
    PluginCmdExecutor *cmdExec = (PluginCmdExecutor *)calloc(1, sizeof(PluginCmdExecutor));
    INIT_ERROR_CHECK(cmdExec != NULL, return -1, "Failed to create cmd listener");
    OH_ListInit(&cmdExec->node);
    cmdExec->id = ++g_cmdExecutorId;
    cmdExec->execCmd = execCmd;
    OH_ListAddTail(&cmd->cmdExecutor, &cmdExec->node);
    return cmdExec->id;
}

void RemoveCmdExecutor(const char *cmdName, int id)
{
    INIT_ERROR_CHECK(cmdName != NULL, return, "Invalid input param");
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_CMDS, cmdName);
    INIT_ERROR_CHECK(groupNode != NULL && groupNode->data.cmd != NULL,
        return, "Can not find cmd %s", cmdName);

    PluginCmd *cmd = groupNode->data.cmd;
    ListNode *node = cmd->cmdExecutor.next;
    while (node != &cmd->cmdExecutor) {
        PluginCmdExecutor *cmdExec = ListEntry(node, PluginCmdExecutor, node);
        if (cmdExec->id == id) {
            OH_ListRemove(&cmdExec->node);
            free(cmdExec);
            break;
        }
        node = node->next;
    }
    if (cmd->cmdExecutor.next != &cmd->cmdExecutor) {
        return;
    }
    DelGroupNode(NODE_TYPE_CMDS, cmdName);
    free(cmd);
}

void PluginExecCmd_(PluginCmd *cmd, const char *cmdContent)
{
    const struct CmdArgs *ctx = GetCmdArg(cmdContent, " ", MAX_CMD_ARGC);
    if (ctx == NULL) {
        INIT_LOGE("Invalid arguments cmd: %s content: %s", cmd->name, cmdContent);
        return;
    } else if (ctx->argc > MAX_CMD_ARGC) {
        INIT_LOGE("Invalid arguments cmd: %s content: %s argc: %d ",
            cmd->name, cmdContent, ctx->argc);
        FreeCmdArg((struct CmdArgs *)ctx);
        return;
    }
    INIT_LOGV("PluginExecCmd_ index %s content: %s", cmd->name, cmdContent);
    ListNode *node = cmd->cmdExecutor.next;
    while (node != &cmd->cmdExecutor) {
        PluginCmdExecutor *cmdExec = ListEntry(node, PluginCmdExecutor, node);
        cmdExec->execCmd(cmdExec->id, cmd->name, ctx->argc, (const char **)ctx->argv);
        node = node->next;
    }
    FreeCmdArg((struct CmdArgs *)ctx);
}

void PluginExecCmdByName(const char *name, const char *cmdContent)
{
    INIT_ERROR_CHECK(name != NULL, return, "Invalid cmd for %s", cmdContent);
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_CMDS, name);
    if (groupNode == NULL || groupNode->data.cmd == NULL) {
        return;
    }
    PluginCmd *cmd = groupNode->data.cmd;
    PluginExecCmd_(cmd, cmdContent);
}

int PluginExecCmd(const char *name, int argc, const char **argv)
{
    INIT_ERROR_CHECK(name != NULL, return -1, "Invalid cmd ");
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_CMDS, name);
    if (groupNode == NULL || groupNode->data.cmd == NULL) {
        return -1;
    }
    PluginCmd *cmd = groupNode->data.cmd;
    INIT_LOGV("PluginExecCmd index %s", cmd->name);
    int ret = 0;
    ListNode *node = cmd->cmdExecutor.next;
    while (node != &cmd->cmdExecutor) {
        PluginCmdExecutor *cmdExec = ListEntry(node, PluginCmdExecutor, node);
        ret = cmdExec->execCmd(cmdExec->id, cmd->name, argc, argv);
        node = node->next;
    }
    return ret;
}

static int CompareCmdId(const HashNode *node, const void *key)
{
    InitGroupNode *groupNode = HASHMAP_ENTRY(node, InitGroupNode, hashNode);
    if (groupNode == NULL || groupNode->data.cmd == NULL) {
        return 1;
    }
    PluginCmd *cmd = groupNode->data.cmd;
    return cmd->cmdId - *(int *)key;
}

static PluginCmd *GetPluginCmdByIndex(int index)
{
    int hashCode = (((unsigned int)index >> 16) & 0x0000ffff) - 1; // 16 left shift
    int cmdId = ((unsigned int)index & 0x0000ffff);
    HashNode *node = OH_HashMapFind(GetGroupHashMap(NODE_TYPE_CMDS),
        hashCode, (const void *)&cmdId, CompareCmdId);
    if (node == NULL) {
        return NULL;
    }
    InitGroupNode *groupNode = HASHMAP_ENTRY(node, InitGroupNode, hashNode);
    if (groupNode == NULL || groupNode->data.cmd == NULL) {
        return NULL;
    }
    return groupNode->data.cmd;
}

const char *GetPluginCmdNameByIndex(int index)
{
    PluginCmd *cmd = GetPluginCmdByIndex(index);
    if (cmd == NULL) {
        return NULL;
    }
    return cmd->name;
}

void PluginExecCmdByCmdIndex(int index, const char *cmdContent)
{
    PluginCmd *cmd = GetPluginCmdByIndex(index);
    if (cmd == NULL) {
        INIT_LOGW("Cannot find plugin command with index %d", index);
        return;
    }
    INIT_LOGV("Command: %s cmdContent: %s", cmd->name, cmdContent);
    PluginExecCmd_(cmd, cmdContent);
}

const char *PluginGetCmdIndex(const char *cmdStr, int *index)
{
    char cmdName[MAX_CMD_NAME_LEN] = {};
    int i = 0;
    while ((i < MAX_CMD_NAME_LEN) && (*(cmdStr + i) != '\0') && (*(cmdStr + i) != ' ')) {
        cmdName[i] = *(cmdStr + i);
        i++;
    }
    if (i >= MAX_CMD_NAME_LEN) {
        return NULL;
    }
    cmdName[i] = '\0';
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_CMDS, cmdName);
    if (groupNode == NULL || groupNode->data.cmd == NULL) {
        AddCmdExecutor(cmdName, NULL);
    }
    groupNode = GetGroupNode(NODE_TYPE_CMDS, cmdName);
    INIT_ERROR_CHECK(groupNode != NULL && groupNode->data.cmd != NULL,
        return NULL, "Failed to create pluginCmd %s", cmdName);

    PluginCmd *cmd = groupNode->data.cmd;
    int hashCode = GenerateHashCode(cmdName);
    hashCode = (hashCode < 0) ? -hashCode : hashCode;
    hashCode = hashCode % GROUP_HASHMAP_BUCKET;
    *index = ((hashCode + 1) << 16) | cmd->cmdId; // 16 left shift
    return cmd->name;
}
