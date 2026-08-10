/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "sysmonitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#include "init_log.h"
#include "init_module_engine.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"

#define CTXT_LINE_PREFIX            "ctxt "
#define PROCESSES_LINE_PREFIX       "processes "
#define PROCS_RUNNING_PREFIX        "procs_running "
#define PROCS_BLOCKED_PREFIX        "procs_blocked "

static MonitorContext g_monitorCtx;
static pthread_mutex_t g_monitorMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;

static int ReadCpuStats(CpuStats *stats);
static int ReadMemoryStats(MemoryStats *stats);
static int ReadProcessInfo(ProcessInfo *info, int pid);
static void FreeProcessList(void);
static void FreeAlarmList(void);
static void FreePerfRecordList(void);
static int UpdateAllStats(void);
static void SetDefaultConfig(MonitorConfig *config);
static void ParseMemInfoLine(const char *line, MemoryStats *stats);
static int ParseProcessStat(const char *line, ProcessInfo *info);
static int ParseDiskstatsLine(const char *line, DiskStats *stats);
static int ParseNetDevLine(char *line, NetworkStats *stats);
static void ParseStatLine(const char *line, CpuStats *stats);
static void CalculateMemUsagePercent(MemoryStats *stats);

static void SetDefaultConfig(MonitorConfig *config)
{
    config->sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_MS;
    config->historySize = DEFAULT_HISTORY_SIZE;
    config->enableCpuMonitor = true;
    config->enableMemMonitor = true;
    config->enableProcMonitor = true;
    config->enableDiskMonitor = true;
    config->enableNetMonitor = true;
    config->cpuThreshold = CPU_THRESHOLD_DEFAULT;
    config->memThreshold = MEM_THRESHOLD_DEFAULT;
    config->diskThreshold = DISK_THRESHOLD_DEFAULT;
}

int InitMonitor(const MonitorConfig *config)
{
    INIT_CHECK_RETURN_VALUE(!g_initialized, MONITOR_OK);

    pthread_mutex_lock(&g_monitorMutex);
    int ret = memset_s(&g_monitorCtx, sizeof(MonitorContext), 0, sizeof(MonitorContext));
    if (ret != EOK) {
        pthread_mutex_unlock(&g_monitorMutex);
        PLUGIN_LOGE("Failed to memset monitor context");
        return MONITOR_ERROR;
    }
    g_monitorCtx.state = MONITOR_STATE_IDLE;

    if (config != NULL) {
        ret = memcpy_s(&g_monitorCtx.config, sizeof(MonitorConfig),
            config, sizeof(MonitorConfig));
        if (ret != EOK) {
            pthread_mutex_unlock(&g_monitorMutex);
            PLUGIN_LOGE("Failed to copy monitor config");
            return MONITOR_ERROR;
        }
    } else {
        SetDefaultConfig(&g_monitorCtx.config);
    }

    OH_ListInit(&g_monitorCtx.processList);
    OH_ListInit(&g_monitorCtx.alarmList);
    OH_ListInit(&g_monitorCtx.perfRecordList);

    g_initialized = true;
    g_monitorCtx.state = MONITOR_STATE_IDLE;
    g_monitorCtx.alarmCount = 0;
    g_monitorCtx.recordCount = 0;

    pthread_mutex_unlock(&g_monitorMutex);
    PLUGIN_LOGI("Monitor module initialized successfully");
    return MONITOR_OK;
}

void DestroyMonitor(void)
{
    INIT_CHECK_ONLY_RETURN(g_initialized);

    pthread_mutex_lock(&g_monitorMutex);
    g_monitorCtx.state = MONITOR_STATE_IDLE;

    FreeProcessList();
    FreeAlarmList();
    FreePerfRecordList();

    g_initialized = false;
    pthread_mutex_unlock(&g_monitorMutex);
    PLUGIN_LOGI("Monitor module destroyed");
}

