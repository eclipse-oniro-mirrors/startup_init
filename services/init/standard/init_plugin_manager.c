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
#include "init_plugin_manager.h"

#include <dlfcn.h>

#include "cJSON.h"
#include "init_param.h"
#include "init_utils.h"
#include "init_log.h"
#include "init_group_manager.h"
#include "init_plugin.h"
#include "init_service_manager.h"
#include "securec.h"

#define MAX_CMD_ARGC 10
static int g_cmdExecutorId = 0;
static int g_cmdId = 0;
int AddCmdExecutor(const char *cmdName, CmdExecutor execCmd)
{
    INIT_ERROR_CHECK(cmdName != NULL, return -1, "Invalid input param");
    INIT_LOGI("AddCmdExecutor %s", cmdName);
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
        ListInit(&cmd->cmdExecutor);
    }
    if (execCmd == NULL) {
        return 0;
    }
    PluginCmdExecutor *cmdExec = (PluginCmdExecutor *)calloc(1, sizeof(PluginCmdExecutor));
    INIT_ERROR_CHECK(cmdExec != NULL, return -1, "Failed to create cmd listener");
    ListInit(&cmdExec->node);
    cmdExec->id = ++g_cmdExecutorId;
    cmdExec->execCmd = execCmd;
    ListAddTail(&cmd->cmdExecutor, &cmdExec->node);
    return cmdExec->id;
}

static void RemoveCmdExecutor(const char *cmdName, int id)
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
            ListRemove(&cmdExec->node);
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
    INIT_LOGI("PluginExecCmd index %s", cmd->name);
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

void PluginExecCmdByCmdIndex(int index, const char *cmdContent)
{
    int hashCode = ((index >> 16) & 0x0000ffff) - 1; // 16 left shift
    int cmdId = (index & 0x0000ffff);
    HashNode *node = HashMapFind(GetGroupHashMap(NODE_TYPE_CMDS),
        hashCode, (const void *)&cmdId, CompareCmdId);
    if (node == NULL) {
        return;
    }
    InitGroupNode *groupNode = HASHMAP_ENTRY(node, InitGroupNode, hashNode);
    if (groupNode == NULL || groupNode->data.cmd == NULL) {
        return;
    }
    PluginCmd *cmd = groupNode->data.cmd;
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
    INIT_LOGI("PluginGetCmdIndex content: %s index %d", cmd->name, *index);
    return cmd->name;
}

static int LoadModule(const char *name, const char *libName)
{
    char path[128] = {0}; // 128 path for plugin
    int ret = 0;
    if (libName == NULL) {
        ret = sprintf_s(path, sizeof(path), "%s/lib%s.z.so", DEFAULT_PLUGIN_PATH, name);
    } else {
        ret = sprintf_s(path, sizeof(path), "%s/%s", DEFAULT_PLUGIN_PATH, libName);
    }
    INIT_ERROR_CHECK(ret > 0, return -1, "Failed to sprintf path %s", name);
    char *realPath = GetRealPath(path);
    void* handle = dlopen(realPath, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    if (handle == NULL) {
        INIT_LOGE("Failed to load module %s, err %s", realPath, dlerror());
        free(realPath);
        return -1;
    }
    free(realPath);
    dlclose(handle);
    return 0;
}

int PluginInstall(const char *name, const char *libName)
{
    // load so, and init module
    int ret = LoadModule(name, libName);
    INIT_ERROR_CHECK(ret == 0, return -1, "pluginInit is null %s", name);

    PluginInfo *info = NULL;
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_PLUGINS, name);
    if (groupNode != NULL && groupNode->data.pluginInfo != NULL) {
        info = groupNode->data.pluginInfo;
    }
    INIT_ERROR_CHECK(info != NULL, return -1, "Failed to pluginInit %s", name);
    INIT_LOGI("PluginInstall %s %d", name, info->state);
    if (info->state == PLUGIN_STATE_INIT) {
        INIT_ERROR_CHECK(info->pluginInit != NULL, return -1, "pluginInit is null %s", name);
        ret = info->pluginInit();
        INIT_ERROR_CHECK(ret == 0, return -1, "Failed to pluginInit %s", name);
        info->state = PLUGIN_STATE_RUNNING;
    }
    return 0;
}

int PluginUninstall(const char *name)
{
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_PLUGINS, name);
    INIT_ERROR_CHECK(groupNode != NULL && groupNode->data.pluginInfo != NULL,
        return 0, "Can not find plugin %s", name);
    PluginInfo *info = groupNode->data.pluginInfo;
    INIT_ERROR_CHECK(info != NULL, return -1, "Failed to pluginInit %s", name);

    INIT_LOGI("PluginUninstall %s %d %p", name, info->state, info->pluginInit);
    if (info->state == PLUGIN_STATE_RUNNING) {
        INIT_ERROR_CHECK(info->pluginExit != NULL, return -1, "pluginExit is null %s", name);
        info->pluginExit();
        info->state = PLUGIN_STATE_INIT;
    }
    return 0;
}

static PluginInfo *GetPluginInfo(const char *name)
{
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_PLUGINS, name);
    if (groupNode == NULL) {
        groupNode = AddGroupNode(NODE_TYPE_PLUGINS, name);
        INIT_ERROR_CHECK(groupNode != NULL, return NULL, "Failed to create group node");
    }
    PluginInfo *info = groupNode->data.pluginInfo;
    if (info == NULL) {
        info = (PluginInfo *)calloc(1, sizeof(PluginInfo));
        INIT_ERROR_CHECK(info != NULL, return NULL, "Failed to create module");
        groupNode->data.pluginInfo = info;
        info->name = groupNode->name;
        info->state = 0;
        info->startMode = 0;
        info->libName = NULL;
    }
    return info;
}

