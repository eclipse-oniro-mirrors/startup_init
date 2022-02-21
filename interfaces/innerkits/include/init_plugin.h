/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_INIT_PLUGIN_H
#define BASE_STARTUP_INIT_PLUGIN_H
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PLUGIN_CONSTRUCTOR(void) static void _init(void) __attribute__((constructor)); static void _init(void)
#define PLUGIN_DESTRUCTOR(void) static void _destroy(void) __attribute__((destructor)); static void _destroy(void)

typedef struct {
    int (*pluginRegister)(const char *name, const char *config, int (*pluginInit)(void), void (*pluginExit)(void));
    int (*addCmdExecutor)(const char *cmdName,
        int (*CmdExecutor)(int id, const char *name, int argc, const char **argv));
    void (*removeCmdExecutor)(const char *cmdName, int id);
    int (*systemWriteParam)(const char *name, const char *value);
    int (*systemReadParam)(const char *name, char *value, unsigned int *len);
    int (*securityLabelSet)(const char *name, const char *label, const char *paraType);
} PluginInterface;

PluginInterface *GetPluginInterface(void);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_INIT_PLUGIN_H
