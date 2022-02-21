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

#include "bootchart.h"

#include <dirent.h>
#include <stdint.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "init_plugin.h"
#include "init_param.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"

#define NANO_PRE_JIFFY 10000000

static BootchartCtrl *g_bootchartCtrl = NULL;
static PluginInterface *g_pluginInterface = NULL;

static int64_t GetJiffies(void)
{
    struct timespec time1 = {0};
    clock_gettime(CLOCK_MONOTONIC, &time1);
    int64_t jiffies1 = (int64_t)time1.tv_nsec / NANO_PRE_JIFFY;
    int64_t jiffies2 = (int64_t)time1.tv_sec * (1000000000 / NANO_PRE_JIFFY); // 1000000000 to nsec
    return jiffies1 + jiffies2;
}

char *ReadFileToBuffer(const char *fileName, char *buffer, uint32_t bufferSize)
{
    PLUGIN_CHECK(buffer != NULL && fileName != NULL, return NULL, "Invalid param");
    int fd = -1;
    ssize_t readLen = 0;
    do {
        buffer[0] = '\0';
        errno = 0;
        fd = open(fileName, O_RDONLY);
        if (fd > 0) {
            readLen = read(fd, buffer, bufferSize - 1);
        }
        PLUGIN_CHECK(readLen >= 0, break, "Failed to read data for %s %d readLen %d", fileName, errno, readLen);
        buffer[readLen] = '\0';
    } while (0);
    if (fd != -1) {
        close(fd);
    }
    return (readLen > 0) ? buffer : NULL;
}

static void BootchartLogHeader(void)
{
    char date[32]; // 32 data size
    time_t tm = time(NULL);
    PLUGIN_CHECK(tm >= 0, return, "Failed to get time");
    struct tm *now = localtime(&tm);
    PLUGIN_CHECK(now != NULL, return, "Failed to get local time");
    size_t size = strftime(date, sizeof(date), "%F %T", now);
    PLUGIN_CHECK(size >= 0, return, "Failed to strftime");
    struct utsname uts;
    if (uname(&uts) == -1) {
        return;
    }

    char release[PARAM_VALUE_LEN_MAX] = {};
    uint32_t len = sizeof(release);
    if (g_pluginInterface->systemReadParam != NULL) {
        (void)g_pluginInterface->systemReadParam("hw_sc.build.os.releasetype", release, &len);
    }
    char *cmdLine = ReadFileToBuffer("/proc/cmdline", g_bootchartCtrl->buffer, g_bootchartCtrl->bufferSize);
    PLUGIN_CHECK(cmdLine != NULL, return, "Failed to open file /data/bootchart/header");

    FILE *file = fopen("/data/bootchart/header", "we");
    PLUGIN_CHECK(file != NULL, return, "Failed to open file /data/bootchart/header");

    (void)fprintf(file, "version = openharmony init\n");
    (void)fprintf(file, "title = Boot chart for openharmony (%s)\n", date);
    (void)fprintf(file, "system.uname = %s %s %s %s\n", uts.sysname, uts.release, uts.version, uts.machine);
    if (strlen(release) > 0) {
        (void)fprintf(file, "system.release = %s\n", release);
    }
    (void)fprintf(file, "system.cpu = %s\n", uts.machine);
    (void)fprintf(file, "system.kernel.options = %s\n", cmdLine);
    (void)fclose(file);
}

static void BootchartLogFile(FILE *log, const char *procfile)
{
    (void)fprintf(log, "%lld\n", GetJiffies());
    char *data = ReadFileToBuffer(procfile, g_bootchartCtrl->buffer, g_bootchartCtrl->bufferSize);
    if (data != NULL) {
        (void)fprintf(log, "%s\n", data);
    }
}

