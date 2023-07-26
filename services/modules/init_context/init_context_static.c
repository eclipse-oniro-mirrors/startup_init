/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "init_context.h"

#include "init_module_engine.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"

static SubInitContext g_subInitContext[INIT_CONTEXT_MAIN] = { 0 };
static ConfigContext g_currContext = { INIT_CONTEXT_MAIN };
static int g_subInitRunning = 0;

int InitSubInitContext(InitContextType type, const SubInitContext *context)
{
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return -1, "Invalid type %d", type);
    PLUGIN_CHECK(context != NULL, return -1, "Invalid context %d", type);
    g_subInitContext[type].type = type;
    g_subInitContext[type].startSubInit = context->startSubInit;
    g_subInitContext[type].stopSubInit = context->stopSubInit;
    g_subInitContext[type].executeCmdInSubInit = context->executeCmdInSubInit;
    g_subInitContext[type].setSubInitContext = context->setSubInitContext;
    return 0;
}

void StopSubInit(pid_t pid)
{
    if (g_subInitContext[0].stopSubInit != NULL) {
        g_subInitContext[0].stopSubInit(pid);
    }
}

int StartSubInit(InitContextType type)
{
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return -1, "Invalid type %d", type);
    if (!g_subInitRunning) {
        // install init context
#ifndef STARTUP_INIT_TEST
        InitModuleMgrInstall("init_context");
#endif
    }
    PLUGIN_CHECK(g_subInitContext[type].startSubInit != NULL, return -1, "Invalid context %d", type);
    g_subInitRunning = 1;
    return g_subInitContext[type].startSubInit(type);
}

int ExecuteCmdInSubInit(const ConfigContext *context, const char *name, const char *cmdContent)
{
    PLUGIN_CHECK(context != NULL, return -1, "Invalid context");
    PLUGIN_CHECK(name != NULL, return -1, "Invalid name");
    PLUGIN_CHECK(context->type < INIT_CONTEXT_MAIN, return -1, "Invalid type %d", context->type);
    PLUGIN_LOGV("Execute command '%s %s' in context %d", name, cmdContent, context->type);
    StartSubInit(context->type);
    PLUGIN_CHECK(g_subInitContext[context->type].executeCmdInSubInit != NULL,
        return -1, "Invalid context %d", context->type);
    return g_subInitContext[context->type].executeCmdInSubInit(context->type, name, cmdContent);
}

int SetSubInitContext(const ConfigContext *context, const char *service)
{
    if (InUpdaterMode() == 1) { // not support sub init in update mode
        return 0;
    }
    PLUGIN_CHECK(context != NULL, return -1, "Invalid context");
    if (context->type >= INIT_CONTEXT_MAIN) {
        g_currContext.type = INIT_CONTEXT_MAIN;
        return 0;
    }
    g_currContext.type = context->type;
    PLUGIN_CHECK(g_subInitContext[context->type].setSubInitContext != NULL,
        return -1, "Invalid context %d", context->type);
    PLUGIN_LOGI("Set selinux context %d for %s", context->type, service);
    return g_subInitContext[context->type].setSubInitContext(context->type);
}

int CheckExecuteInSubInit(const ConfigContext *context)
{
    if (InUpdaterMode() == 1) { // not support sub init in update mode
        return 0;
    }
#ifdef INIT_SUPPORT_CHIPSET_INIT
    return !(context == NULL || context->type == INIT_CONTEXT_MAIN || g_currContext.type != INIT_CONTEXT_MAIN);
#else
    return 0;
#endif
}

#ifdef STARTUP_INIT_TEST
SubInitContext *GetSubInitContext(InitContextType type)
{
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return NULL, "Invalid type %d", type);
    return &g_subInitContext[type];
}
#endif
