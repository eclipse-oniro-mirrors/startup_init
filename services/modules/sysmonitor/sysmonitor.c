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

/* ========== 常量定义 ========== */

/* PROC_STAT字段索引 */
#define PROC_STAT_FIELD_PPID         0
#define PROC_STAT_FIELD_UTIME        11
#define PROC_STAT_FIELD_STIME        12
#define PROC_STAT_FIELD_PRIORITY     15
#define PROC_STAT_FIELD_NICE         16
#define PROC_STAT_FIELD_NUM_THREADS  17
#define PROC_STAT_FIELD_STARTTIME    20
#define PROC_STAT_FIELD_VSIZE        21
#define PROC_STAT_FIELD_RSS          22

/* 诊断阈值常量 */
#define CPU_CRITICAL_THRESHOLD       9000  /* 90.00% */
#define CPU_WARNING_THRESHOLD        7000  /* 70.00% */
#define MEM_CRITICAL_THRESHOLD       9000  /* 90.00% */
#define MEM_WARNING_THRESHOLD        7000  /* 70.00% */
#define SWAP_USAGE_INFO_THRESHOLD    5000  /* 50.00% */
#define BLOCKED_PROC_THRESHOLD       10

/* 字段计数常量 */
#define DISKSTATS_FIELDS             14
#define NETDEV_FIELDS                16

/* 缓冲区常量 */
#define MAX_LINE_LENGTH              4096
#define PROC_PATH_LEN                256
#define MEM_KEY_LEN                  64
#define MEM_UNIT_LEN                 16
#define DEVICE_NAME_MAX_LEN          64
#define NET_IFACE_MAX_LEN            64
#define STAT_FIELD_MAX_COUNT         52

/* 初始化常量 */
#define DECIMAL_BASE                 10
#define BYTES_PER_KB                 1024ULL
#define BYTES_PER_MB                 (1024ULL * 1024ULL)
#define NSEC_PER_SEC                 1000000000ULL

/* ========== 类型定义 ========== */

typedef struct {
    MonitorCallback callback;
    void *context;
    bool registered;
} CallbackInfo;

typedef struct {
    ListNode node;
    uint64_t timestamp;
    MonitorType type;
    void *data;
    uint32_t dataSize;
} HistoryRecord;

typedef struct {
    const char *key;
    uint64_t *target;
} MemInfoField;

typedef struct {
    bool valid;
    uint32_t major;
    uint32_t minor;
    char deviceName[DEVICE_NAME_MAX_LEN];
    uint64_t readsCompleted;
    uint64_t readsMerged;
    uint64_t sectorsRead;
    uint64_t readTimeMs;
    uint64_t writesCompleted;
    uint64_t writesMerged;
    uint64_t sectorsWritten;
    uint64_t writeTimeMs;
    uint64_t ioInProgress;
    uint64_t ioTimeMs;
    uint64_t weightedIoTimeMs;
} DiskStatsParsed;

/* ========== 全局变量 ========== */

static MonitorContext g_monitorCtx;
static CallbackInfo g_callbacks[MONITOR_TYPE_MAX];
static pthread_mutex_t g_monitorMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;

static HistoryRecord *g_historyRecords[MONITOR_TYPE_MAX];
static uint32_t g_historyCounts[MONITOR_TYPE_MAX];

/* ========== 静态函数声明 ========== */

static int ReadCpuStats(CpuStats *stats);
static int ReadMemoryStats(MemoryStats *stats);
static int ReadProcessInfo(ProcessInfo *info, int pid);
static void FreeProcessList(void);
static void FreeAlarmList(void);
static void FreePerfRecordList(void);
static int UpdateAllStats(void);
static void SetDefaultConfig(MonitorConfig *config);
static void InitCallbacks(void);
static void InitHistoryRecords(void);
static void ParseMemInfoLine(const char *line, MemoryStats *stats);
static int ParseProcessStat(const char *line, ProcessInfo *info);
static int ReadProcessCmdline(int pid, char *buf, size_t bufLen);
static int ParseDiskstatsLine(const char *line, DiskStatsParsed *parsed);
static int ParseNetDevLine(char *line, NetworkStats *stats);
static void ParseStatLine(const char *line, CpuStats *stats);
static void CalculateMemUsagePercent(MemoryStats *stats);
static void DiagnoseCpu(FILE *fp, uint32_t cpuUsage);
static void DiagnoseMemory(FILE *fp, uint32_t memUsage);
static void DiagnoseProcesses(FILE *fp);
static const char *GetAlarmLevelString(AlarmLevel level);
static void DiagnoseAlarms(FILE *fp);
static void PrintRecommendations(FILE *fp, uint32_t cpuUsage, uint32_t memUsage);
static void ExportCpuStats(FILE *fp, const CpuStats *stats);
static void ExportMemStats(FILE *fp, const MemoryStats *stats);
static void ExportAlarms(FILE *fp, const ListNode *alarmList);
static void ExportPerfRecords(FILE *fp, const ListNode *recordList);