static void BootchartLogProcessStat(FILE *log, pid_t pid)
{
    static char path[255] = { }; // 255 path length
    static char nameBuffer[255] = { }; // 255 path length
    // /proc/<pid>/stat only has truncated task names, so get the full name from /proc/<pid>/cmdline.
    int ret = sprintf_s(path, sizeof(path) - 1, "/proc/%d/cmdline", pid);
    PLUGIN_CHECK(ret > 0, return, "Failed to format path %d", pid);
    path[ret] = '\0';

    char *name = ReadFileToBuffer(path, nameBuffer, sizeof(nameBuffer));
    // Read process stat line.
    ret = sprintf_s(path, sizeof(path) - 1, "/proc/%d/stat", pid);
    PLUGIN_CHECK(ret > 0, return, "Failed to format path %d", pid);
    path[ret] = '\0';

    char *stat = ReadFileToBuffer(path, g_bootchartCtrl->buffer, g_bootchartCtrl->bufferSize);
    if (stat == NULL) {
        return;
    }
    if (name != NULL && strlen(name) > 0) {
        // Substitute the process name with its real name.
        char *end = NULL;
        char *start = strstr(stat, "(");
        if (start != NULL) {
            end = strstr(start, ")");
        }
        if (end != NULL) {
            stat[start - stat + 1] = '\0';
            (void)fputs(stat, log);
            (void)fputs(name, log);
            (void)fputs(end, log);
        } else {
            (void)fputs(stat, log);
        }
    } else {
        (void)fputs(stat, log);
    }
}

static void bootchartLogProcess(FILE *log)
{
    (void)fprintf(log, "%lld\n", GetJiffies());
    DIR *pDir = opendir("/proc");
    PLUGIN_CHECK(pDir != NULL, return, "Read dir /proc failed.%d", errno);
    struct dirent *entry;
    while ((entry = readdir(pDir)) != NULL) {
        pid_t pid = (pid_t)atoi(entry->d_name); // // Only process processor
        if (pid == 0) {
            continue;
        }
        BootchartLogProcessStat(log, pid);
    }
    closedir(pDir);
    (void)fputc('\n', log);
}

static void *BootchartThreadMain(void *data)
{
    PLUGIN_LOGI("bootcharting start");
    FILE *statFile = fopen("/data/bootchart/proc_stat.log", "w");
    FILE *procFile = fopen("/data/bootchart/proc_ps.log", "w");
    FILE *diskFile = fopen("/data/bootchart/proc_diskstats.log", "w");
    do {
        if (statFile == NULL || procFile == NULL || diskFile == NULL) {
            PLUGIN_LOGE("Failed to open file");
            break;
        }
        BootchartLogHeader();
        while (1) {
            pthread_mutex_lock(&(g_bootchartCtrl->mutex));
            struct timespec abstime = {};
            struct timeval now = {};
            const long timeout = 200; // wait time 200ms
            gettimeofday(&now, NULL);
            long nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000; // 1000 unit 1000000 unit nsec
            abstime.tv_sec = now.tv_sec + nsec / 1000000000 + timeout / 1000; // 1000 unit 1000000000 unit nsec
            abstime.tv_nsec = nsec % 1000000000; // 1000000000 unit nsec
            pthread_cond_timedwait(&(g_bootchartCtrl->cond), &(g_bootchartCtrl->mutex), &abstime);
            if (g_bootchartCtrl->stop) {
                pthread_mutex_unlock(&(g_bootchartCtrl->mutex));
                break;
            }
            pthread_mutex_unlock(&(g_bootchartCtrl->mutex));
            PLUGIN_LOGV("bootcharting running");
            BootchartLogFile(statFile, "/proc/stat");
            BootchartLogFile(diskFile, "/proc/diskstats");
            bootchartLogProcess(procFile);
        }
    } while (0);

    if (statFile != NULL) {
        (void)fflush(statFile);
        (void)fclose(statFile);
    }
    if (procFile != NULL) {
        (void)fflush(procFile);
        (void)fclose(procFile);
    }
    if (diskFile != NULL) {
        (void)fflush(diskFile);
        (void)fclose(diskFile);
    }
    PLUGIN_LOGI("bootcharting stop");
    return NULL;
}

static void BootchartDestory(void)
{
    pthread_mutex_destroy(&(g_bootchartCtrl->mutex));
    pthread_cond_destroy(&(g_bootchartCtrl->cond));
    free(g_bootchartCtrl);
    g_bootchartCtrl = NULL;
}

