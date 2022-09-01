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
#include <pthread.h>
#include <string.h>
#include <sys/time.h>

#include "begetctl.h"
#include "init_param.h"
#include "loop_event.h"
#include "parameter.h"
#include "plugin_test.h"
#include "service_watcher.h"

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
        int wait = READ_DURATION + READ_DURATION;  // 100ms rand
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
    long long commit = GetSystemCommitId();
    printf("Receive parameter commit %lld change %s %s \n", commit, key, value);
}

static int32_t BShellParamCmdWatch(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGV("BShellParamCmdWatch %s", argv[1]);
    static int index = 0;
    int ret = SystemWatchParameter(argv[1], HandleParamChange, (void *)index);
    if (ret != 0) {
        PLUGIN_LOGE("Failed to watch %s", argv[1]);
        return 0;
    }
    index++;
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
    if (argc > 2 && strcmp(argv[1], "wait") == 0) {                  // 2 service name index
        PLUGIN_LOGI("group-test-stage3: wait service %s", argv[2]);  // 2 service name index
        ServiceWatchForStatus(argv[2], ServiceStatusChangeTest);     // 2 service name index
        LE_RunLoop(LE_GetDefaultLoop());
    }
    return 0;
}

static int32_t BShellParamCmdUdidGet(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGI("BShellParamCmdUdidGet ");
    char localDeviceId[65] = {0};      // 65 udid len
    AclGetDevUdid(localDeviceId, 65);  // 65 udid len
    BShellEnvOutput(shell, "    udid: %s\r\n", localDeviceId);
    return 0;
}

int32_t BShellCmdRegister(BShellHandle shell, int execMode)
{
    if (execMode == 0) {
        const CmdInfo infos[] = {
            {"init", BShellParamCmdGroupTest, "init group test", "init group test [stage]", "init group test"},
        };
        for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
            BShellEnvRegisterCmd(shell, &infos[i]);
        }
    } else {
        const CmdInfo infos[] = {
            {"display", BShellParamCmdDisplay, "display system service", "display service", "display service"},
            {"read", BShellParamCmdRead, "read system parameter", "read [start | stop]", ""},
            {"watcher", BShellParamCmdWatch, "watcher system parameter", "watcher [name]", ""},
            {"install", BShellParamCmdInstall, "install plugin", "install [name]", ""},
            {"uninstall", BShellParamCmdInstall, "uninstall plugin", "uninstall [name]", ""},
            {"group", BShellParamCmdGroupTest, "group test", "group test [stage]", "group test"},
            {"display", BShellParamCmdUdidGet, "display udid", "display udid", "display udid"},
        };
        for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
            BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
        }
    }
    return 0;
}
