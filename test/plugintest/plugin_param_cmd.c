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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "begetctl.h"
#include "loop_event.h"
#include "plugin_test.h"
#include "service_watcher.h"
#include "shell_utils.h"
#include "sys_param.h"

#define READ_DURATION 100000
static char *GetLocalBuffer(uint32_t *buffSize)
{
    static char buffer[PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX] = {0};
    if (buffSize != NULL) {
        *buffSize = sizeof(buffer);
    }
    return buffer;
}

int g_stop = 0;
static void *CmdReader(void *args)
{
    (void)srand((unsigned)time(NULL));
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    while (g_stop == 0) {
        int wait = READ_DURATION + READ_DURATION; // 100ms rand
        uint32_t size = buffSize;
        int ret = SystemGetParameter("test.randrom.read", buffer, &size);
        if (ret == 0) {
            printf("SystemGetParameter value %s %d \n", buffer, wait);
        } else {
            printf("SystemGetParameter fail %d \n", wait);
        }
        usleep(wait);
    }
    return NULL;
}

static int32_t BShellParamCmdRead(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    static pthread_t thread = 0;
    PLUGIN_LOGV("BShellParamCmdWatch %s, threadId %d", argv[1], thread);
    if (strcmp(argv[1], "start") == 0) {
        if (thread != 0) {
            return 0;
        }
        SystemSetParameter("test.randrom.read.start", "1");
        pthread_create(&thread, NULL, CmdReader, argv[1]);
    } else if (strcmp(argv[1], "stop") == 0) {
        if (thread == 0) {
            return 0;
        }
        SystemSetParameter("test.randrom.read.start", "0");
        g_stop = 1;
        pthread_join(thread, NULL);
        thread = 0;
    }
    return 0;
}

static void HandleParamChange(const char *key, const char *value, void *context)
{
    PLUGIN_CHECK(key != NULL && value != NULL, return, "Invalid parameter");
    printf("Receive parameter change %s %s \n", key, value);
}

static void *CmdWatcher(void *args)
{
    char *name = (char *)args;
    int ret = SystemWatchParameter(name, HandleParamChange, NULL);
    if (ret != 0) {
        return 0;
    }
    while (1) {
        pause();
    }
    return NULL;
}

static int32_t BShellParamCmdWatch(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGV("BShellParamCmdWatch %s", argv[1]);
    pthread_t thread;
    pthread_create(&thread, NULL, CmdWatcher, argv[1]);
    return 0;
}

static int32_t BShellParamCmdInstall(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGV("BShellParamCmdInstall %s %s", argv[0], argv[1]);
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    int ret = sprintf_s(buffer, buffSize, "ohos.servicectrl.%s", argv[0]);
    PLUGIN_CHECK(ret > 0, return -1, "Invalid buffer");
    buffer[ret] = '\0';
    SystemSetParameter(buffer, argv[1]);
    return 0;
}

static int32_t BShellParamCmdDisplay(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGV("BShellParamCmdDisplay %s %s", argv[0], argv[1]);
    SystemSetParameter("ohos.servicectrl.display", argv[1]);
    return 0;
}

void ServiceStatusChangeTest(const char *key, ServiceStatus status)
{
    PLUGIN_LOGI("group-test-stage3: wait service %s status: %d", key, status);
    if (status == SERVICE_READY || status == SERVICE_STARTED) {
        PLUGIN_LOGI("Service %s start work", key);
    }
}

static int32_t BShellParamCmdGroupTest(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGI("BShellParamCmdGroupTest %s stage: %s", argv[0], argv[1]);
    if (argc > 2 && strcmp(argv[1], "wait") == 0) { // 2 service name index
        PLUGIN_LOGI("group-test-stage3: wait service %s", argv[2]); // 2 service name index
        ServiceWatchForStatus(argv[2], ServiceStatusChangeTest); // 2 service name index
        LE_RunLoop(LE_GetDefaultLoop());
    }
    return 0;
}

int32_t BShellCmdRegister(BShellHandle shell, int execMode)
{
    if (execMode == 0) {
        CmdInfo infos[] = {
            {"init", BShellParamCmdGroupTest, "init group test", "init group test [stage]", "init group test"},
        };
        for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
            BShellEnvRegitsterCmd(shell, &infos[i]);
        }
    } else {
        CmdInfo infos[] = {
            {"display", BShellParamCmdDisplay, "display system service", "display service", "display service"},
            {"read", BShellParamCmdRead, "read system parameter", "read [start | stop]", ""},
            {"watcher", BShellParamCmdWatch, "watcher system parameter", "watcher [name]", ""},
            {"install", BShellParamCmdInstall, "install plugin", "install [name]", ""},
            {"uninstall", BShellParamCmdInstall, "uninstall plugin", "uninstall [name]", ""},
            {"group", BShellParamCmdGroupTest, "group test", "group test [stage]", "group test"},
        };
        for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
            BShellEnvRegitsterCmd(GetShellHandle(), &infos[i]);
        }
    }
    return 0;
}