static int DoBootchartStart(void)
{
    if (g_pluginInterface == NULL) {
        PLUGIN_LOGI("Invalid bootchart plugin");
        return -1;
    }
    // We don't care about the content, but we do care that /data/bootchart/enabled actually exists.
    char enable[4] = {}; // 4 enable size
    uint32_t size = sizeof(enable);
    if (g_pluginInterface->systemReadParam != NULL) {
        g_pluginInterface->systemReadParam("init.bootchart.enabled", enable, &size);
    }
    if (strcmp(enable, "1") != 0) {
        PLUGIN_LOGI("Not bootcharting");
        return 0;
    }
    mkdir("/data/bootchart", S_IRWXU | S_IRWXG | S_IRWXO);
    if (g_bootchartCtrl != NULL) {
        PLUGIN_LOGI("bootcharting has been start");
        return 0;
    }
    int ret = 0;
    g_bootchartCtrl = malloc(sizeof(BootchartCtrl));
    PLUGIN_CHECK(g_bootchartCtrl != NULL, return -1, "Failed to alloc mem for bootchart");
    g_bootchartCtrl->bufferSize = DEFAULT_BUFFER;

    ret = pthread_mutex_init(&(g_bootchartCtrl->mutex), NULL);
    PLUGIN_CHECK(ret == 0, BootchartDestory();
        return -1, "Failed to init mutex");
    ret = pthread_cond_init(&(g_bootchartCtrl->cond), NULL);
    PLUGIN_CHECK(ret == 0, BootchartDestory();
        return -1, "Failed to init cond");

    g_bootchartCtrl->stop = 0;
    ret = pthread_create(&(g_bootchartCtrl->threadId), NULL, BootchartThreadMain, (void *)g_bootchartCtrl);
    PLUGIN_CHECK(ret == 0, BootchartDestory();
        return -1, "Failed to init cond");

    pthread_mutex_lock(&(g_bootchartCtrl->mutex));
    pthread_cond_signal(&(g_bootchartCtrl->cond));
    pthread_mutex_unlock(&(g_bootchartCtrl->mutex));
    g_bootchartCtrl->start = 1;
    return 0;
}

static int DoBootchartStop(void)
{
    if (g_bootchartCtrl == NULL || !g_bootchartCtrl->start) {
        PLUGIN_LOGI("bootcharting not start");
        return 0;
    }
    pthread_mutex_lock(&(g_bootchartCtrl->mutex));
    g_bootchartCtrl->stop = 1;
    pthread_cond_signal(&(g_bootchartCtrl->cond));
    pthread_mutex_unlock(&(g_bootchartCtrl->mutex));
    pthread_join(g_bootchartCtrl->threadId, NULL);
    BootchartDestory();
    PLUGIN_LOGI("bootcharting stoped");
    return 0;
}

static int DoBootchartCmd(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("DoBootchartCmd argc %d %s", argc, name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    if (strcmp(argv[0], "start") == 0) {
        return DoBootchartStart();
    } else if (strcmp(argv[0], "stop") == 0) {
        return DoBootchartStop();
    }
    return 0;
}

static PluginCmd g_bootchartCmds[] = {
    {"bootchart", DoBootchartCmd, 0},
};

static int BootchartInit(void)
{
    PLUGIN_LOGI("BootchartInit ");
    g_pluginInterface = GetPluginInterface();
    PLUGIN_CHECK(g_pluginInterface != NULL && g_pluginInterface->addCmdExecutor != NULL, return -1,
                 "Invalid install parameter");

    for (int i = 0; i < (int)(sizeof(g_bootchartCmds) / sizeof(g_bootchartCmds[0])); i++) {
        g_bootchartCmds[i].index = g_pluginInterface->addCmdExecutor(
            g_bootchartCmds[i].name, g_bootchartCmds[i].cmdExecutor);
        PLUGIN_LOGI("BootchartInit %d", g_bootchartCmds[i].index);
    }
    return 0;
}

static void BootchartExit(void)
{
    PLUGIN_LOGI("BootchartExit %d", g_bootchartCmds[0]);
    PLUGIN_CHECK(g_pluginInterface != NULL && g_pluginInterface->removeCmdExecutor != NULL, return,
                 "Invalid install parameter");
    for (int i = 0; i < (int)(sizeof(g_bootchartCmds) / sizeof(g_bootchartCmds[0])); i++) {
        g_pluginInterface->removeCmdExecutor(g_bootchartCmds[i].name, g_bootchartCmds[i].index);
    }
}

PLUGIN_CONSTRUCTOR(void)
{
    g_pluginInterface = GetPluginInterface();
    if (g_pluginInterface != NULL && g_pluginInterface->pluginRegister != NULL) {
        g_pluginInterface->pluginRegister("bootchart", NULL, BootchartInit, BootchartExit);
    }
    PLUGIN_LOGI("bootchart pluginInterface %p %p %p",
        g_pluginInterface, g_pluginInterface->pluginRegister, GetPluginInterface,
        BootchartInit, BootchartExit);
}