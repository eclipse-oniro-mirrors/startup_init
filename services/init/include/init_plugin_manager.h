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
#ifndef STARTUP_INIT_PULUGIN_MANAGER_H
#define STARTUP_INIT_PULUGIN_MANAGER_H
#include <stdlib.h>
#include <string.h>

#include "init_plugin.h"
#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define DEFAULT_PLUGIN_PATH "/system/lib/plugin"
#define DEFAULT_PLUGIN_CFG "/system/etc/plugin_modules.cfg"
typedef enum {
    PLUGIN_STATE_IDLE,
    PLUGIN_STATE_INIT,
    PLUGIN_STATE_RUNNING,
    PLUGIN_STATE_DESTORYED
} PluginState;

typedef struct PluginInfo_ {
    int state;
    int startMode;
    int (*pluginInit)();
    void (*pluginExit)();
    char *name;
    char *libName;
} PluginInfo;

typedef struct {
    ListNode cmdExecutor;
    int cmdId;
    char *name;
} PluginCmd;

typedef int (*CmdExecutor)(int id, const char *name, int argc, const char **argv);
typedef struct {
    ListNode node;
    int id;
    CmdExecutor execCmd;
} PluginCmdExecutor;

void PluginExecCmdByName(const char *name, const char *cmdContent);
void PluginExecCmdByCmdIndex(int index, const char *cmdContent);
int PluginExecCmd(const char *name, int argc, const char **argv);
const char *PluginGetCmdIndex(const char *cmdStr, int *index);

int PluginUninstall(const char *name);
int PluginInstall(const char *name, const char *libName);
void PluginManagerInit(void);

int AddCmdExecutor(const char *cmdName, CmdExecutor execCmd);

int ParseInitCfg(const char *configFile, void *context);
typedef PluginInterface *(*GetPluginInterfaceFunc)();
int SetPluginInterface(void);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // STARTUP_INIT_PULUGIN_MANAGER_H