/* ========== 初始化/销毁 ========== */

static void SetDefaultConfig(MonitorConfig *config)
{
    if (config == NULL) {
        return;
    }
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

static void InitCallbacks(void)
{
    for (uint32_t i = 0; i < MONITOR_TYPE_MAX; i++) {
        g_callbacks[i].callback = NULL;
        g_callbacks[i].context = NULL;
        g_callbacks[i].registered = false;
    }
}

static void InitHistoryRecords(void)
{
    for (uint32_t i = 0; i < MONITOR_TYPE_MAX; i++) {
        g_historyCounts[i] = 0;
        g_historyRecords[i] = NULL;
    }
}

int InitMonitor(const MonitorConfig *config)
{
    INIT_CHECK_RETURN_VALUE(!g_initialized, MONITOR_OK);

    pthread_mutex_lock(&g_monitorMutex);
    int ret = memset_s(&g_monitorCtx, sizeof(MonitorContext), 0, sizeof(MonitorContext));
    if (ret != EOK) {
        pthread_mutex_unlock(&g_monitorMutex);
        INIT_LOGE("Failed to memset monitor context");
        return MONITOR_ERROR;
    }
    g_monitorCtx.state = MONITOR_STATE_IDLE;

    if (config != NULL) {
        ret = memcpy_s(&g_monitorCtx.config, sizeof(MonitorConfig),
            config, sizeof(MonitorConfig));
        if (ret != EOK) {
            pthread_mutex_unlock(&g_monitorMutex);
            INIT_LOGE("Failed to copy monitor config");
            return MONITOR_ERROR;
        }
    } else {
        SetDefaultConfig(&g_monitorCtx.config);
    }

    OH_ListInit(&g_monitorCtx.processList);
    OH_ListInit(&g_monitorCtx.alarmList);
    OH_ListInit(&g_monitorCtx.perfRecordList);

    InitCallbacks();
    InitHistoryRecords();

    g_initialized = true;
    g_monitorCtx.state = MONITOR_STATE_IDLE;
    g_monitorCtx.alarmCount = 0;
    g_monitorCtx.recordCount = 0;

    pthread_mutex_unlock(&g_monitorMutex);
    INIT_LOGI("Monitor module initialized successfully");
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

    for (uint32_t i = 0; i < MONITOR_TYPE_MAX; i++) {
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

/* ========== 状态控制 ========== */

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
        INIT_LOGE("Failed to update initial stats");
        g_monitorCtx.state = MONITOR_STATE_ERROR;
        pthread_mutex_unlock(&g_monitorMutex);
        return MONITOR_ERROR;
    }

    g_monitorCtx.state = MONITOR_STATE_RUNNING;
    clock_gettime(CLOCK_MONOTONIC, (struct timespec *)&g_monitorCtx.lastUpdateTime);
    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor started");
    return MONITOR_OK;
}

int StopMonitor(void)
{
    INIT_CHECK_RETURN_VALUE(g_initialized, MONITOR_ERROR);

    pthread_mutex_lock(&g_monitorMutex);
    g_monitorCtx.state = MONITOR_STATE_IDLE;
    pthread_mutex_unlock(&g_monitorMutex);

    INIT_LOGI("Monitor stopped");
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

    INIT_LOGI("Monitor paused");
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

    INIT_LOGI("Monitor resumed");
    return MONITOR_OK;
}

MonitorState GetMonitorState(void)
{
    return g_monitorCtx.state;
}

/* ========== 更新函数 ========== */

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
        INIT_CHECK_ONLY_ELOG(info != NULL, "Failed to allocate memory for process info");
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

/* ========== 磁盘统计 ========== */

static int ParseDiskstatsLine(const char *line, DiskStatsParsed *parsed)
{
    INIT_CHECK_RETURN_VALUE(line != NULL && parsed != NULL, MONITOR_INVALID_PARAM);

    parsed->valid = false;
    
    uint32_t major = 0;
    uint32_t minor = 0;
    char deviceName[DEVICE_NAME_MAX_LEN] = {0};

    uint64_t readsCompleted = 0;
    uint64_t readsMerged = 0;
    uint64_t sectorsRead = 0;
    uint64_t readTimeMs = 0;
    uint64_t writesCompleted = 0;
    uint64_t writesMerged = 0;
    uint64_t sectorsWritten = 0;
    uint64_t writeTimeMs = 0;
    uint64_t ioInProgress = 0;
    uint64_t ioTimeMs = 0;
    uint64_t weightedIoTimeMs = 0;

    int fields = sscanf_s(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &major, &minor, deviceName, (unsigned int)sizeof(deviceName),
        &readsCompleted, &readsMerged, &sectorsRead, &readTimeMs,
        &writesCompleted, &writesMerged, &sectorsWritten, &writeTimeMs,
        &ioInProgress, &ioTimeMs, &weightedIoTimeMs);
    
    if (fields < DISKSTATS_FIELDS) {
        return MONITOR_ERROR;
    }

    parsed->valid = true;
    parsed->major = major;
    parsed->minor = minor;
    parsed->readsCompleted = readsCompleted;
    parsed->readsMerged = readsMerged;
    parsed->sectorsRead = sectorsRead;
    parsed->readTimeMs = readTimeMs;
    parsed->writesCompleted = writesCompleted;
    parsed->writesMerged = writesMerged;
    parsed->sectorsWritten = sectorsWritten;
    parsed->writeTimeMs = writeTimeMs;
    parsed->ioInProgress = ioInProgress;
    parsed->ioTimeMs = ioTimeMs;
    parsed->weightedIoTimeMs = weightedIoTimeMs;

    int ret = strncpy_s(parsed->deviceName, sizeof(parsed->deviceName),
        deviceName, strlen(deviceName));
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

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
        DiskStatsParsed parsed;
        if (ParseDiskstatsLine(line, &parsed) == MONITOR_OK && parsed.valid) {
            stats[count].readsCompleted = parsed.readsCompleted;
            stats[count].readsMerged = parsed.readsMerged;
            stats[count].sectorsRead = parsed.sectorsRead;
            stats[count].readTimeMs = parsed.readTimeMs;
            stats[count].writesCompleted = parsed.writesCompleted;
            stats[count].writesMerged = parsed.writesMerged;
            stats[count].sectorsWritten = parsed.sectorsWritten;
            stats[count].writeTimeMs = parsed.writeTimeMs;
            stats[count].ioInProgress = parsed.ioInProgress;
            stats[count].ioTimeMs = parsed.ioTimeMs;
            stats[count].weightedIoTimeMs = parsed.weightedIoTimeMs;
            
            int ret = strncpy_s(stats[count].deviceName, sizeof(stats[count].deviceName),
                parsed.deviceName, strlen(parsed.deviceName));
            if (ret == EOK) {
                count++;
            }
        }
    }
    fclose(fp);

    uint32_t copyCount = (count < MAX_DISK_STATS_COUNT) ? count : MAX_DISK_STATS_COUNT;
    int ret = memcpy_s(g_monitorCtx.diskStats, sizeof(g_monitorCtx.diskStats),
        stats, sizeof(DiskStats) * copyCount);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    return MONITOR_OK;
}

