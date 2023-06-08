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
#include <stdlib.h>

#include "init_param.h"
#include "modulemgr.h"
#include "init_module_engine.h"
#include "securec.h"

#define MAX_COUNT 1000
#define TEST_CMD_NAME "param_randrom_write"

#define READ_DURATION 100000

static int g_testCmdIndex = 0;
static int PluginParamCmdWriteParam(int id, const char *name, int argc, const char **argv)
{
    if ((argv == NULL) || (argc < 1)) {
        return -1;
    }
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

MODULE_CONSTRUCTOR(void)
{
    g_testCmdIndex = AddCmdExecutor(TEST_CMD_NAME, PluginParamCmdWriteParam);
}

MODULE_DESTRUCTOR(void)
{
    RemoveCmdExecutor(TEST_CMD_NAME, g_testCmdIndex);
}