int StartMonitor(void)
{
    INIT_CHECK_RETURN_VALUE(g_initialized, MONITOR_ERROR);

    pthread_mutex_lock(&g_monitorMutex);
    if (g_monitorCtx.state == MONITOR_STATE_RUNNING) {
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_OK;
    }

    int ret = UpdateAllStats();
    if (ret != MONITOR_OK) {
        PLUGIN_LOGE("Failed to update initial stats");
        g_monitorCtx.state = MONITOR_STATE_ERROR;
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_RUNNING;
    clock_gettime(CLOCK_MONOTONIC, (struct timespec *)&g_monitorCtx.lastUpdateTime);
    pthread_mutex_unlock(&g_monitorMutex);

    PLUGIN_LOGI("Monitor started");
    return MONITOR_OK;
}

int StopMonitor(void)
{
    INIT_CHECK_RETURN_VALUE(g_initialized, MONITOR_ERROR);

    pthread_mutex_lock(&g_monitorMutex);
    g_monitorCtx.state = MONITOR_STATE_IDLE;
    pthread_mutex_unlock(&g_monitorMutex);

    PLUGIN_LOGI("Monitor stopped");
    return MONITOR_OK;
}

int PauseMonitor(void)
{
    INIT_CHECK_RETURN_VALUE(g_initialized, MONITOR_ERROR);

    pthread_mutex_lock(&g_monitorMutex);
    if (g_monitorCtx.state != MONITOR_STATE_RUNNING) {
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_PAUSED;
    pthread_mutex_unlock(&g_monitorMutex);

    PLUGIN_LOGI("Monitor paused");
    return MONITOR_OK;
}

int ResumeMonitor(void)
{
    INIT_CHECK_RETURN_VALUE(g_initialized, MONITOR_ERROR);

    pthread_mutex_lock(&g_monitorMutex);
    if (g_monitorCtx.state != MONITOR_STATE_PAUSED) {
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_RUNNING;
    pthread_mutex_unlock(&g_monitorMutex);

    PLUGIN_LOGI("Monitor resumed");
    return MONITOR_OK;
}

MonitorState GetMonitorState(void)
{
    return g_monitorCtx.state;
}

int UpdateCpuStats(CpuStats *stats)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL, MONITOR_INVALID_PARAM);

    int ret = ReadCpuStats(&g_monitorCtx.cpuStats);
    if (ret == MONITOR_OK) {
        ret = memcpy_s(stats, sizeof(CpuStats), &g_monitorCtx.cpuStats, sizeof(CpuStats));
        INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);
    }
    return ret;
}

int UpdateMemoryStats(MemoryStats *stats)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL, MONITOR_INVALID_PARAM);

    int ret = ReadMemoryStats(&g_monitorCtx.memStats);
    if (ret == MONITOR_OK) {
        ret = memcpy_s(stats, sizeof(MemoryStats), &g_monitorCtx.memStats, sizeof(MemoryStats));
        INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);
    }
    return ret;
}

int UpdateProcessStats(void)
{
    FreeProcessList();

    DIR *procDir = opendir(PROC_DIR_PATH);
    INIT_CHECK_RETURN_VALUE(procDir != NULL, MONITOR_ERROR);

    struct dirent *entry = NULL;
    int processCount = 0;
    while ((entry = readdir(procDir)) != NULL && processCount < MAX_PROCESS_COUNT) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        char *endptr = NULL;
        long pid = strtol(entry->d_name, &endptr, DECIMAL_BASE);
        if (*endptr != '\0' || pid <= 0) {
            continue;
        }

        ProcessInfo *info = (ProcessInfo *)calloc(1, sizeof(ProcessInfo));
        PLUGIN_ONLY_LOG(info != NULL, "Failed to allocate memory for process info");
        if (info == NULL) {
            continue;
        }

        if (ReadProcessInfo(info, (int)pid) == MONITOR_OK) {
            OH_ListAddTail(&g_monitorCtx.processList, &info->node);
            processCount++;
        } else {
            free(info);
        }
    }

    closedir(procDir);
    return MONITOR_OK;
}