static int PluginRegister(const char *name, const char *config, int (*pluginInit)(void), void (*pluginExit)(void))
{
    INIT_LOGI("PluginRegister %s %p %p", name, pluginInit, pluginExit);
    INIT_ERROR_CHECK(name != NULL, return -1, "Invalid plugin name");
    INIT_ERROR_CHECK(pluginInit != NULL && pluginExit != NULL,
        return -1, "Invalid plugin constructor %s", name);
    InitServiceSpace();
    PluginInfo *info = GetPluginInfo(name);
    INIT_ERROR_CHECK(info != NULL, return -1, "Failed to create group node");
    info->state = PLUGIN_STATE_INIT;
    info->pluginInit = pluginInit;
    info->pluginExit = pluginExit;

    // load config
    if (config != NULL) {
        ParseInitCfg(config, NULL);
    }
    return 0;
}

static int PluginCmdInstall(int id, const char *name, int argc, const char **argv)
{
    INIT_ERROR_CHECK(argv != NULL && argc >= 1, return -1, "Invalid install parameter");
    PluginInfo *info = GetPluginInfo(argv[0]);
    int ret = 0;
    if (info == NULL) {
        ret = PluginInstall(argv[0], NULL);
    } else {
        ret = PluginInstall(argv[0], info->libName);
    }
    INIT_ERROR_CHECK(ret == 0, return ret, "Install plugin %s fail", argv[0]);
    return 0;
}

static int PluginCmdUninstall(int id, const char *name, int argc, const char **argv)
{
    INIT_ERROR_CHECK(argv != NULL && argc >= 1, return -1, "Invalid install parameter");
    int ret = PluginUninstall(argv[0]);
    INIT_ERROR_CHECK(ret == 0, return ret, "Uninstall plugin %s fail", argv[0]);
    return 0;
}

static int LoadPluginCfg()
{
    char *fileBuf = ReadFileToBuf(DEFAULT_PLUGIN_CFG);
    INIT_ERROR_CHECK(fileBuf != NULL, return -1, "Failed to read file content %s", DEFAULT_PLUGIN_CFG);
    cJSON *root = cJSON_Parse(fileBuf);
    INIT_ERROR_CHECK(root != NULL, free(fileBuf);
        return -1, "Failed to parse json file %s", DEFAULT_PLUGIN_CFG);
    int itemNumber = 0;
    cJSON *json = GetArrayItem(root, &itemNumber, "modules");
    if (json == NULL) {
        free(fileBuf);
        return 0;
    }
    for (int i = 0; i < itemNumber; ++i) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        char *moduleName = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
        if (moduleName == NULL) {
            INIT_LOGE("Failed to get plugin info");
            continue;
        }
        PluginInfo *info = GetPluginInfo(moduleName);
        if (info == NULL) {
            INIT_LOGE("Failed to get plugin for module %s", moduleName);
            continue;
        }

        info->startMode = 0;
        char *mode = cJSON_GetStringValue(cJSON_GetObjectItem(item, "start-mode"));
        if (mode == NULL || (strcmp(mode, "static") != 0)) {
            info->startMode = 1;
        }
        char *libName = cJSON_GetStringValue(cJSON_GetObjectItem(item, "lib-name"));
        if (libName != NULL) {
            info->libName = strdup(libName); // do not care error
        }
        INIT_LOGI("LoadPluginCfg module %s %d libName %s", moduleName, info->startMode, info->libName);
        if (info->startMode == 0) {
            PluginInstall(moduleName, info->libName);
        }
    }
    free(fileBuf);
    return 0;
}

void PluginManagerInit(void)
{
    // "ohos.servicectrl.install"
    (void)AddCmdExecutor("install", PluginCmdInstall);
    (void)AddCmdExecutor("uninstall", PluginCmdUninstall);
    // register interface
    SetPluginInterface();
    // read cfg and start static plugin
    LoadPluginCfg();
}

int SetPluginInterface(void)
{
    static PluginInterface *pluginInterface = NULL;
    if (pluginInterface != NULL) {
        return 0;
    }
    char *realPath = GetRealPath("/system/lib/libplugin.z.so");
#ifndef STARTUP_INIT_TEST
    void* handle = dlopen(realPath, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    if (handle == NULL) {
        INIT_LOGE("Failed to load module %s, err %s", realPath, dlerror());
        free(realPath);
        return -1;
    }
    GetPluginInterfaceFunc getPluginInterface = (GetPluginInterfaceFunc)dlsym(handle, "GetPluginInterface");
#else
    GetPluginInterfaceFunc getPluginInterface = GetPluginInterface;
#endif
    INIT_LOGI("PluginManagerInit getPluginInterface %p ", getPluginInterface);
    if (getPluginInterface != NULL) {
        pluginInterface = getPluginInterface();
        if (pluginInterface != NULL) {
            pluginInterface->pluginRegister = PluginRegister;
            pluginInterface->addCmdExecutor = AddCmdExecutor;
            pluginInterface->removeCmdExecutor = RemoveCmdExecutor;
            pluginInterface->systemWriteParam = SystemWriteParam;
            pluginInterface->systemReadParam = SystemReadParam;
            pluginInterface->securityLabelSet = NULL;
        }
        INIT_LOGI("PluginManagerInit pluginInterface %p %p %p",
            pluginInterface, pluginInterface->pluginRegister, getPluginInterface);
    }
    free(realPath);
#ifndef STARTUP_INIT_TEST
    dlclose(handle);
#endif
    return 0;
}
