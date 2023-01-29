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
#include "init_utils.h"
#include "loop_event.h"
#include "parameter.h"
#include "plugin_test.h"
#include "service_watcher.h"
#include "parameter.h"

#define MAX_THREAD_NUMBER 100
#define MAX_NUMBER 10
#define READ_DURATION 100000
#define CMD_INDEX 2

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

typedef struct {
    char name[PARAM_NAME_LEN_MAX];
    pthread_t thread;
} TestWatchContext;

static void HandleParamChange2(const char *key, const char *value, void *context)
{
    PLUGIN_CHECK(key != NULL && value != NULL, return, "Invalid parameter");
    long long commit = GetSystemCommitId();
    size_t index = (size_t)context;
    printf("[%zu] Receive parameter commit %lld change %s %s \n", index, commit, key, value);
    static int addWatcher = 0;
    int ret = 0;
    if ((index == 4) && !addWatcher) { // 4 when context == 4 add
        index = 5; // 5 add context
        ret = SystemWatchParameter(key, HandleParamChange2, (void *)index);
        if (ret != 0) {
            printf("Add watcher %s fail %zu \n", key, index);
        }
        addWatcher = 1;
        return;
    }
    if (index == 2) { // 2 when context == 2 delete 3
        index = 3; // 3 delete context
        RemoveParameterWatcher(key, HandleParamChange2, (void *)index);
        if (ret != 0) {
            printf("Remove watcher fail %zu  \n", index);
        }
        return;
    }
    if (index == 1) { // 1 when context == 1 delete 1
        RemoveParameterWatcher(key, HandleParamChange2, (void *)index);
        if (ret != 0) {
            printf("Remove watcher fail %zu  \n", index);
        }
        return;
    }
    if ((index == 5) && (addWatcher == 1)) {  // 5 when context == 5 delete 5
        RemoveParameterWatcher(key, HandleParamChange2, (void *)index);
        if (ret != 0) {
            printf("Remove watcher fail %zu  \n", index);
        }
        addWatcher = 0;
    }
}

static void HandleParamChange1(const char *key, const char *value, void *context)
{
    PLUGIN_CHECK(key != NULL && value != NULL, return, "Invalid parameter");
    long long commit = GetSystemCommitId();
    size_t index = (size_t)context;
    printf("[%zu] Receive parameter commit %lld change %s %s \n", index, commit, key, value);
}

static void *CmdThreadWatcher(void *args)
{
    TestWatchContext *context = (TestWatchContext *)args;
    for (size_t i = 1; i <= MAX_NUMBER; i++) {
        int ret = SystemWatchParameter(context->name, HandleParamChange2, (void *)i);
        if (ret != 0) {
            printf("Add watcher %s fail %zu \n", context->name, i);
        }
        ret = SetParameter(context->name, context->name);
        if (ret != 0) {
            printf("Set parameter %s fail %zu \n", context->name, i);
        }
    }
    sleep(1);
    for (size_t i = 1; i <= MAX_NUMBER; i++) {
        int ret = RemoveParameterWatcher(context->name, HandleParamChange2, (void *)i);
        if (ret != 0) {
            printf("Remove watcher %s fail %zu \n", context->name, i);
        }
    }
    free(context);
    return NULL;
}

static void StartWatcherInThread(const char *prefix)
{
    TestWatchContext *context[MAX_THREAD_NUMBER] = { NULL };
    for (size_t i = 0; i < MAX_THREAD_NUMBER; i++) {
        context[i] = calloc(1, sizeof(TestWatchContext));
        PLUGIN_CHECK(context[i] != NULL, return, "Failed to alloc context");
        int len = sprintf_s(context[i]->name, sizeof(context[i]->name), "%s.%zu", prefix, i);
        if (len > 0) {
            printf("Add watcher %s \n", context[i]->name);
            pthread_create(&context[i]->thread, NULL, CmdThreadWatcher, context[i]);
        }
    }
}

static void StartWatcher(const char *prefix)
{
    static size_t index = 0;
    int ret = SystemWatchParameter(prefix, HandleParamChange1, (void *)index);
    if (ret != 0) {
        printf("Add watcher %s fail \n", prefix);
        return;
    }
    index++;
}