static int ParseDiskstatsLine(const char *line, DiskStats *stats)
{
    uint32_t major = 0;
    uint32_t minor = 0;
    char deviceName[DEVICE_NAME_MAX_LEN] = {0};

    uint64_t readsCompleted = 0;
    uint64_t readsMerged = 0;
    uint64_t sectorsRead = 0;
    uint64_t readTimeMs = 0;
    uint64_t writesCompleted = 0;

    int fields = sscanf_s(line, "%u %u %63s %llu %llu %llu %llu %llu %llu",
        &major, &minor, deviceName, sizeof(deviceName),
        &readsCompleted, &readsMerged, &sectorsRead, &readTimeMs,
        &writesCompleted);
    INIT_CHECK_RETURN_VALUE(fields >= 5, MONITOR_ERROR);

    int ret = strncpy_s(stats->deviceName, sizeof(stats->deviceName),
        deviceName, strlen(deviceName));
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    stats->readsCompleted = readsCompleted;
    stats->writesCompleted = writesCompleted;
    return MONITOR_OK;
}

int UpdateDiskStats(DiskStats *stats, uint32_t maxCount)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL && maxCount > 0, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(PROC_DISKSTATS_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    uint32_t count = 0;
    while (fgets(line, sizeof(line), fp) != NULL && count < maxCount) {
        if (ParseDiskstatsLine(line, &stats[count]) == MONITOR_OK) {
            count++;
        }
    }
    fclose(fp);

    uint32_t copyCount = (count < MAX_DISK_STATS_COUNT) ? count : MAX_DISK_STATS_COUNT;
    int ret = memcpy_s(g_monitorCtx.diskStats, sizeof(g_monitorCtx.diskStats),
        stats, sizeof(DiskStats) * copyCount);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    return MONITOR_OK;
}

static int ParseNetDevLine(char *line, NetworkStats *stats)
{
    char *colon = strchr(line, ':');
    INIT_CHECK_RETURN_VALUE(colon != NULL, MONITOR_ERROR);

    char interface[NET_IFACE_MAX_LEN] = {0};
    char *start = line;
    while (*start == ' ') {
        start++;
    }

    size_t ifaceLen = (size_t)(colon - start);
    if (ifaceLen >= sizeof(interface)) {
        ifaceLen = sizeof(interface) - 1;
    }
    int ret = strncpy_s(interface, sizeof(interface), start, ifaceLen);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    uint64_t rxBytes = 0;
    uint64_t txBytes = 0;

    int fields = sscanf_s(colon + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu",
        &rxBytes, &txBytes);
    INIT_CHECK_RETURN_VALUE(fields >= 1, MONITOR_ERROR);

    ret = strncpy_s(stats->interface, sizeof(stats->interface),
        interface, strlen(interface));
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    stats->rxBytes = rxBytes;
    stats->txBytes = txBytes;
    return MONITOR_OK;
}

int UpdateNetworkStats(NetworkStats *stats, uint32_t maxCount)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL && maxCount > 0, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(PROC_NET_DEV_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    if (fgets(line, sizeof(line), fp) == NULL ||
        fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return MONITOR_ERROR;
    }

    uint32_t count = 0;
    while (fgets(line, sizeof(line), fp) != NULL && count < maxCount) {
        if (ParseNetDevLine(line, &stats[count]) == MONITOR_OK) {
            count++;
        }
    }
    fclose(fp);

    uint32_t copyCount = (count < MAX_NET_STATS_COUNT) ? count : MAX_NET_STATS_COUNT;
    int ret = memcpy_s(g_monitorCtx.netStats, sizeof(g_monitorCtx.netStats),
        stats, sizeof(NetworkStats) * copyCount);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    return MONITOR_OK;
}

const CpuStats *GetCpuStats(void)
{
    return &g_monitorCtx.cpuStats;
}

const MemoryStats *GetMemoryStats(void)
{
    return &g_monitorCtx.memStats;
}

const ListNode *GetProcessList(void)
{
    return &g_monitorCtx.processList;
}

ProcessInfo *GetProcessInfo(int pid)
{
    ListNode *node = g_monitorCtx.processList.next;
    while (node != &g_monitorCtx.processList) {
        ProcessInfo *info = (ProcessInfo *)node;
        if (info->pid == pid) {
            return info;
        }
        node = node->next;
    }
    return NULL;
}

