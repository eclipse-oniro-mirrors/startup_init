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
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "plugin_test.h"
#include "init_param.h"
#include "init_plugin_engine.h"

#define MAX_COUNT 1000
#define TEST_CMD_NAME "param_randrom_write"
static int g_testCmdIndex = 0;
static int PluginParamCmdWriteParam(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("PluginParamCmdWriteParam %d %s", id, name);
    PLUGIN_CHECK(argv != NULL && argc >= 1, return -1, "Invalid install parameter");
    PLUGIN_LOGI("PluginParamCmdWriteParam argc %d %s", argc, argv[0]);
    int maxCount = MAX_COUNT;
    if (argc > 1) {
        maxCount = atoi(argv[1]);
    }
    (void)srand((unsigned)time(NULL));
    char buffer[32] = { 0 }; // 32 buffer size
    int count = 0;
    while (count < maxCount) {
        const int wait = READ_DURATION + READ_DURATION; // 100ms
        int ret = sprintf_s(buffer, sizeof(buffer), "%d", count);
        if (ret > 0) {
            SystemWriteParam(argv[0], buffer);
            usleep(wait);
        }
        count++;
    }
    return 0;
}

static int PluginParamTestInit(void)
{
    g_testCmdIndex = AddCmdExecutor(TEST_CMD_NAME, PluginParamCmdWriteParam);
    PLUGIN_LOGI("PluginParamTestInit %d", g_testCmdIndex);
    return 0;
}

static void PluginParamTestExit(void)
{
    RemoveCmdExecutor(TEST_CMD_NAME, g_testCmdIndex);
}

PLUGIN_CONSTRUCTOR(void)
{
    PluginRegister("pluginparamtest",
            "/system/etc/plugin/plugin_param_test.cfg",
            PluginParamTestInit, PluginParamTestExit);
    PLUGIN_LOGI("PLUGIN_CONSTRUCTOR pluginInterface %p",
        g_pluginInterface->pluginRegister);
}