/* ========== 网络统计 ========== */

static int ParseNetDevLine(char *line, NetworkStats *stats)
{
    INIT_CHECK_RETURN_VALUE(line != NULL && stats != NULL, MONITOR_INVALID_PARAM);

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
    uint64_t rxPackets = 0;
    uint64_t rxErrors = 0;
    uint64_t rxDropped = 0;
    uint64_t rxFifo = 0;
    uint64_t rxFrame = 0;
    uint64_t rxCompressed = 0;
    uint64_t rxMulticast = 0;
    uint64_t txBytes = 0;
    uint64_t txPackets = 0;
    uint64_t txErrors = 0;
    uint64_t txDropped = 0;
    uint64_t txFifo = 0;
    uint64_t txCollisions = 0;
    uint64_t txCarrier = 0;
    uint64_t txCompressed = 0;

    int fields = sscanf_s(colon + 1,
        "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &rxBytes, &rxPackets, &rxErrors, &rxDropped, &rxFifo, &rxFrame,
        &rxCompressed, &rxMulticast, &txBytes, &txPackets, &txErrors,
        &txDropped, &txFifo, &txCollisions, &txCarrier, &txCompressed);
    INIT_CHECK_RETURN_VALUE(fields >= NETDEV_FIELDS, MONITOR_ERROR);

    ret = strncpy_s(stats->interface, sizeof(stats->interface),
        interface, strlen(interface));
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    stats->rxBytes = rxBytes;
    stats->rxPackets = rxPackets;
    stats->rxErrors = rxErrors;
    stats->rxDropped = rxDropped;
    stats->rxFifo = rxFifo;
    stats->rxFrame = rxFrame;
    stats->rxCompressed = rxCompressed;
    stats->rxMulticast = rxMulticast;
    stats->txBytes = txBytes;
    stats->txPackets = txPackets;
    stats->txErrors = txErrors;
    stats->txDropped = txDropped;
    stats->txFifo = txFifo;
    stats->txCollisions = txCollisions;
    stats->txCarrier = txCarrier;
    stats->txCompressed = txCompressed;
    return MONITOR_OK;
}