int AddMonitorAlarm(MonitorType type, AlarmLevel level,
    const char *message, uint32_t threshold, uint32_t actualValue)
{
    INIT_CHECK_RETURN_VALUE(message != NULL, MONITOR_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(g_monitorCtx.alarmCount < MAX_ALARM_COUNT, MONITOR_INVALID_PARAM);

    MonitorAlarm *alarm = (MonitorAlarm *)calloc(1, sizeof(MonitorAlarm));
    INIT_CHECK_RETURN_VALUE(alarm != NULL, MONITOR_ERROR);

    alarm->type = type;
    alarm->level = level;
    alarm->threshold = threshold;
    alarm->actualValue = actualValue;
    alarm->handled = false;
    clock_gettime(CLOCK_REALTIME, &alarm->timestamp);

    int ret = strncpy_s(alarm->message, sizeof(alarm->message),
        message, strlen(message));
    if (ret != EOK) {
        free(alarm);
        return MONITOR_ERROR;
    }

    OH_ListAddTail(&g_monitorCtx.alarmList, &alarm->node);
    g_monitorCtx.alarmCount++;

    PLUGIN_LOGI("Added monitor alarm: type=%d level=%d message=%s", type, level, message);
    return MONITOR_OK;
}

const ListNode *GetAlarmList(void)
{
    return &g_monitorCtx.alarmList;
}

void ClearHandledAlarms(void)
{
    ListNode *node = g_monitorCtx.alarmList.next;
    while (node != &g_monitorCtx.alarmList) {
        MonitorAlarm *alarm = (MonitorAlarm *)node;
        ListNode *next = node->next;

        if (alarm->handled) {
            OH_ListRemove(node);
            free(alarm);
            g_monitorCtx.alarmCount--;
        }
        node = next;
    }
}

PerfRecord *BeginPerfRecord(const char *name, uint32_t type)
{
    INIT_CHECK_RETURN_VALUE(name != NULL, NULL);
    INIT_CHECK_RETURN_VALUE(g_monitorCtx.recordCount < MAX_PERF_RECORD_COUNT, NULL);

    PerfRecord *record = (PerfRecord *)calloc(1, sizeof(PerfRecord));
    INIT_CHECK_RETURN_VALUE(record != NULL, NULL);

    int ret = strncpy_s(record->name, sizeof(record->name), name, strlen(name));
    if (ret != EOK) {
        free(record);
        return NULL;
    }

    record->type = type;
    record->flags = 0;
    record->userData = NULL;
    clock_gettime(CLOCK_MONOTONIC, &record->startTime);

    OH_ListAddTail(&g_monitorCtx.perfRecordList, &record->node);
    g_monitorCtx.recordCount++;

    return record;
}

int EndPerfRecord(PerfRecord *record)
{
    INIT_CHECK_RETURN_VALUE(record != NULL, MONITOR_INVALID_PARAM);

    clock_gettime(CLOCK_MONOTONIC, &record->endTime);
    record->durationNs = (uint64_t)(record->endTime.tv_sec - record->startTime.tv_sec) * NSEC_PER_SEC +
                         (uint64_t)(record->endTime.tv_nsec - record->startTime.tv_nsec);
    return MONITOR_OK;
}

const ListNode *GetPerfRecords(void)
{
    return &g_monitorCtx.perfRecordList;
}

uint32_t CalculateCpuUsage(const CpuStats *prev, const CpuStats *curr)
{
    INIT_CHECK_RETURN_VALUE(prev != NULL && curr != NULL, 0);

    uint64_t prevIdle = prev->idleTime + prev->ioWaitTime;
    uint64_t currIdle = curr->idleTime + curr->ioWaitTime;

    uint64_t prevTotal = prev->userModeTime + prev->niceTime + prev->systemTime +
                         prev->idleTime + prev->ioWaitTime + prev->irqTime +
                         prev->softIrqTime + prev->stealTime + prev->guestTime +
                         prev->guestNiceTime;

    uint64_t currTotal = curr->userModeTime + curr->niceTime + curr->systemTime +
                         curr->idleTime + curr->ioWaitTime + curr->irqTime +
                         curr->softIrqTime + curr->stealTime + curr->guestTime +
                         curr->guestNiceTime;

    uint64_t totalDiff = currTotal - prevTotal;
    uint64_t idleDiff = currIdle - prevIdle;
    if (totalDiff == 0) {
        return 0;
    }

    return (uint32_t)(((totalDiff - idleDiff) * PERCENT_SCALE) / totalDiff);
}

uint32_t CalculateMemUsage(const MemoryStats *stats)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL && stats->totalMem > 0, 0);

    uint64_t usedMem = stats->totalMem - stats->availableMem;
    return (uint32_t)((usedMem * PERCENT_SCALE) / stats->totalMem);
}

