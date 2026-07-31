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
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

#define MONITOR_OK 0
#define MONITOR_ERROR -1
#define MONITOR_INVALID_PARAM -2

#define DEFAULT_SAMPLE_INTERVAL_MS 5000
#define DEFAULT_HISTORY_SIZE 60
#define MAX_PROCESS_COUNT 1024
#define MAX_LINE_LENGTH 512
#define MAX_ALARM_COUNT 100
#define MAX_PERF_RECORD_COUNT 500

// 回调注册信息
typedef struct {
    MonitorCallback callback;
    void *context;
    bool registered;
} CallbackInfo;

// 历史统计记录
typedef struct {
    ListNode node;
    uint64_t timestamp;
    MonitorType type;
    void *data;
    uint32_t dataSize;
} HistoryRecord;

// 全局监控上下文
static MonitorContext g_monitorCtx;
static CallbackInfo g_callbacks[MONITOR_TYPE_MAX];
static pthread_mutex_t g_monitorMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;

// 历史记录存储
static HistoryRecord *g_historyRecords[MONITOR_TYPE_MAX];
static uint32_t g_historyCounts[MONITOR_TYPE_MAX];

// 前向声明内部函数
static int ReadCpuStats(CpuStats *stats);
static int ReadMemoryStats(MemoryStats *stats);
static int ReadProcessInfo(ProcessInfo *info, int pid);
static void FreeProcessList(void);
static void FreeAlarmList(void);
static void FreePerfRecordList(void);
static int UpdateAllStats(void);

// 初始化监控模块
int InitMonitor(const MonitorConfig *config)
{
    if (g_initialized) {
        INIT_LOGW("Monitor already initialized");
        return MONITOR_OK;
    }

    pthread_mutex_lock(&g_monitorMutex);

    // 初始化上下文
    (void)memset_s(&g_monitorCtx, sizeof(MonitorContext), 0, sizeof(MonitorContext));
    g_monitorCtx.state = MONITOR_STATE_IDLE;

    // 设置配置
    if (config != NULL) {
        if (memcpy_s(&g_monitorCtx.config, sizeof(MonitorConfig),
            config, sizeof(MonitorConfig)) != EOK) {
            pthread_mutex_unlock(&g_monitorMutex);
            INIT_LOGE("Failed to copy monitor config");
            return MONITOR_ERROR;
        }
    } else {
        // 使用默认配置
        g_monitorCtx.config.sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_MS;
        g_monitorCtx.config.historySize = DEFAULT_HISTORY_SIZE;
        g_monitorCtx.config.enableCpuMonitor = true;
        g_monitorCtx.config.enableMemMonitor = true;
        g_monitorCtx.config.enableProcMonitor = true;
        g_monitorCtx.config.enableDiskMonitor = true;
        g_monitorCtx.config.enableNetMonitor = true;
        g_monitorCtx.config.cpuThreshold = 80;  // 80%
        g_monitorCtx.config.memThreshold = 85;  // 85%
        g_monitorCtx.config.diskThreshold = 90; // 90%
    }

    // 初始化链表
    OH_ListInit(&g_monitorCtx.processList);
    OH_ListInit(&g_monitorCtx.alarmList);
    OH_ListInit(&g_monitorCtx.perfRecordList);

    // 初始化回调
    for (int i = 0; i < MONITOR_TYPE_MAX; i++) {
        g_callbacks[i].callback = NULL;
        g_callbacks[i].context = NULL;
        g_callbacks[i].registered = false;
        g_historyCounts[i] = 0;
        g_historyRecords[i] = NULL;
    }

    g_initialized = true;
    g_monitorCtx.state = MONITOR_STATE_IDLE;
    g_monitorCtx.alarmCount = 0;
    g_monitorCtx.recordCount = 0;

    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor module initialized successfully");
    return MONITOR_OK;
}

// 销毁监控模块
void DestroyMonitor(void)
{
    if (!g_initialized) {
        return;
    }

    pthread_mutex_lock(&g_monitorMutex);

    g_monitorCtx.state = MONITOR_STATE_IDLE;

    // 释放进程列表
    FreeProcessList();

    // 释放告警列表
    FreeAlarmList();

    // 释放性能记录列表
    FreePerfRecordList();

    // 释放历史记录
    for (int i = 0; i < MONITOR_TYPE_MAX; i++) {
        if (g_historyRecords[i] != NULL) {
            free(g_historyRecords[i]);
            g_historyRecords[i] = NULL;
        }
        g_historyCounts[i] = 0;
    }

    g_initialized = false;

    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor module destroyed");
}