int UpdateNetworkStats(NetworkStats *stats, uint32_t maxCount)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL && maxCount > 0, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(PROC_NET_DEV_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    /* 跳过头部两行 */
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

/* ========== 获取函数 ========== */

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

/* ========== 告警功能 ========== */

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

    INIT_LOGI("Added monitor alarm: type=%d level=%d message=%s", type, level, message);
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

/* ========== 性能记录 ========== */

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

/* ========== 打印摘要 ========== */

void PrintMonitorSummary(void)
{
    INIT_LOGI("=== System Monitor Summary ===");
    INIT_LOGI("State: %d", g_monitorCtx.state);
    INIT_LOGI("Alarm Count: %u", g_monitorCtx.alarmCount);
    INIT_LOGI("Perf Record Count: %u", g_monitorCtx.recordCount);
    INIT_LOGI("CPU Usage: %u.%02u%%",
        (uint32_t)(g_monitorCtx.cpuStats.totalUsage / PERCENT_MULTIPLIER),
        (uint32_t)(g_monitorCtx.cpuStats.totalUsage % PERCENT_MULTIPLIER));
    INIT_LOGI("CPU Cores: %u", g_monitorCtx.cpuStats.cpuCoreNum);
    INIT_LOGI("Memory Usage: %u.%02u%%",
        g_monitorCtx.memStats.usagePercent / PERCENT_MULTIPLIER,
        g_monitorCtx.memStats.usagePercent % PERCENT_MULTIPLIER);
    INIT_LOGI("Total Memory: %llu MB",
        (unsigned long long)(g_monitorCtx.memStats.totalMem / BYTES_PER_MB));
    INIT_LOGI("Free Memory: %llu MB",
        (unsigned long long)(g_monitorCtx.memStats.freeMem / BYTES_PER_MB));
    INIT_LOGI("Available Memory: %llu MB",
        (unsigned long long)(g_monitorCtx.memStats.availableMem / BYTES_PER_MB));
    INIT_LOGI("Context Switches: %u", g_monitorCtx.cpuStats.contextSwitches);
    INIT_LOGI("Processes Running: %u", g_monitorCtx.cpuStats.processesRunning);
    INIT_LOGI("Processes Blocked: %u", g_monitorCtx.cpuStats.processesBlocked);
}

/* ========== 导出数据 ========== */

static void ExportCpuStats(FILE *fp, const CpuStats *stats)
{
    fprintf(fp, "[CPU Statistics]\n");
    fprintf(fp, "  CPU Usage: %u.%02u%%\n",
        (uint32_t)(stats->totalUsage / PERCENT_MULTIPLIER),
        (uint32_t)(stats->totalUsage % PERCENT_MULTIPLIER));
    fprintf(fp, "  CPU Cores: %u\n", stats->cpuCoreNum);
    fprintf(fp, "  User Mode Time: %llu ms\n", (unsigned long long)stats->userModeTime);
    fprintf(fp, "  System Mode Time: %llu ms\n", (unsigned long long)stats->systemTime);
    fprintf(fp, "  Idle Time: %llu ms\n", (unsigned long long)stats->idleTime);
    fprintf(fp, "  IO Wait Time: %llu ms\n", (unsigned long long)stats->ioWaitTime);
    fprintf(fp, "  Context Switches: %u\n", stats->contextSwitches);
    fprintf(fp, "  Processes Created: %u\n", stats->processesCreated);
    fprintf(fp, "  Processes Running: %u\n", stats->processesRunning);
    fprintf(fp, "  Processes Blocked: %u\n\n", stats->processesBlocked);
}

static void ExportMemStats(FILE *fp, const MemoryStats *stats)
{
    fprintf(fp, "[Memory Statistics]\n");
    fprintf(fp, "  Memory Usage: %u.%02u%%\n",
        stats->usagePercent / PERCENT_MULTIPLIER,
        stats->usagePercent % PERCENT_MULTIPLIER);
    fprintf(fp, "  Total Memory: %llu MB\n",
        (unsigned long long)(stats->totalMem / BYTES_PER_MB));
    fprintf(fp, "  Free Memory: %llu MB\n",
        (unsigned long long)(stats->freeMem / BYTES_PER_MB));
    fprintf(fp, "  Available Memory: %llu MB\n",
        (unsigned long long)(stats->availableMem / BYTES_PER_MB));
    fprintf(fp, "  Buffers: %llu MB\n",
        (unsigned long long)(stats->buffers / BYTES_PER_MB));
    fprintf(fp, "  Cached: %llu MB\n",
        (unsigned long long)(stats->cached / BYTES_PER_MB));
    fprintf(fp, "  Swap Total: %llu MB\n",
        (unsigned long long)(stats->swapTotal / BYTES_PER_MB));
    fprintf(fp, "  Swap Free: %llu MB\n",
        (unsigned long long)(stats->swapFree / BYTES_PER_MB));
    fprintf(fp, "  Swap Usage: %u.%02u%%\n\n",
        stats->swapUsagePercent / PERCENT_MULTIPLIER,
        stats->swapUsagePercent % PERCENT_MULTIPLIER);
}

static void ExportAlarms(FILE *fp, const ListNode *alarmList)
{
    fprintf(fp, "[Alarms] (Total: %u)\n", g_monitorCtx.alarmCount);
    const ListNode *node = alarmList->next;
    while (node != alarmList) {
        const MonitorAlarm *alarm = (const MonitorAlarm *)node;
        fprintf(fp, "  [%s] Type:%d Level:%d Threshold:%u Actual:%u Message:%s\n",
            alarm->handled ? "Handled" : "Active",
            alarm->type, alarm->level,
            alarm->threshold, alarm->actualValue, alarm->message);
        node = node->next;
    }
}

static void ExportPerfRecords(FILE *fp, const ListNode *recordList)
{
    fprintf(fp, "\n[Performance Records] (Total: %u)\n", g_monitorCtx.recordCount);
    const ListNode *node = recordList->next;
    while (node != recordList) {
        const PerfRecord *record = (const PerfRecord *)node;
        fprintf(fp, "  Name:%s Duration:%llu ns\n",
            record->name, (unsigned long long)record->durationNs);
        node = node->next;
    }
}

int ExportMonitorData(const char *filePath)
{
    INIT_CHECK_RETURN_VALUE(filePath != NULL, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(filePath, "w");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    fprintf(fp, "=== System Monitor Report ===\n\n");
    ExportCpuStats(fp, &g_monitorCtx.cpuStats);
    ExportMemStats(fp, &g_monitorCtx.memStats);
    ExportAlarms(fp, &g_monitorCtx.alarmList);
    ExportPerfRecords(fp, &g_monitorCtx.perfRecordList);

    fclose(fp);
    INIT_LOGI("Monitor data exported to: %s", filePath);
    return MONITOR_OK;
}

/* ========== 计算函数 ========== */

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

/* ========== 查找函数 ========== */

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

/* ========== 回调注册 ========== */

int RegisterMonitorCallback(MonitorType type, MonitorCallback callback, void *context)
{
    INIT_CHECK_RETURN_VALUE(type < MONITOR_TYPE_MAX && callback != NULL, MONITOR_INVALID_PARAM);

    pthread_mutex_lock(&g_monitorMutex);
    g_callbacks[type].callback = callback;
    g_callbacks[type].context = context;
    g_callbacks[type].registered = true;
    pthread_mutex_unlock(&g_monitorMutex);

    return MONITOR_OK;
}

int UnregisterMonitorCallback(MonitorType type)
{
    INIT_CHECK_RETURN_VALUE(type < MONITOR_TYPE_MAX, MONITOR_INVALID_PARAM);

    pthread_mutex_lock(&g_monitorMutex);
    g_callbacks[type].callback = NULL;
    g_callbacks[type].context = NULL;
    g_callbacks[type].registered = false;
    pthread_mutex_unlock(&g_monitorMutex);

    return MONITOR_OK;
}

/* ========== 阈值检查 ========== */

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

/* ========== 历史统计 ========== */

int GetHistoryStats(MonitorType type, void *stats, uint32_t index)
{
    INIT_CHECK_RETURN_VALUE(type < MONITOR_TYPE_MAX, MONITOR_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(stats != NULL, MONITOR_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(index < g_historyCounts[type], MONITOR_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(g_historyRecords[type] != NULL, MONITOR_INVALID_PARAM);

    HistoryRecord *record = &g_historyRecords[type][index];
    int ret = memcpy_s(stats, record->dataSize, record->data, record->dataSize);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    return MONITOR_OK;
}

/* ========== CPU统计读取 ========== */

static void ParseStatLine(const char *line, CpuStats *stats)
{
    if (line == NULL || stats == NULL) {
        return;
    }

    if (strncmp(line, CTXT_LINE_PREFIX, strlen(CTXT_LINE_PREFIX)) == 0) {
        int parsed = sscanf_s(line, "ctxt %u", &stats->contextSwitches);
        INIT_CHECK_ONLY_ELOG(parsed == 1, "Failed to parse ctxt line");
    } else if (strncmp(line, PROCESSES_LINE_PREFIX, strlen(PROCESSES_LINE_PREFIX)) == 0) {
        int parsed = sscanf_s(line, "processes %u", &stats->processesCreated);
        INIT_CHECK_ONLY_ELOG(parsed == 1, "Failed to parse processes line");
    } else if (strncmp(line, PROCS_RUNNING_PREFIX, strlen(PROCS_RUNNING_PREFIX)) == 0) {
        int parsed = sscanf_s(line, "procs_running %u", &stats->processesRunning);
        INIT_CHECK_ONLY_ELOG(parsed == 1, "Failed to parse procs_running line");
    } else if (strncmp(line, PROCS_BLOCKED_PREFIX, strlen(PROCS_BLOCKED_PREFIX)) == 0) {
        int parsed = sscanf_s(line, "procs_blocked %u", &stats->processesBlocked);
        INIT_CHECK_ONLY_ELOG(parsed == 1, "Failed to parse procs_blocked line");
    }
}

static int ReadCpuStats(CpuStats *stats)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(PROC_STAT_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    INIT_ERROR_CHECK(fgets(line, sizeof(line), fp) != NULL, fclose(fp);
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
        cpuLabel, (unsigned int)sizeof(cpuLabel),
        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guestNice);
    fclose(fp);
    INIT_CHECK_RETURN_VALUE(parsed >= CPU_STAT_MIN_FIELDS, MONITOR_ERROR);

    stats->userModeTime = user;
    stats->niceTime = nice;
    stats->systemTime = system;
    stats->idleTime = idle;
    stats->ioWaitTime = (parsed >= 6) ? iowait : 0;
    stats->irqTime = (parsed >= 7) ? irq : 0;
    stats->softIrqTime = (parsed >= 8) ? softirq : 0;
    stats->stealTime = (parsed >= 9) ? steal : 0;
    stats->guestTime = (parsed >= 10) ? guest : 0;
    stats->guestNiceTime = (parsed >= CPU_STAT_MAX_FIELDS) ? guestNice : 0;
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

/* ========== 内存统计读取 ========== */

static void ParseMemInfoLine(const char *line, MemoryStats *stats)
{
    if (line == NULL || stats == NULL) {
        return;
    }

    char key[MEM_KEY_LEN] = {0};
    uint64_t value = 0;
    char unit[MEM_UNIT_LEN] = {0};

    if (sscanf_s(line, "%63s %llu %15s", key, (unsigned int)sizeof(key), 
        &value, unit, (unsigned int)sizeof(unit)) < 2) {
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
        {"SwapCached:", &stats->swapCached},
        {"Active:", &stats->activeMem},
        {"Inactive:", &stats->inactiveMem},
        {"SwapTotal:", &stats->swapTotal},
        {"SwapFree:", &stats->swapFree},
        {"Dirty:", &stats->dirtyPages},
        {"Writeback:", &stats->writeback},
        {"AnonPages:", &stats->anonPages},
        {"Mapped:", &stats->mapped},
        {"Shmem:", &stats->shmem},
        {"Slab:", &stats->slab},
        {"SReclaimable:", &stats->slabReclaimable},
        {"SUnreclaim:", &stats->slabUnreclaimable},
        {"KernelStack:", &stats->kernelStack},
        {"PageTables:", &stats->pageTables},
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
    if (stats == NULL || stats->totalMem == 0) {
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

/* ========== 进程信息读取 ========== */

static int ParseProcessStat(const char *line, ProcessInfo *info)
{
    INIT_CHECK_RETURN_VALUE(line != NULL && info != NULL, MONITOR_ERROR);

    char *start = strchr(line, '(');
    char *end = strrchr(line, ')');
    INIT_CHECK_RETURN_VALUE(start != NULL && end != NULL, MONITOR_ERROR);

    int nameLen = (int)(end - start - 1);
    if (nameLen >= (int)sizeof(info->name)) {
        nameLen = (int)sizeof(info->name) - 1;
    }
    int ret = strncpy_s(info->name, sizeof(info->name), start + 1, (size_t)nameLen);
    INIT_CHECK_RETURN_VALUE(ret == EOK, MONITOR_ERROR);

    char *next = end + 2;
    info->state = *next++;

    char *savePtr = NULL;
    char *token = strtok_r(next, " ", &savePtr);
    int fieldIndex = 0;
    while (token != NULL && fieldIndex < STAT_FIELD_MAX_COUNT) {
        switch (fieldIndex) {
            case PROC_STAT_FIELD_PPID:
                info->ppid = atoi(token);
                break;
            case PROC_STAT_FIELD_UTIME:
                info->utime = strtoull(token, NULL, DECIMAL_BASE);
                break;
            case PROC_STAT_FIELD_STIME:
                info->stime = strtoull(token, NULL, DECIMAL_BASE);
                break;
            case PROC_STAT_FIELD_PRIORITY:
                info->priority = atol(token);
                break;
            case PROC_STAT_FIELD_NICE:
                info->nice = atol(token);
                break;
            case PROC_STAT_FIELD_NUM_THREADS:
                info->numThreads = atol(token);
                break;
            case PROC_STAT_FIELD_STARTTIME:
                info->startTime = strtoull(token, NULL, DECIMAL_BASE);
                break;
            case PROC_STAT_FIELD_VSIZE:
                info->vsize = atol(token);
                break;
            case PROC_STAT_FIELD_RSS:
                info->rss = atol(token) * (long)sysconf(_SC_PAGESIZE);
                break;
            default:
                break;
        }
        token = strtok_r(NULL, " ", &savePtr);
        fieldIndex++;
    }
    return MONITOR_OK;
}

static int ReadProcessCmdline(int pid, char *buf, size_t bufLen)
{
    char path[PROC_PATH_LEN] = {0};
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/cmdline", pid);
    INIT_CHECK_RETURN_VALUE(ret >= 0, MONITOR_ERROR);

    FILE *fp = fopen(path, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_OK);

    size_t len = fread(buf, 1, bufLen - 1, fp);
    fclose(fp);
    INIT_CHECK_RETURN_VALUE(len > 0, MONITOR_OK);

    buf[len] = '\0';
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\0') {
            buf[i] = ' ';
        }
    }
    return MONITOR_OK;
}

static int ReadProcessInfo(ProcessInfo *info, int pid)
{
    INIT_CHECK_RETURN_VALUE(info != NULL && pid > 0, MONITOR_INVALID_PARAM);

    char path[PROC_PATH_LEN] = {0};
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/stat", pid);
    INIT_CHECK_RETURN_VALUE(ret >= 0, MONITOR_ERROR);

    FILE *fp = fopen(path, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    char line[MAX_LINE_LENGTH] = {0};
    INIT_ERROR_CHECK(fgets(line, sizeof(line), fp) != NULL, fclose(fp);
        return MONITOR_ERROR, "Failed to read %s", path);
    fclose(fp);

    ret = ParseProcessStat(line, info);
    INIT_CHECK_RETURN_VALUE(ret == MONITOR_OK, MONITOR_ERROR);

    info->pid = pid;
    return ReadProcessCmdline(pid, info->cmdline, sizeof(info->cmdline));
}

/* ========== 列表清理 ========== */

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

/* ========== 更新所有统计 ========== */

static int UpdateAllStats(void)
{
    int ret = MONITOR_OK;

    if (g_monitorCtx.config.enableCpuMonitor) {
        ret = ReadCpuStats(&g_monitorCtx.cpuStats);
        INIT_CHECK_ONLY_ELOG(ret == MONITOR_OK, "Failed to read CPU stats");
    }

    if (g_monitorCtx.config.enableMemMonitor) {
        ret = ReadMemoryStats(&g_monitorCtx.memStats);
        INIT_CHECK_ONLY_ELOG(ret == MONITOR_OK, "Failed to read memory stats");
    }

    if (g_monitorCtx.config.enableProcMonitor) {
        ret = UpdateProcessStats();
        INIT_CHECK_ONLY_ELOG(ret == MONITOR_OK, "Failed to update process stats");
    }

    if (g_monitorCtx.config.enableDiskMonitor) {
        UpdateDiskStats(g_monitorCtx.diskStats, MAX_DISK_STATS_COUNT);
    }

    if (g_monitorCtx.config.enableNetMonitor) {
        UpdateNetworkStats(g_monitorCtx.netStats, MAX_NET_STATS_COUNT);
    }

    return ret;
}

/* ========== 系统诊断 ========== */

static void DiagnoseCpu(FILE *fp, uint32_t cpuUsage)
{
    if (cpuUsage > CPU_CRITICAL_THRESHOLD) {
        fprintf(fp, "  WARNING: CPU usage is very high (%u.%02u%%)\n",
            cpuUsage / PERCENT_MULTIPLIER, cpuUsage % PERCENT_MULTIPLIER);
        fprintf(fp, "  Recommendation: Check for runaway processes\n");
    } else if (cpuUsage > CPU_WARNING_THRESHOLD) {
        fprintf(fp, "  CAUTION: CPU usage is elevated (%u.%02u%%)\n",
            cpuUsage / PERCENT_MULTIPLIER, cpuUsage % PERCENT_MULTIPLIER);
    } else {
        fprintf(fp, "  OK: CPU usage is normal (%u.%02u%%)\n",
            cpuUsage / PERCENT_MULTIPLIER, cpuUsage % PERCENT_MULTIPLIER);
    }
}

static void DiagnoseMemory(FILE *fp, uint32_t memUsage)
{
    if (memUsage > MEM_CRITICAL_THRESHOLD) {
        fprintf(fp, "  CRITICAL: Memory usage is very high (%u.%02u%%)\n",
            memUsage / PERCENT_MULTIPLIER, memUsage % PERCENT_MULTIPLIER);
        fprintf(fp, "  Recommendation: Free memory or add more RAM\n");
    } else if (memUsage > MEM_WARNING_THRESHOLD) {
        fprintf(fp, "  WARNING: Memory usage is elevated (%u.%02u%%)\n",
            memUsage / PERCENT_MULTIPLIER, memUsage % PERCENT_MULTIPLIER);
        fprintf(fp, "  Recommendation: Consider clearing caches\n");
    } else {
        fprintf(fp, "  OK: Memory usage is normal (%u.%02u%%)\n",
            memUsage / PERCENT_MULTIPLIER, memUsage % PERCENT_MULTIPLIER);
    }

    if (g_monitorCtx.memStats.swapUsagePercent > SWAP_USAGE_INFO_THRESHOLD) {
        fprintf(fp, "  INFO: Swap usage: %u.%02u%%\n",
            g_monitorCtx.memStats.swapUsagePercent / PERCENT_MULTIPLIER,
            g_monitorCtx.memStats.swapUsagePercent % PERCENT_MULTIPLIER);
        fprintf(fp, "  Note: High swap usage may affect performance\n");
    }
}

static void DiagnoseProcesses(FILE *fp)
{
    ProcessInfo *topCpu = FindTopCpuProcess();
    if (topCpu != NULL) {
        fprintf(fp, "  Top CPU consumer: %s (PID: %d, CPU: %llu%%)\n",
            topCpu->name, topCpu->pid, (unsigned long long)topCpu->cpuUsage);
    }

    ProcessInfo *topMem = FindTopMemProcess();
    if (topMem != NULL) {
        fprintf(fp, "  Top Memory consumer: %s (PID: %d, Mem: %llu%%)\n",
            topMem->name, topMem->pid, (unsigned long long)topMem->memUsage);
    }

    fprintf(fp, "\n  Total processes monitored: %u\n", g_monitorCtx.recordCount);
    fprintf(fp, "  Context switches: %u\n", g_monitorCtx.cpuStats.contextSwitches);
    fprintf(fp, "  Processes blocked: %u\n", g_monitorCtx.cpuStats.processesBlocked);
}

static const char *GetAlarmLevelString(AlarmLevel level)
{
    if (level >= ALARM_LEVEL_CRITICAL) {
        return "CRITICAL";
    }
    if (level >= ALARM_LEVEL_WARNING) {
        return "WARNING";
    }
    return "INFO";
}

static void DiagnoseAlarms(FILE *fp)
{
    fprintf(fp, "\n[Active Alarms] (%u total)\n", g_monitorCtx.alarmCount);
    ListNode *node = g_monitorCtx.alarmList.next;
    while (node != &g_monitorCtx.alarmList) {
        MonitorAlarm *alarm = (MonitorAlarm *)node;
        if (!alarm->handled) {
            const char *levelStr = GetAlarmLevelString(alarm->level);
            fprintf(fp, "  [%s] %s (Threshold: %u, Actual: %u)\n",
                levelStr, alarm->message, alarm->threshold, alarm->actualValue);
        }
        node = node->next;
    }
}

static void PrintRecommendations(FILE *fp, uint32_t cpuUsage, uint32_t memUsage)
{
    (void)cpuUsage;
    (void)memUsage;
    
    fprintf(fp, "\n[Recommendations]\n");
    if (g_monitorCtx.cpuStats.totalUsage > CPU_WARNING_THRESHOLD) {
        fprintf(fp, "  - Investigate high CPU usage processes\n");
    }
    if (g_monitorCtx.memStats.usagePercent > MEM_WARNING_THRESHOLD) {
        fprintf(fp, "  - Consider increasing available memory\n");
    }
    if (g_monitorCtx.cpuStats.processesBlocked > BLOCKED_PROC_THRESHOLD) {
        fprintf(fp, "  - Check for I/O bottlenecks\n");
    }
}

int RunSystemDiagnosis(const char *outputPath)
{
    INIT_CHECK_RETURN_VALUE(outputPath != NULL, MONITOR_INVALID_PARAM);

    FILE *fp = fopen(outputPath, "w");
    INIT_CHECK_RETURN_VALUE(fp != NULL, MONITOR_ERROR);

    fprintf(fp, "=== System Diagnosis Report ===\n\n");
    fprintf(fp, "Generated at: %ld\n\n", (long)time(NULL));

    uint32_t cpuUsage = g_monitorCtx.cpuStats.totalUsage;
    uint32_t memUsage = g_monitorCtx.memStats.usagePercent;

    fprintf(fp, "[CPU Diagnosis]\n");
    DiagnoseCpu(fp, cpuUsage);

    fprintf(fp, "\n[Memory Diagnosis]\n");
    DiagnoseMemory(fp, memUsage);

    fprintf(fp, "\n[Process Diagnosis]\n");
    DiagnoseProcesses(fp);

    DiagnoseAlarms(fp);

    PrintRecommendations(fp, cpuUsage, memUsage);

    fclose(fp);
    INIT_LOGI("System diagnosis completed: %s", outputPath);
    return MONITOR_OK;
}