ProcessInfo *FindTopCpuProcess(void)
{
    ProcessInfo *topProcess = NULL;
    uint64_t maxCpuUsage = 0;

    ListNode *node = g_monitorCtx.processList.next;
    while (node != &g_monitorCtx.processList) {
        ProcessInfo *info = (ProcessInfo *)node;
        if (info->cpuUsage > maxCpuUsage) {
            maxCpuUsage = info->cpuUsage;
            topProcess = info;
        }
        node = node->next;
    }
    return topProcess;
}

ProcessInfo *FindTopMemProcess(void)
{
    ProcessInfo *topProcess = NULL;
    uint64_t maxMemUsage = 0;

    ListNode *node = g_monitorCtx.processList.next;
    while (node != &g_monitorCtx.processList) {
        ProcessInfo *info = (ProcessInfo *)node;
        if (info->memUsage > maxMemUsage) {
            maxMemUsage = info->memUsage;
            topProcess = info;
        }
        node = node->next;
    }
    return topProcess;
}

int CheckThresholds(void)
{
    int alarmsGenerated = 0;
    uint32_t cpuThresholdScaled = g_monitorCtx.config.cpuThreshold * PERCENT_MULTIPLIER;
    uint32_t memThresholdScaled = g_monitorCtx.config.memThreshold * PERCENT_MULTIPLIER;

    if (g_monitorCtx.config.enableCpuMonitor &&
        g_monitorCtx.cpuStats.totalUsage > cpuThresholdScaled) {
        AddMonitorAlarm(MONITOR_TYPE_CPU, ALARM_LEVEL_WARNING,
            "CPU usage exceeds threshold",
            cpuThresholdScaled, g_monitorCtx.cpuStats.totalUsage);
        alarmsGenerated++;
    }

    if (g_monitorCtx.config.enableMemMonitor &&
        g_monitorCtx.memStats.usagePercent > memThresholdScaled) {
        AddMonitorAlarm(MONITOR_TYPE_MEMORY, ALARM_LEVEL_WARNING,
            "Memory usage exceeds threshold",
            memThresholdScaled, g_monitorCtx.memStats.usagePercent);
        alarmsGenerated++;
    }

    return alarmsGenerated;
}

static void ParseStatLine(const char *line, CpuStats *stats)
{
    int parsed = 0;
    if (strncmp(line, CTXT_LINE_PREFIX, strlen(CTXT_LINE_PREFIX)) == 0) {
        parsed = sscanf_s(line, "ctxt %u", &stats->contextSwitches);
        PLUGIN_ONLY_LOG(parsed == 1, "Failed to parse ctxt line");
    } else if (strncmp(line, PROCESSES_LINE_PREFIX, strlen(PROCESSES_LINE_PREFIX)) == 0) {
        parsed = sscanf_s(line, "processes %u", &stats->processesCreated);
        PLUGIN_ONLY_LOG(parsed == 1, "Failed to parse processes line");
    } else if (strncmp(line, PROCS_RUNNING_PREFIX, strlen(PROCS_RUNNING_PREFIX)) == 0) {
        parsed = sscanf_s(line, "procs_running %u", &stats->processesRunning);
        PLUGIN_ONLY_LOG(parsed == 1, "Failed to parse procs_running line");
    } else if (strncmp(line, PROCS_BLOCKED_PREFIX, strlen(PROCS_BLOCKED_PREFIX)) == 0) {
        parsed = sscanf_s(line, "procs_blocked %u", &stats->processesBlocked);
        PLUGIN_ONLY_LOG(parsed == 1, "Failed to parse procs_blocked line");
    }
}