// 启动监控
int StartMonitor(void)
{
    if (!g_initialized) {
        INIT_LOGE("Monitor not initialized");
        return MONITOR_ERROR;
    }

    pthread_mutex_lock(&g_monitorMutex);

    if (g_monitorCtx.state == MONITOR_STATE_RUNNING) {
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_OK;
    }

    // 执行首次统计更新
    int ret = UpdateAllStats();
    if (ret != MONITOR_OK) {
        INIT_LOGE("Failed to update initial stats");
        g_monitorCtx.state = MONITOR_STATE_ERROR;
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_RUNNING;
    clock_gettime(CLOCK_MONOTONIC, &g_monitorCtx.lastUpdateTime);

    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor started");
    return MONITOR_OK;
}

// 停止监控
int StopMonitor(void)
{
    if (!g_initialized) {
        return MONITOR_ERROR;
    }

    pthread_mutex_lock(&g_monitorMutex);
    g_monitorCtx.state = MONITOR_STATE_IDLE;
    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor stopped");
    return MONITOR_OK;
}

// 暂停监控
int PauseMonitor(void)
{
    if (!g_initialized) {
        return MONITOR_ERROR;
    }

    pthread_mutex_lock(&g_monitorMutex);

    if (g_monitorCtx.state != MONITOR_STATE_RUNNING) {
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_PAUSED;
    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor paused");
    return MONITOR_OK;
}

// 恢复监控
int ResumeMonitor(void)
{
    if (!g_initialized) {
        return MONITOR_ERROR;
    }

    pthread_mutex_lock(&g_monitorMutex);

    if (g_monitorCtx.state != MONITOR_STATE_PAUSED) {
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_RUNNING;
    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor resumed");
    return MONITOR_OK;
}

// 获取监控状态
MonitorState GetMonitorState(void)
{
    return g_monitorCtx.state;
}

// 更新CPU统计
int UpdateCpuStats(CpuStats *stats)
{
    if (stats == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    int ret = ReadCpuStats(&g_monitorCtx.cpuStats);
    if (ret == MONITOR_OK) {
        if (memcpy_s(stats, sizeof(CpuStats),
            &g_monitorCtx.cpuStats, sizeof(CpuStats)) != EOK) {
            return MONITOR_ERROR;
        }
    }

    return ret;
}

// 更新内存统计
int UpdateMemoryStats(MemoryStats *stats)
{
    if (stats == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    int ret = ReadMemoryStats(&g_monitorCtx.memStats);
    if (ret == MONITOR_OK) {
        if (memcpy_s(stats, sizeof(MemoryStats),
            &g_monitorCtx.memStats, sizeof(MemoryStats)) != EOK) {
            return MONITOR_ERROR;
        }
    }

    return ret;
}

// 更新进程统计
int UpdateProcessStats(void)
{
    FreeProcessList();

    DIR *procDir = opendir("/proc");
    if (procDir == NULL) {
        INIT_LOGE("Failed to open /proc directory: %d", errno);
        return MONITOR_ERROR;
    }

    struct dirent *entry;
    int processCount = 0;

    while ((entry = readdir(procDir)) != NULL && processCount < MAX_PROCESS_COUNT) {
        // 跳过非数字目录
        if (entry->d_type != DT_DIR) {
            continue;
        }

        char *endptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 0) {
            continue;
        }

        ProcessInfo *info = (ProcessInfo *)calloc(1, sizeof(ProcessInfo));
        if (info == NULL) {
            INIT_LOGE("Failed to allocate memory for process info");
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

// 更新磁盘统计
int UpdateDiskStats(DiskStats *stats, uint32_t maxCount)
{
    if (stats == NULL || maxCount == 0) {
        return MONITOR_INVALID_PARAM;
    }

    FILE *fp = fopen("/proc/diskstats", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/diskstats: %d", errno);
        return MONITOR_ERROR;
    }

    char line[MAX_LINE_LENGTH];
    uint32_t count = 0;

    while (fgets(line, sizeof(line), fp) != NULL && count < maxCount) {
        uint32_t major, minor;
        char deviceName[64];

        int parsed = sscanf_s(line, "%u %u %63s", &major, &minor, deviceName, sizeof(deviceName));
        if (parsed < 3) {
            continue;
        }

        uint64_t readsCompleted, readsMerged, sectorsRead, readTimeMs;
        uint64_t writesCompleted, writesMerged, sectorsWritten, writeTimeMs;
        uint64_t ioInProgress, ioTimeMs, weightedIoTimeMs;

        int fields = sscanf_s(line,
            "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &major, &minor, deviceName, sizeof(deviceName),
            &readsCompleted, &readsMerged, &sectorsRead, &readTimeMs,
            &writesCompleted, &writesMerged, &sectorsWritten, &writeTimeMs,
            &ioInProgress, &ioTimeMs, &weightedIoTimeMs);

        if (fields >= 14) {
            strncpy_s(stats[count].deviceName, sizeof(stats[count].deviceName),
                deviceName, strlen(deviceName));
            stats[count].readsCompleted = readsCompleted;
            stats[count].readsMerged = readsMerged;
            stats[count].sectorsRead = sectorsRead;
            stats[count].readTimeMs = readTimeMs;
            stats[count].writesCompleted = writesCompleted;
            stats[count].writesMerged = writesMerged;
            stats[count].sectorsWritten = sectorsWritten;
            stats[count].writeTimeMs = writeTimeMs;
            stats[count].ioInProgress = ioInProgress;
            stats[count].ioTimeMs = ioTimeMs;
            stats[count].weightedIoTimeMs = weightedIoTimeMs;
            count++;
        }
    }

    fclose(fp);

    // 复制到全局统计
    uint32_t copyCount = (count < 16) ? count : 16;
    if (memcpy_s(g_monitorCtx.diskStats, sizeof(DiskStats) * 16,
        stats, sizeof(DiskStats) * copyCount) != EOK) {
        return MONITOR_ERROR;
    }

    return MONITOR_OK;
}

// 更新网络统计
int UpdateNetworkStats(NetworkStats *stats, uint32_t maxCount)
{
    if (stats == NULL || maxCount == 0) {
        return MONITOR_INVALID_PARAM;
    }

    FILE *fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/net/dev: %d", errno);
        return MONITOR_ERROR;
    }

    char line[MAX_LINE_LENGTH];
    uint32_t count = 0;

    // 跳过前两行标题
    if (fgets(line, sizeof(line), fp) == NULL ||
        fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return MONITOR_ERROR;
    }

    while (fgets(line, sizeof(line), fp) != NULL && count < maxCount) {
        char *colon = strchr(line, ':');
        if (colon == NULL) {
            continue;
        }

        char interface[32];
        char *start = line;
        while (*start == ' ') {
            start++;
        }

        size_t ifaceLen = colon - start;
        if (ifaceLen >= sizeof(interface)) {
            ifaceLen = sizeof(interface) - 1;
        }
        strncpy_s(interface, sizeof(interface), start, ifaceLen);
        interface[ifaceLen] = '\0';

        uint64_t rxBytes, rxPackets, rxErrors, rxDropped, rxFifo, rxFrame, rxCompressed, rxMulticast;
        uint64_t txBytes, txPackets, txErrors, txDropped, txFifo, txCollisions, txCarrier, txCompressed;

        int fields = sscanf_s(colon + 1,
            "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &rxBytes, &rxPackets, &rxErrors, &rxDropped, &rxFifo, &rxFrame,
            &rxCompressed, &rxMulticast, &txBytes, &txPackets, &txErrors,
            &txDropped, &txFifo, &txCollisions, &txCarrier, &txCompressed);

        if (fields >= 16) {
            strncpy_s(stats[count].interface, sizeof(stats[count].interface),
                interface, strlen(interface));
            stats[count].rxBytes = rxBytes;
            stats[count].rxPackets = rxPackets;
            stats[count].rxErrors = rxErrors;
            stats[count].rxDropped = rxDropped;
            stats[count].rxFifo = rxFifo;
            stats[count].rxFrame = rxFrame;
            stats[count].rxCompressed = rxCompressed;
            stats[count].rxMulticast = rxMulticast;
            stats[count].txBytes = txBytes;
            stats[count].txPackets = txPackets;
            stats[count].txErrors = txErrors;
            stats[count].txDropped = txDropped;
            stats[count].txFifo = txFifo;
            stats[count].txCollisions = txCollisions;
            stats[count].txCarrier = txCarrier;
            stats[count].txCompressed = txCompressed;
            count++;
        }
    }

    fclose(fp);

    // 复制到全局统计
    uint32_t copyCount = (count < 16) ? count : 16;
    if (memcpy_s(g_monitorCtx.netStats, sizeof(NetworkStats) * 16,
        stats, sizeof(NetworkStats) * copyCount) != EOK) {
        return MONITOR_ERROR;
    }

    return MONITOR_OK;
}

// 获取CPU统计
const CpuStats* GetCpuStats(void)
{
    return &g_monitorCtx.cpuStats;
}

// 获取内存统计
const MemoryStats* GetMemoryStats(void)
{
    return &g_monitorCtx.memStats;
}

// 获取进程列表
const ListNode* GetProcessList(void)
{
    return &g_monitorCtx.processList;
}

// 获取进程信息
ProcessInfo* GetProcessInfo(int pid)
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

// 添加告警
int AddMonitorAlarm(MonitorType type, AlarmLevel level,
    const char *message, uint32_t threshold, uint32_t actualValue)
{
    if (message == NULL || g_monitorCtx.alarmCount >= MAX_ALARM_COUNT) {
        return MONITOR_INVALID_PARAM;
    }

    MonitorAlarm *alarm = (MonitorAlarm *)calloc(1, sizeof(MonitorAlarm));
    if (alarm == NULL) {
        INIT_LOGE("Failed to allocate memory for alarm");
        return MONITOR_ERROR;
    }

    alarm->type = type;
    alarm->level = level;
    alarm->threshold = threshold;
    alarm->actualValue = actualValue;
    alarm->handled = false;
    clock_gettime(CLOCK_REALTIME, &alarm->timestamp);

    if (strncpy_s(alarm->message, sizeof(alarm->message),
        message, strlen(message)) != EOK) {
        free(alarm);
        return MONITOR_ERROR;
    }

    OH_ListAddTail(&g_monitorCtx.alarmList, &alarm->node);
    g_monitorCtx.alarmCount++;

    INIT_LOGI("Added monitor alarm: type=%d level=%d message=%s", type, level, message);
    return MONITOR_OK;
}

// 获取告警列表
const ListNode* GetAlarmList(void)
{
    return &g_monitorCtx.alarmList;
}

// 清除已处理告警
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

// 开始性能记录
PerfRecord* BeginPerfRecord(const char *name, uint32_t type)
{
    if (name == NULL || g_monitorCtx.recordCount >= MAX_PERF_RECORD_COUNT) {
        return NULL;
    }

    PerfRecord *record = (PerfRecord *)calloc(1, sizeof(PerfRecord));
    if (record == NULL) {
        INIT_LOGE("Failed to allocate memory for perf record");
        return NULL;
    }

    if (strncpy_s(record->name, sizeof(record->name), name, strlen(name)) != EOK) {
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

// 结束性能记录
int EndPerfRecord(PerfRecord *record)
{
    if (record == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    clock_gettime(CLOCK_MONOTONIC, &record->endTime);

    record->durationNs = (record->endTime.tv_sec - record->startTime.tv_sec) * 1000000000LL +
                         (record->endTime.tv_nsec - record->startTime.tv_nsec);

    return MONITOR_OK;
}

// 获取性能记录列表
const ListNode* GetPerfRecords(void)
{
    return &g_monitorCtx.perfRecordList;
}

// 打印监控摘要
void PrintMonitorSummary(void)
{
    INIT_LOGI("=== System Monitor Summary ===");
    INIT_LOGI("State: %d", g_monitorCtx.state);
    INIT_LOGI("Alarm Count: %u", g_monitorCtx.alarmCount);
    INIT_LOGI("Perf Record Count: %u", g_monitorCtx.recordCount);

    INIT_LOGI("CPU Usage: %u.%02u%%",
        g_monitorCtx.cpuStats.totalUsage / 100,
        g_monitorCtx.cpuStats.totalUsage % 100);
    INIT_LOGI("CPU Cores: %u", g_monitorCtx.cpuStats.cpuCoreNum);

    INIT_LOGI("Memory Usage: %u.%02u%%",
        g_monitorCtx.memStats.usagePercent / 100,
        g_monitorCtx.memStats.usagePercent % 100);
    INIT_LOGI("Total Memory: %llu MB",
        (unsigned long long)(g_monitorCtx.memStats.totalMem / 1024 / 1024));
    INIT_LOGI("Free Memory: %llu MB",
        (unsigned long long)(g_monitorCtx.memStats.freeMem / 1024 / 1024));
    INIT_LOGI("Available Memory: %llu MB",
        (unsigned long long)(g_monitorCtx.memStats.availableMem / 1024 / 1024));

    INIT_LOGI("Context Switches: %u", g_monitorCtx.cpuStats.contextSwitches);
    INIT_LOGI("Processes Running: %u", g_monitorCtx.cpuStats.processesRunning);
    INIT_LOGI("Processes Blocked: %u", g_monitorCtx.cpuStats.processesBlocked);
}

// 导出监控数据到文件
int ExportMonitorData(const char *filePath)
{
    if (filePath == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    FILE *fp = fopen(filePath, "w");
    if (fp == NULL) {
        INIT_LOGE("Failed to open export file: %s, err=%d", filePath, errno);
        return MONITOR_ERROR;
    }

    fprintf(fp, "=== System Monitor Report ===\n\n");

    fprintf(fp, "[CPU Statistics]\n");
    fprintf(fp, "  CPU Usage: %u.%02u%%\n",
        g_monitorCtx.cpuStats.totalUsage / 100,
        g_monitorCtx.cpuStats.totalUsage % 100);
    fprintf(fp, "  CPU Cores: %u\n", g_monitorCtx.cpuStats.cpuCoreNum);
    fprintf(fp, "  User Mode Time: %llu ms\n",
        (unsigned long long)g_monitorCtx.cpuStats.userModeTime);
    fprintf(fp, "  System Mode Time: %llu ms\n",
        (unsigned long long)g_monitorCtx.cpuStats.systemTime);
    fprintf(fp, "  Idle Time: %llu ms\n",
        (unsigned long long)g_monitorCtx.cpuStats.idleTime);
    fprintf(fp, "  IO Wait Time: %llu ms\n",
        (unsigned long long)g_monitorCtx.cpuStats.ioWaitTime);
    fprintf(fp, "  Context Switches: %u\n", g_monitorCtx.cpuStats.contextSwitches);
    fprintf(fp, "  Processes Created: %u\n", g_monitorCtx.cpuStats.processesCreated);
    fprintf(fp, "  Processes Running: %u\n", g_monitorCtx.cpuStats.processesRunning);
    fprintf(fp, "  Processes Blocked: %u\n\n", g_monitorCtx.cpuStats.processesBlocked);

    fprintf(fp, "[Memory Statistics]\n");
    fprintf(fp, "  Memory Usage: %u.%02u%%\n",
        g_monitorCtx.memStats.usagePercent / 100,
        g_monitorCtx.memStats.usagePercent % 100);
    fprintf(fp, "  Total Memory: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.totalMem / 1024 / 1024));
    fprintf(fp, "  Free Memory: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.freeMem / 1024 / 1024));
    fprintf(fp, "  Available Memory: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.availableMem / 1024 / 1024));
    fprintf(fp, "  Buffers: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.buffers / 1024 / 1024));
    fprintf(fp, "  Cached: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.cached / 1024 / 1024));
    fprintf(fp, "  Active Memory: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.activeMem / 1024 / 1024));
    fprintf(fp, "  Inactive Memory: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.inactiveMem / 1024 / 1024));
    fprintf(fp, "  Swap Total: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.swapTotal / 1024 / 1024));
    fprintf(fp, "  Swap Free: %llu MB\n",
        (unsigned long long)(g_monitorCtx.memStats.swapFree / 1024 / 1024));
    fprintf(fp, "  Swap Usage: %u.%02u%%\n\n",
        g_monitorCtx.memStats.swapUsagePercent / 100,
        g_monitorCtx.memStats.swapUsagePercent % 100);

    fprintf(fp, "[Alarms] (Total: %u)\n", g_monitorCtx.alarmCount);
    ListNode *node = g_monitorCtx.alarmList.next;
    while (node != &g_monitorCtx.alarmList) {
        MonitorAlarm *alarm = (MonitorAlarm *)node;
        fprintf(fp, "  [%s] Type:%d Level:%d Threshold:%u Actual:%u Message:%s\n",
            alarm->handled ? "Handled" : "Active",
            alarm->type, alarm->level,
            alarm->threshold, alarm->actualValue, alarm->message);
        node = node->next;
    }

    fprintf(fp, "\n[Performance Records] (Total: %u)\n", g_monitorCtx.recordCount);
    node = g_monitorCtx.perfRecordList.next;
    while (node != &g_monitorCtx.perfRecordList) {
        PerfRecord *record = (PerfRecord *)node;
        fprintf(fp, "  Name:%s Duration:%llu ns\n",
            record->name, (unsigned long long)record->durationNs);
        node = node->next;
    }

    fclose(fp);

    INIT_LOGI("Monitor data exported to: %s", filePath);
    return MONITOR_OK;
}

// 计算CPU使用率
uint32_t CalculateCpuUsage(const CpuStats *prev, const CpuStats *curr)
{
    if (prev == NULL || curr == NULL) {
        return 0;
    }

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

    uint32_t usage = (uint32_t)(((totalDiff - idleDiff) * 10000ULL) / totalDiff);
    return usage;
}

// 计算内存使用率
uint32_t CalculateMemUsage(const MemoryStats *stats)
{
    if (stats == NULL || stats->totalMem == 0) {
        return 0;
    }

    uint64_t usedMem = stats->totalMem - stats->availableMem;
    uint32_t usage = (uint32_t)((usedMem * 10000ULL) / stats->totalMem);
    return usage;
}

// 查找占用CPU最多的进程
ProcessInfo* FindTopCpuProcess(void)
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

// 查找占用内存最多的进程
ProcessInfo* FindTopMemProcess(void)
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

// 注册监控回调
int RegisterMonitorCallback(MonitorType type, MonitorCallback callback, void *context)
{
    if (type >= MONITOR_TYPE_MAX || callback == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_monitorMutex);
    g_callbacks[type].callback = callback;
    g_callbacks[type].context = context;
    g_callbacks[type].registered = true;
    pthread_mutex_unlock(&g_monitorMutex);

    return MONITOR_OK;
}

// 注销监控回调
int UnregisterMonitorCallback(MonitorType type)
{
    if (type >= MONITOR_TYPE_MAX) {
        return MONITOR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_monitorMutex);
    g_callbacks[type].callback = NULL;
    g_callbacks[type].context = NULL;
    g_callbacks[type].registered = false;
    pthread_mutex_unlock(&g_monitorMutex);

    return MONITOR_OK;
}

// 阈值检查
int CheckThresholds(void)
{
    int alarmsGenerated = 0;

    // CPU阈值检查
    if (g_monitorCtx.config.enableCpuMonitor &&
        g_monitorCtx.cpuStats.totalUsage > g_monitorCtx.config.cpuThreshold * 100) {
        AddMonitorAlarm(MONITOR_TYPE_CPU, ALARM_LEVEL_WARNING,
            "CPU usage exceeds threshold",
            g_monitorCtx.config.cpuThreshold * 100,
            g_monitorCtx.cpuStats.totalUsage);
        alarmsGenerated++;
    }

    // 内存阈值检查
    if (g_monitorCtx.config.enableMemMonitor &&
        g_monitorCtx.memStats.usagePercent > g_monitorCtx.config.memThreshold * 100) {
        AddMonitorAlarm(MONITOR_TYPE_MEMORY, ALARM_LEVEL_WARNING,
            "Memory usage exceeds threshold",
            g_monitorCtx.config.memThreshold * 100,
            g_monitorCtx.memStats.usagePercent);
        alarmsGenerated++;
    }

    return alarmsGenerated;
}

// 获取历史统计
int GetHistoryStats(MonitorType type, void *stats, uint32_t index)
{
    if (type >= MONITOR_TYPE_MAX || stats == NULL ||
        index >= g_historyCounts[type] || g_historyRecords[type] == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    HistoryRecord *record = &g_historyRecords[type][index];
    if (memcpy_s(stats, record->dataSize, record->data, record->dataSize) != EOK) {
        return MONITOR_ERROR;
    }

    return MONITOR_OK;
}

// ============== 内部函数实现 ==============

// 读取CPU统计信息
static int ReadCpuStats(CpuStats *stats)
{
    if (stats == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/stat: %d", errno);
        return MONITOR_ERROR;
    }

    char line[MAX_LINE_LENGTH];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return MONITOR_ERROR;
    }

    char cpuLabel[16];
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;

    int parsed = sscanf_s(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        cpuLabel, sizeof(cpuLabel),
        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

    fclose(fp);

    if (parsed < 5) {
        INIT_LOGE("Failed to parse /proc/stat");
        return MONITOR_ERROR;
    }

    stats->userModeTime = user;
    stats->niceTime = nice;
    stats->systemTime = system;
    stats->idleTime = idle;
    stats->ioWaitTime = (parsed >= 6) ? iowait : 0;
    stats->irqTime = (parsed >= 7) ? irq : 0;
    stats->softIrqTime = (parsed >= 8) ? softirq : 0;
    stats->stealTime = (parsed >= 9) ? steal : 0;
    stats->guestTime = (parsed >= 10) ? guest : 0;
    stats->guestNiceTime = (parsed >= 11) ? guest_nice : 0;

    // 获取CPU核心数
    stats->cpuCoreNum = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);

    // 读取进程相关信息
    fp = fopen("/proc/stat", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strncmp(line, "ctxt ", 5) == 0) {
                sscanf_s(line, "ctxt %u", &stats->contextSwitches);
            } else if (strncmp(line, "processes ", 10) == 0) {
                sscanf_s(line, "processes %u", &stats->processesCreated);
            } else if (strncmp(line, "procs_running ", 14) == 0) {
                sscanf_s(line, "procs_running %u", &stats->processesRunning);
            } else if (strncmp(line, "procs_blocked ", 14) == 0) {
                sscanf_s(line, "procs_blocked %u", &stats->processesBlocked);
            }
        }
        fclose(fp);
    }

    return MONITOR_OK;
}

// 读取内存统计信息
static int ReadMemoryStats(MemoryStats *stats)
{
    if (stats == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/meminfo: %d", errno);
        return MONITOR_ERROR;
    }

    char line[MAX_LINE_LENGTH];
    (void)memset_s(stats, sizeof(MemoryStats), 0, sizeof(MemoryStats));

    while (fgets(line, sizeof(line), fp) != NULL) {
        char key[64];
        uint64_t value;
        char unit[16];

        if (sscanf_s(line, "%63s %llu %15s", key, sizeof(key), &value, unit, sizeof(unit)) >= 2) {
            // 统一转换为字节单位
            if (strcmp(unit, "kB") == 0 || strcmp(unit, "KB") == 0) {
                value *= 1024;
            }

            if (strcmp(key, "MemTotal:") == 0) {
                stats->totalMem = value;
            } else if (strcmp(key, "MemFree:") == 0) {
                stats->freeMem = value;
            } else if (strcmp(key, "MemAvailable:") == 0) {
                stats->availableMem = value;
            } else if (strcmp(key, "Buffers:") == 0) {
                stats->buffers = value;
            } else if (strcmp(key, "Cached:") == 0) {
                stats->cached = value;
            } else if (strcmp(key, "SwapCached:") == 0) {
                stats->swapCached = value;
            } else if (strcmp(key, "Active:") == 0) {
                stats->activeMem = value;
            } else if (strcmp(key, "Inactive:") == 0) {
                stats->inactiveMem = value;
            } else if (strcmp(key, "Active(anon):") == 0) {
                stats->activeAnon = value;
            } else if (strcmp(key, "Inactive(anon):") == 0) {
                stats->inactiveAnon = value;
            } else if (strcmp(key, "Active(file):") == 0) {
                stats->activeFile = value;
            } else if (strcmp(key, "Inactive(file):") == 0) {
                stats->inactiveFile = value;
            } else if (strcmp(key, "Unevictable:") == 0) {
                stats->unevictable = value;
            } else if (strcmp(key, "Mlocked:") == 0) {
                stats->mlocked = value;
            } else if (strcmp(key, "SwapTotal:") == 0) {
                stats->swapTotal = value;
            } else if (strcmp(key, "SwapFree:") == 0) {
                stats->swapFree = value;
            } else if (strcmp(key, "Dirty:") == 0) {
                stats->dirtyPages = value;
            } else if (strcmp(key, "Writeback:") == 0) {
                stats->writeback = value;
            } else if (strcmp(key, "AnonPages:") == 0) {
                stats->anonPages = value;
            } else if (strcmp(key, "Mapped:") == 0) {
                stats->mapped = value;
            } else if (strcmp(key, "Shmem:") == 0) {
                stats->shmem = value;
            } else if (strcmp(key, "Slab:") == 0) {
                stats->slab = value;
            } else if (strcmp(key, "SReclaimable:") == 0) {
                stats->slabReclaimable = value;
            } else if (strcmp(key, "SUnreclaim:") == 0) {
                stats->slabUnreclaimable = value;
            } else if (strcmp(key, "KernelStack:") == 0) {
                stats->kernelStack = value;
            } else if (strcmp(key, "PageTables:") == 0) {
                stats->pageTables = value;
            } else if (strcmp(key, "NFS_Unstable:") == 0) {
                stats->nfsUnstable = value;
            } else if (strcmp(key, "Bounce:") == 0) {
                stats->bounce = value;
            } else if (strcmp(key, "WritebackTmp:") == 0) {
                stats->writebackTmp = value;
            } else if (strcmp(key, "CommitLimit:") == 0) {
                stats->commitLimit = value;
            } else if (strcmp(key, "Committed_AS:") == 0) {
                stats->committedAs = value;
            } else if (strcmp(key, "VmallocTotal:") == 0) {
                stats->vmallocTotal = value;
            } else if (strcmp(key, "VmallocUsed:") == 0) {
                stats->vmallocUsed = value;
            } else if (strcmp(key, "VmallocChunk:") == 0) {
                stats->vmallocChunk = value;
            }
        }
    }

    fclose(fp);

    // 计算使用率
    if (stats->totalMem > 0) {
        uint64_t usedMem = stats->totalMem - stats->availableMem;
        stats->usagePercent = (uint32_t)((usedMem * 10000ULL) / stats->totalMem);

        if (stats->swapTotal > 0) {
            uint64_t usedSwap = stats->swapTotal - stats->swapFree;
            stats->swapUsagePercent = (uint32_t)((usedSwap * 10000ULL) / stats->swapTotal);
        }
    }

    return MONITOR_OK;
}

// 读取进程信息
static int ReadProcessInfo(ProcessInfo *info, int pid)
{
    if (info == NULL || pid <= 0) {
        return MONITOR_INVALID_PARAM;
    }

    char path[256];
    char line[MAX_LINE_LENGTH];

    // 读取 /proc/[pid]/stat
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1,
        "/proc/%d/stat", pid) < 0) {
        return MONITOR_ERROR;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return MONITOR_ERROR;
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return MONITOR_ERROR;
    }
    fclose(fp);

    // 解析进程状态
    char *start = strchr(line, '(');
    char *end = strrchr(line, ')');
    if (start == NULL || end == NULL) {
        return MONITOR_ERROR;
    }

    int nameLen = end - start - 1;
    if (nameLen >= (int)sizeof(info->name)) {
        nameLen = sizeof(info->name) - 1;
    }
    strncpy_s(info->name, sizeof(info->name), start + 1, nameLen);
    info->name[nameLen] = '\0';

    // 解析其余字段
    char *next = end + 2;
    info->state = *next++;

    char *token = strtok(next, " ");
    int fieldIndex = 0;
    while (token != NULL && fieldIndex < 50) {
        switch (fieldIndex) {
            case 0: info->ppid = atoi(token); break;
            case 11: info->utime = strtoull(token, NULL, 10); break;
            case 12: info->stime = strtoull(token, NULL, 10); break;
            case 15: info->priority = atol(token); break;
            case 16: info->nice = atol(token); break;
            case 17: info->numThreads = atol(token); break;
            case 20: info->startTime = strtoull(token, NULL, 10); break;
            case 21: info->vsize = atol(token); break;
            case 22: info->rss = atol(token) * sysconf(_SC_PAGESIZE); break;
        }
        token = strtok(NULL, " ");
        fieldIndex++;
    }

    info->pid = pid;

    // 读取命令行
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1,
        "/proc/%d/cmdline", pid) < 0) {
        return MONITOR_OK;
    }

    fp = fopen(path, "r");
    if (fp != NULL) {
        size_t len = fread(info->cmdline, 1, sizeof(info->cmdline) - 1, fp);
        if (len > 0) {
            info->cmdline[len] = '\0';
            // 替换空字符为空格
            for (size_t i = 0; i < len; i++) {
                if (info->cmdline[i] == '\0') {
                    info->cmdline[i] = ' ';
                }
            }
        }
        fclose(fp);
    }

    return MONITOR_OK;
}

// 释放进程列表
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

// 释放告警列表
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

// 释放性能记录列表
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

// 更新所有统计信息
static int UpdateAllStats(void)
{
    int ret = MONITOR_OK;

    if (g_monitorCtx.config.enableCpuMonitor) {
        ret = ReadCpuStats(&g_monitorCtx.cpuStats);
        if (ret != MONITOR_OK) {
            INIT_LOGE("Failed to read CPU stats");
        }
    }

    if (g_monitorCtx.config.enableMemMonitor) {
        ret = ReadMemoryStats(&g_monitorCtx.memStats);
        if (ret != MONITOR_OK) {
            INIT_LOGE("Failed to read memory stats");
        }
    }

    if (g_monitorCtx.config.enableProcMonitor) {
        ret = UpdateProcessStats();
        if (ret != MONITOR_OK) {
            INIT_LOGE("Failed to update process stats");
        }
    }

    if (g_monitorCtx.config.enableDiskMonitor) {
        UpdateDiskStats(g_monitorCtx.diskStats, 16);
    }

    if (g_monitorCtx.config.enableNetMonitor) {
        UpdateNetworkStats(g_monitorCtx.netStats, 16);
    }

    return ret;
}

// 系统诊断
int RunSystemDiagnosis(const char *outputPath)
{
    if (outputPath == NULL) {
        return MONITOR_INVALID_PARAM;
    }

    FILE *fp = fopen(outputPath, "w");
    if (fp == NULL) {
        INIT_LOGE("Failed to open diagnosis output file: %s", outputPath);
        return MONITOR_ERROR;
    }

    fprintf(fp, "=== System Diagnosis Report ===\n\n");
    fprintf(fp, "Generated at: %ld\n\n", time(NULL));

    // CPU诊断
    fprintf(fp, "[CPU Diagnosis]\n");
    uint32_t cpuUsage = g_monitorCtx.cpuStats.totalUsage;
    if (cpuUsage > 9000) {  // > 90%
        fprintf(fp, "  WARNING: CPU usage is very high (%u.%02u%%)\n",
            cpuUsage / 100, cpuUsage % 100);
        fprintf(fp, "  Recommendation: Check for runaway processes\n");
    } else if (cpuUsage > 8000) {  // > 80%
        fprintf(fp, "  CAUTION: CPU usage is elevated (%u.%02u%%)\n",
            cpuUsage / 100, cpuUsage % 100);
    } else {
        fprintf(fp, "  OK: CPU usage is normal (%u.%02u%%)\n",
            cpuUsage / 100, cpuUsage % 100);
    }

    // 内存诊断
    fprintf(fp, "\n[Memory Diagnosis]\n");
    uint32_t memUsage = g_monitorCtx.memStats.usagePercent;
    if (memUsage > 9000) {
        fprintf(fp, "  CRITICAL: Memory usage is very high (%u.%02u%%)\n",
            memUsage / 100, memUsage % 100);
        fprintf(fp, "  Recommendation: Free memory or add more RAM\n");
    } else if (memUsage > 8500) {
        fprintf(fp, "  WARNING: Memory usage is elevated (%u.%02u%%)\n",
            memUsage / 100, memUsage % 100);
        fprintf(fp, "  Recommendation: Consider clearing caches\n");
    } else {
        fprintf(fp, "  OK: Memory usage is normal (%u.%02u%%)\n",
            memUsage / 100, memUsage % 100);
    }

    if (g_monitorCtx.memStats.swapUsagePercent > 5000) {
        fprintf(fp, "  INFO: Swap usage: %u.%02u%%\n",
            g_monitorCtx.memStats.swapUsagePercent / 100,
            g_monitorCtx.memStats.swapUsagePercent % 100);
        fprintf(fp, "  Note: High swap usage may affect performance\n");
    }

    // 进程诊断
    fprintf(fp, "\n[Process Diagnosis]\n");
    ProcessInfo *topCpu = FindTopCpuProcess();
    if (topCpu != NULL) {
        fprintf(fp, "  Top CPU consumer: %s (PID: %d, CPU: %llu%%)\n",
            topCpu->name, topCpu->pid, topCpu->cpuUsage);
    }

    ProcessInfo *topMem = FindTopMemProcess();
    if (topMem != NULL) {
        fprintf(fp, "  Top Memory consumer: %s (PID: %d, Mem: %llu%%)\n",
            topMem->name, topMem->pid, topMem->memUsage);
    }

    fprintf(fp, "\n  Total processes monitored: %u\n", g_monitorCtx.recordCount);
    fprintf(fp, "  Context switches: %u\n", g_monitorCtx.cpuStats.contextSwitches);
    fprintf(fp, "  Processes blocked: %u\n", g_monitorCtx.cpuStats.processesBlocked);

    // 告警诊断
    fprintf(fp, "\n[Active Alarms] (%u total)\n", g_monitorCtx.alarmCount);
    ListNode *node = g_monitorCtx.alarmList.next;
    while (node != &g_monitorCtx.alarmList) {
        MonitorAlarm *alarm = (MonitorAlarm *)node;
        if (!alarm->handled) {
            fprintf(fp, "  [%s] %s (Threshold: %u, Actual: %u)\n",
                alarm->level >= ALARM_LEVEL_CRITICAL ? "CRITICAL" :
                alarm->level >= ALARM_LEVEL_WARNING ? "WARNING" : "INFO",
                alarm->message, alarm->threshold, alarm->actualValue);
        }
        node = node->next;
    }

    fprintf(fp, "\n[Recommendations]\n");
    if (cpuUsage > 8000) {
        fprintf(fp, "  - Investigate high CPU usage processes\n");
    }
    if (memUsage > 8500) {
        fprintf(fp, "  - Consider increasing available memory\n");
    }
    if (g_monitorCtx.cpuStats.processesBlocked > 100) {
        fprintf(fp, "  - Check for I/O bottlenecks\n");
    }

    fclose(fp);

    INIT_LOGI("System diagnosis completed: %s", outputPath);
    return MONITOR_OK;
}