static int32_t BShellParamCmdWatch(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGV("BShellParamCmdWatch %s", argv[1]);
    StartWatcher(argv[1]);

    if (argc <= CMD_INDEX) {
        return 0;
    }
    if (strcmp(argv[CMD_INDEX], "thread") == 0) { // 2 cmd index
        StartWatcherInThread(argv[1]);
        return 0;
    }

    int maxCount = StringToInt(argv[CMD_INDEX], -1); // 2 cmd index
    if (maxCount <= 0 || maxCount > 65535) { // 65535 max count
        PLUGIN_LOGE("Invalid input %s", argv[CMD_INDEX]);
        return 0;
    }
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    size_t count = 0;
    while (count < (size_t)maxCount) { // 100 max count
        int len = sprintf_s(buffer, buffSize, "%s.%zu", argv[1], count);
        PLUGIN_CHECK(len > 0, return -1, "Invalid buffer");
        buffer[len] = '\0';
        StartWatcher(buffer);
        count++;
    }
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
        LE_CloseLoop(LE_GetDefaultLoop());
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

static int CalcValue(const char *value)
{
    char *begin = (char *)value;
    while (*begin != '\0') {
        if (*begin == ' ') {
            begin++;
        } else {
            break;
        }
    }
    char *end = begin + strlen(begin);
    while (end > begin) {
        if (*end > '9' || *end < '0') {
            *end = '\0';
        }
        end--;
    }
    return StringToInt(begin, -1);
}

static int32_t BShellParamCmdMemGet(BShellHandle shell, int32_t argc, char *argv[])
{
    PLUGIN_CHECK(argc > 1, return -1, "Invalid parameter");
    uint32_t buffSize = 0;  // 1024 max buffer for decode
    char *buff = GetLocalBuffer(&buffSize);
    PLUGIN_CHECK(buff != NULL, return -1, "Failed to get local buffer");
    int ret = sprintf_s(buff, buffSize - 1, "/proc/%s/smaps", argv[1]);
    PLUGIN_CHECK(ret > 0, return -1, "Failed to format path %s", argv[1]);
    buff[ret] = '\0';
    char *realPath = GetRealPath(buff);
    PLUGIN_CHECK(realPath != NULL, return -1, "Failed to get real path");
    int all = 0;
    if (argc > 2 && strcmp(argv[2], "all") == 0) {  // 2 2 max arg
        all = 1;
    }
    FILE *fp = fopen(realPath, "r");
    free(realPath);
    int value = 0;
    while (fp != NULL && buff != NULL && fgets(buff, buffSize, fp) != NULL) {
        buff[buffSize - 1] = '\0';
        if (strncmp(buff, "Pss:", strlen("Pss:")) == 0) {
            int v = CalcValue(buff + strlen("Pss:"));
            if (all) {
                printf("Pss:     %d kb\n", v);
            }
            value += v;
        } else if (strncmp(buff, "SwapPss:", strlen("SwapPss:")) == 0) {
            int v = CalcValue(buff + strlen("SwapPss:"));
            if (all) {
                printf("SwapPss: %d kb\n", v);
            }
            value += v;
        }
    }
    if (fp != NULL) {
        fclose(fp);
        printf("Total mem %d kb\n", value);
    } else {
        printf("Failed to get memory for %s %s \n", argv[1], buff);
    }
    return 0;
}

#define MAX_TEST 10000
static long long DiffLocalTime(struct timespec *startTime)
{
    struct timespec endTime;
    clock_gettime(CLOCK_MONOTONIC, &(endTime));
    long long diff = (long long)((endTime.tv_sec - startTime->tv_sec) * 1000000); // 1000000 1000ms
    if (endTime.tv_nsec > startTime->tv_nsec) {
        diff += (endTime.tv_nsec - startTime->tv_nsec) / 1000; // 1000 1ms
    } else {
        diff -= (startTime->tv_nsec - endTime.tv_nsec) / 1000; // 1000 1ms
    }
    return diff;
}

static void TestPerformance(const char *testParamName)
{
    struct timespec startTime;
    CachedHandle cacheHandle = CachedParameterCreate(testParamName, "true");
    clock_gettime(CLOCK_MONOTONIC, &(startTime));
    const char *value = NULL;
    for (int i = 0; i < MAX_TEST; ++i) {
        value = CachedParameterGet(cacheHandle);
    }
    CachedParameterDestroy(cacheHandle);
    printf("CachedParameterGet time %lld us value %s \n", DiffLocalTime(&startTime), value);
    return;
}

static int32_t BShellParamCmdPerformance(BShellHandle shell, int32_t argc, char *argv[])
{
    const char *name = "test.performance.read";
    TestPerformance(name);
    CachedHandle cacheHandle = CachedParameterCreate(name, "true");
    int i = 0;
    while (++i < MAX_TEST) {
        const char *value = CachedParameterGet(cacheHandle);
        printf("CachedParameterGet %s value %s \n", name, value);
        usleep(100);
    }
    CachedParameterDestroy(cacheHandle);
    return 0;
}

int32_t BShellCmdRegister(BShellHandle shell, int execMode)
{
    if (execMode == 0) {
        const CmdInfo infos[] = {
            {"init", BShellParamCmdGroupTest, "init group test", "init group test [stage]", "init group test"},
            {"display", BShellParamCmdMemGet, "display memory pid", "display memory [pid]", "display memory"},
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
            {"display", BShellParamCmdMemGet, "display memory pid", "display memory [pid]", "display memory"},
            {"test", BShellParamCmdPerformance, "test performance", "test performance", "test performance"},
        };
        for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
            BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
        }
    }
    return 0;
}