static int ReadCpuStats(CpuStats *stats)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(PROC_STAT_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    PLUGIN_CHECK(fgets(line, sizeof(line), fp) != NULL, fclose(fp);
        return MONITOR_ERROR, "Failed to read /proc/stat");

    char cpuLabel[CPU_LABEL_LEN] = {0};
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;
    uint64_t guest = 0;
    uint64_t guestNice = 0;

    int parsed = sscanf_s(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        cpuLabel, sizeof(cpuLabel),
        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guestNice);
    fclose(fp);
    INIT_CHECK_RETURN_VALUE(parsed >= 5, MONITOR_ERROR);

    stats->userModeTime = user;
    stats->niceTime = nice;
    stats->systemTime = system;
    stats->idleTime = idle;
    stats->ioWaitTime = (parsed >= 6) ? iowait : 0;
    stats->irqTime = (parsed >= 7) ? irq : 0;
    stats->softIrqTime = (parsed >= 8) ? softirq : 0;
    stats->stealTime = (parsed >= 9) ? steal : 0;
    stats->guestTime = (parsed >= 10) ? guest : 0;
    stats->guestNiceTime = (parsed >= 11) ? guestNice : 0;
    stats->cpuCoreNum = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);

    fp = fopen(PROC_STAT_PATH, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            ParseStatLine(line, stats);
        }
        fclose(fp);
    }
    return MONITOR_OK;
}

typedef struct {
    const char *key;
    uint64_t *target;
} MemInfoField;

static void ParseMemInfoLine(const char *line, MemoryStats *stats)
{
    char key[MEM_KEY_LEN] = {0};
    uint64_t value = 0;
    char unit[MEM_UNIT_LEN] = {0};

    if (sscanf_s(line, "%63s %llu %15s", key, sizeof(key), &value, unit, sizeof(unit)) < 2) {
        return;
    }

    if (strcmp(unit, "kB") == 0 || strcmp(unit, "KB") == 0) {
        value *= BYTES_PER_KB;
    }

    MemInfoField fields[] = {
        {"MemTotal:", &stats->totalMem},
        {"MemFree:", &stats->freeMem},
        {"MemAvailable:", &stats->availableMem},
        {"Buffers:", &stats->buffers},
        {"Cached:", &stats->cached},
        {"SwapTotal:", &stats->swapTotal},
        {"SwapFree:", &stats->swapFree},
    };

    size_t fieldCount = sizeof(fields) / sizeof(fields[0]);
    for (size_t i = 0; i < fieldCount; i++) {
        if (strcmp(key, fields[i].key) == 0) {
            *fields[i].target = value;
            return;
        }
    }
}

static void CalculateMemUsagePercent(MemoryStats *stats)
{
    if (stats->totalMem == 0) {
        return;
    }

    uint64_t usedMem = stats->totalMem - stats->availableMem;
    stats->usagePercent = (uint32_t)((usedMem * PERCENT_SCALE) / stats->totalMem);

    if (stats->swapTotal > 0) {
        uint64_t usedSwap = stats->swapTotal - stats->swapFree;
        stats->swapUsagePercent = (uint32_t)((usedSwap * PERCENT_SCALE) / stats->swapTotal);
    }
}

static int ReadMemoryStats(MemoryStats *stats)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(PROC_MEMINFO_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    int ret = memset_s(stats, sizeof(MemoryStats), 0, sizeof(MemoryStats));
    if (ret != EOK) {
        fclose(fp);
        return MONITOR_ERROR;
    }

    char line[MAX_LINE_LENGTH] = {0};
    while (fgets(line, sizeof(line), fp) != NULL) {
        ParseMemInfoLine(line, stats);
    }
    fclose(fp);

    CalculateMemUsagePercent(stats);
    return MONITOR_OK;
}

static int ParseProcessStat(const char *line, ProcessInfo *info)
{
    char *start = strchr(line, '(');
    char *end = strrchr(line, ')');
    INIT_CHECK_RETURN_VALUE(start != NULL && end != NULL, MONITOR_ERROR);

    int nameLen = (int)(end - start - 1);
    if (nameLen >= (int)sizeof(info->name)) {
        nameLen = (int)sizeof(info->name) - 1;
    }
    int ret = strncpy_s(info->name, sizeof(info->name), start + 1, (size_t)nameLen);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    return MONITOR_OK;
}

static int ReadProcessInfo(ProcessInfo *info, int pid)
{
    INIT_CHECK_RETURN_VALUE(info != NULL && pid > 0, MONITOR_INVALID_PARAM);

    char path[256] = {0};
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/stat", pid);
    INIT_CHECK_RETURN_VALUE(ret >= 0, MONITOR_ERROR);

    FILE *fp = fopen(path, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    PLUGIN_CHECK(fgets(line, sizeof(line), fp) != NULL, fclose(fp);
        return MONITOR_ERROR, "Failed to read %s", path);
    fclose(fp);

    ret = ParseProcessStat(line, info);
    INIT_CHECK_RETURN_VALUE(ret == MONITOR_OK, MONITOR_ERROR);

    info->pid = pid;
    return MONITOR_OK;
}

static void FreeProcessList(void)
{
    ListNode *node = g_monitorCtx.processList.next;
    while (node != &g_monitorCtx.processList) {
        ProcessInfo *info = (ProcessInfo *)node;
        ListNode *next = node->next;
        OH_ListRemove(node);
        free(info);
        node = next;
    }
    OH_ListInit(&g_monitorCtx.processList);
}

static void FreeAlarmList(void)
{
    ListNode *node = g_monitorCtx.alarmList.next;
    while (node != &g_monitorCtx.alarmList) {
        MonitorAlarm *alarm = (MonitorAlarm *)node;
        ListNode *next = node->next;
        OH_ListRemove(node);
        free(alarm);
        node = next;
    }
    OH_ListInit(&g_monitorCtx.alarmList);
    g_monitorCtx.alarmCount = 0;
}

static void FreePerfRecordList(void)
{
    ListNode *node = g_monitorCtx.perfRecordList.next;
    while (node != &g_monitorCtx.perfRecordList) {
        PerfRecord *record = (PerfRecord *)node;
        ListNode *next = node->next;
        OH_ListRemove(node);
        free(record);
        node = next;
    }
    OH_ListInit(&g_monitorCtx.perfRecordList);
    g_monitorCtx.recordCount = 0;
}

static int UpdateAllStats(void)
{
    int ret = MONITOR_OK;

    if (g_monitorCtx.config.enableCpuMonitor) {
        ret = ReadCpuStats(&g_monitorCtx.cpuStats);
        PLUGIN_ONLY_LOG(ret == MONITOR_OK, "Failed to read CPU stats");
    }

    if (g_monitorCtx.config.enableMemMonitor) {
        ret = ReadMemoryStats(&g_monitorCtx.memStats);
        PLUGIN_ONLY_LOG(ret == MONITOR_OK, "Failed to read memory stats");
    }

    if (g_monitorCtx.config.enableProcMonitor) {
        ret = UpdateProcessStats();
        PLUGIN_ONLY_LOG(ret == MONITOR_OK, "Failed to update process stats");
    }

    if (g_monitorCtx.config.enableDiskMonitor) {
        UpdateDiskStats(g_monitorCtx.diskStats, MAX_DISK_STATS_COUNT);
    }

    if (g_monitorCtx.config.enableNetMonitor) {
        UpdateNetworkStats(g_monitorCtx.netStats, MAX_NET_STATS_COUNT);
    }

    return ret;
}

static int SysmonitorBootHook(const HOOK_INFO *hookInfo, void *cookie)
{
    PLUGIN_LOGI("Sysmonitor boot hook init now ...");
    int ret = InitMonitor(NULL);
    if (ret != MONITOR_OK) {
        PLUGIN_LOGE("Failed to init monitor");
        return ret;
    }
    ret = StartMonitor();
    if (ret != MONITOR_OK) {
        PLUGIN_LOGE("Failed to start monitor");
        return ret;
    }
    return MONITOR_OK;
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("Sysmonitor plug-in init now ...");
    InitAddPostCfgLoadHook(0, SysmonitorBootHook);
}

MODULE_DESTRUCTOR(void)
{
    PLUGIN_LOGI("Sysmonitor plug-in exit now ...");
    StopMonitor();
    DestroyMonitor();
}
