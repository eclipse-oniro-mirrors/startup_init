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
#include <errno.h>
#include <time.h>

#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

#define RESOURCE_OK             0
#define RESOURCE_ERROR          (-1)
#define RESOURCE_INVALID_PARAM  (-2)

#define MAX_BUFFER_SIZE         4096
#define MAX_DISPLAY_CPU_CORES   8
#define MAX_DISPLAY_HISTORY     10
#define SAMPLE_INTERVAL_SEC     5
#define SECONDS_PER_DAY         86400
#define TREND_THRESHOLD         100
#define TREND_SAMPLE_COUNT      3
#define LOADAVG_FIELD_COUNT     3
#define UPTIME_FIELD_COUNT      2
#define AVG_CALC_DURATION_SEC   60

#define VFS_TYPE_PROC           "proc"
#define VFS_TYPE_SYSFS          "sysfs"
#define VFS_TYPE_DEVFS          "devfs"
#define VFS_TYPE_TMPFS          "tmpfs"
#define VFS_TYPE_CGROUP         "cgroup"

typedef enum {
    TREND_STABLE = 0,
    TREND_INCREASING,
    TREND_DECREASING,
    TREND_UNKNOWN
} MemoryTrend;

typedef struct {
    uint64_t userModeTime;
    uint64_t niceTime;
    uint64_t systemTime;
    uint64_t idleTime;
    uint64_t ioWaitTime;
    uint64_t irqTime;
    uint64_t softIrqTime;
    uint64_t stealTime;
    uint64_t guestTime;
    uint64_t guestNiceTime;
    uint32_t usagePercent;
} CpuCoreStats;

typedef struct {
    uint64_t timestamp;
    CpuStats cpuStats;
    MemoryStats memStats;
    uint32_t cpuHistory[MAX_CPU_CORES];
} ResourceHistory;

typedef struct {
    char mountPoint[MOUNT_POINT_MAX_LEN];
    char device[MOUNT_POINT_MAX_LEN];
    char fsType[FS_TYPE_MAX_LEN];
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t freeBytes;
    uint32_t usagePercent;
    uint32_t inodeUsagePercent;
} FileSystemStats;

static CpuCoreStats g_cpuCoreStats[MAX_CPU_CORES];
static ResourceHistory g_history[STAT_HISTORY_SIZE];
static uint32_t g_historyIndex = 0;
static uint32_t g_historyCount = 0;
static uint32_t g_cpuCoreCount = 0;

static int ParseCpuCoreStats(const char *line, CpuCoreStats *stats);
static int ReadLoadAverage(double *avg1, double *avg5, double *avg15);
static int ReadUptime(double *uptime, double *idleTime);
static uint64_t GetTimestampMs(void);
static uint32_t CalcHistoryIndex(uint32_t index);
static bool IsVirtualFileSystem(const char *device);
static void FillFsStats(FileSystemStats *stats, const char *device,
    const char *mountPoint, const char *fsType);

static uint64_t GetTimestampMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * (uint64_t)NSEC_PER_MSEC + (uint64_t)ts.tv_nsec / (uint64_t)NSEC_PER_USEC;
}

static uint32_t CalcHistoryIndex(uint32_t index)
{
    return (g_historyIndex + STAT_HISTORY_SIZE - index - 1) % STAT_HISTORY_SIZE;
}

int InitResourceStats(void)
{
    g_cpuCoreCount = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
    if (g_cpuCoreCount > MAX_CPU_CORES) {
        g_cpuCoreCount = MAX_CPU_CORES;
    }

    int ret = memset_s(g_history, sizeof(g_history), 0, sizeof(g_history));
    INIT_CHECK_RETURN_VALUE(ret == EOK, RESOURCE_ERROR);

    g_historyIndex = 0;
    g_historyCount = 0;

    INIT_LOGI("Resource stats initialized, CPU cores: %u", g_cpuCoreCount);
    return RESOURCE_OK;
}

static int ParseCpuCoreStats(const char *line, CpuCoreStats *stats)
{
    INIT_CHECK_RETURN_VALUE(line != NULL && stats != NULL, RESOURCE_INVALID_PARAM);

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
    INIT_CHECK_RETURN_VALUE(parsed >= 5, RESOURCE_ERROR);

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
    return RESOURCE_OK;
}

int GetCpuCoreStats(CpuCoreStats *stats, uint32_t *count)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL && count != NULL, RESOURCE_INVALID_PARAM);

    FILE *fp = fopen(PROC_STAT_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, RESOURCE_ERROR);

    char line[MAX_BUFFER_SIZE] = {0};
    uint32_t coreCount = 0;
    size_t prefixLen = strlen(CPU_LABEL_PREFIX);

    while (fgets(line, sizeof(line), fp) != NULL && coreCount < MAX_CPU_CORES) {
        if (strncmp(line, CPU_LABEL_PREFIX, prefixLen) != 0) {
            continue;
        }
        char digitChar = line[prefixLen];
        if (digitChar < '0' || digitChar > '9') {
            continue;
        }

        if (ParseCpuCoreStats(line, &g_cpuCoreStats[coreCount]) != RESOURCE_OK) {
            continue;
        }

        int ret = memcpy_s(&stats[coreCount], sizeof(CpuCoreStats),
            &g_cpuCoreStats[coreCount], sizeof(CpuCoreStats));
        if (ret != EOK) {
            continue;
        }
        coreCount++;
    }

    fclose(fp);
    *count = coreCount;
    return RESOURCE_OK;
}

int CalculateCpuCoreUsage(CpuCoreStats *prev, CpuCoreStats *curr, uint32_t *usage)
{
    INIT_CHECK_RETURN_VALUE(prev != NULL && curr != NULL && usage != NULL, RESOURCE_INVALID_PARAM);

    uint64_t prevIdle = prev->idleTime + prev->ioWaitTime;
    uint64_t currIdle = curr->idleTime + curr->ioWaitTime;

    uint64_t prevTotal = prev->userModeTime + prev->niceTime + prev->systemTime +
                         prev->idleTime + prev->ioWaitTime + prev->irqTime +
                         prev->softIrqTime + prev->stealTime;

    uint64_t currTotal = curr->userModeTime + curr->niceTime + curr->systemTime +
                         curr->idleTime + curr->ioWaitTime + curr->irqTime +
                         curr->softIrqTime + curr->stealTime;

    uint64_t totalDiff = currTotal - prevTotal;
    uint64_t idleDiff = currIdle - prevIdle;
    if (totalDiff == 0) {
        *usage = 0;
        return RESOURCE_OK;
    }

    *usage = (uint32_t)(((totalDiff - idleDiff) * PERCENT_SCALE) / totalDiff);
    return RESOURCE_OK;
}

static int ReadLoadAverage(double *avg1, double *avg5, double *avg15)
{
    INIT_CHECK_RETURN_VALUE(avg1 != NULL && avg5 != NULL && avg15 != NULL, RESOURCE_INVALID_PARAM);

    FILE *fp = fopen(PROC_LOADAVG_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, RESOURCE_ERROR);

    char line[MAX_BUFFER_SIZE] = {0};
    INIT_ERROR_CHECK(fgets(line, sizeof(line), fp) != NULL, fclose(fp);
        return RESOURCE_ERROR, "Failed to read /proc/loadavg");
    fclose(fp);

    int parsed = sscanf_s(line, "%lf %lf %lf", avg1, avg5, avg15);
    INIT_CHECK_RETURN_VALUE(parsed == LOADAVG_FIELD_COUNT, RESOURCE_ERROR);

    return RESOURCE_OK;
}

int GetSystemLoadAverage(double *avg1, double *avg5, double *avg15)
{
    return ReadLoadAverage(avg1, avg5, avg15);
}

static int ReadUptime(double *uptime, double *idleTime)
{
    INIT_CHECK_RETURN_VALUE(uptime != NULL && idleTime != NULL, RESOURCE_INVALID_PARAM);

    FILE *fp = fopen(PROC_UPTIME_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, RESOURCE_ERROR);

    char line[MAX_BUFFER_SIZE] = {0};
    INIT_ERROR_CHECK(fgets(line, sizeof(line), fp) != NULL, fclose(fp);
        return RESOURCE_ERROR, "Failed to read /proc/uptime");
    fclose(fp);

    int parsed = sscanf_s(line, "%lf %lf", uptime, idleTime);
    INIT_CHECK_RETURN_VALUE(parsed == UPTIME_FIELD_COUNT, RESOURCE_ERROR);

    return RESOURCE_OK;
}

int GetSystemUptime(double *uptime, double *idleTime)
{
    return ReadUptime(uptime, idleTime);
}

int RecordHistoryStats(const CpuStats *cpuStats, const MemoryStats *memStats)
{
    INIT_CHECK_RETURN_VALUE(cpuStats != NULL && memStats != NULL, RESOURCE_INVALID_PARAM);

    ResourceHistory *history = &g_history[g_historyIndex];
    history->timestamp = GetTimestampMs();

    int ret = memcpy_s(&history->cpuStats, sizeof(CpuStats), cpuStats, sizeof(CpuStats));
    INIT_CHECK_RETURN_VALUE(ret == EOK, RESOURCE_ERROR);

    ret = memcpy_s(&history->memStats, sizeof(MemoryStats), memStats, sizeof(MemoryStats));
    INIT_CHECK_RETURN_VALUE(ret == EOK, RESOURCE_ERROR);

    for (uint32_t i = 0; i < g_cpuCoreCount && i < MAX_CPU_CORES; i++) {
        history->cpuHistory[i] = g_cpuCoreStats[i].usagePercent;
    }

    g_historyIndex = (g_historyIndex + 1) % STAT_HISTORY_SIZE;
    if (g_historyCount < STAT_HISTORY_SIZE) {
        g_historyCount++;
    }

    return RESOURCE_OK;
}

int GetHistoryStatsByIndex(uint32_t index, CpuStats *cpuStats, MemoryStats *memStats)
{
    INIT_CHECK_RETURN_VALUE(cpuStats != NULL && memStats != NULL, RESOURCE_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(index < g_historyCount, RESOURCE_INVALID_PARAM);

    uint32_t actualIndex = CalcHistoryIndex(index);
    ResourceHistory *history = &g_history[actualIndex];

    int ret = memcpy_s(cpuStats, sizeof(CpuStats), &history->cpuStats, sizeof(CpuStats));
    INIT_CHECK_RETURN_VALUE(ret == EOK, RESOURCE_ERROR);

    ret = memcpy_s(memStats, sizeof(MemoryStats), &history->memStats, sizeof(MemoryStats));
    INIT_CHECK_RETURN_VALUE(ret == EOK, RESOURCE_ERROR);

    return RESOURCE_OK;
}

static uint32_t CalcSampleCount(uint32_t durationSec)
{
    uint32_t sampleCount = durationSec / SAMPLE_INTERVAL_SEC;
    if (sampleCount == 0) {
        sampleCount = 1;
    }
    if (sampleCount > g_historyCount) {
        sampleCount = g_historyCount;
    }
    return sampleCount;
}

int CalculateAvgCpuUsage(uint32_t durationSec, uint32_t *avgUsage)
{
    INIT_CHECK_RETURN_VALUE(avgUsage != NULL, RESOURCE_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(g_historyCount > 0, RESOURCE_INVALID_PARAM);

    uint64_t totalUsage = 0;
    uint32_t count = 0;
    uint32_t sampleCount = CalcSampleCount(durationSec);

    for (uint32_t i = 0; i < sampleCount; i++) {
        uint32_t actualIndex = CalcHistoryIndex(i);
        totalUsage += g_history[actualIndex].cpuStats.totalUsage;
        count++;
    }

    *avgUsage = (count > 0) ? (uint32_t)(totalUsage / count) : 0;
    return RESOURCE_OK;
}

int CalculateAvgMemUsage(uint32_t durationSec, uint32_t *avgUsage)
{
    INIT_CHECK_RETURN_VALUE(avgUsage != NULL, RESOURCE_INVALID_PARAM);
    INIT_CHECK_RETURN_VALUE(g_historyCount > 0, RESOURCE_INVALID_PARAM);

    uint64_t totalUsage = 0;
    uint32_t count = 0;
    uint32_t sampleCount = CalcSampleCount(durationSec);

    for (uint32_t i = 0; i < sampleCount; i++) {
        uint32_t actualIndex = CalcHistoryIndex(i);
        totalUsage += g_history[actualIndex].memStats.usagePercent;
        count++;
    }

    *avgUsage = (count > 0) ? (uint32_t)(totalUsage / count) : 0;
    return RESOURCE_OK;
}

int GetMemoryTrend(MemoryTrend *trend)
{
    INIT_CHECK_RETURN_VALUE(trend != NULL, RESOURCE_INVALID_PARAM);
    *trend = TREND_UNKNOWN;
    INIT_CHECK_RETURN_VALUE(g_historyCount >= TREND_SAMPLE_COUNT, RESOURCE_INVALID_PARAM);

    uint32_t recent[TREND_SAMPLE_COUNT] = {0};
    for (uint32_t i = 0; i < TREND_SAMPLE_COUNT; i++) {
        uint32_t actualIndex = CalcHistoryIndex(i);
        recent[i] = g_history[actualIndex].memStats.usagePercent;
    }

    int32_t diff1 = (int32_t)recent[0] - (int32_t)recent[1];
    int32_t diff2 = (int32_t)recent[1] - (int32_t)recent[2];

    if (diff1 > TREND_THRESHOLD && diff2 > TREND_THRESHOLD) {
        *trend = TREND_INCREASING;
    } else if (diff1 < -TREND_THRESHOLD && diff2 < -TREND_THRESHOLD) {
        *trend = TREND_DECREASING;
    } else {
        *trend = TREND_STABLE;
    }

    return RESOURCE_OK;
}

static bool IsVirtualFileSystem(const char *device)
{
    if (device == NULL) {
        return false;
    }
    if (strncmp(device, VFS_TYPE_PROC, sizeof(VFS_TYPE_PROC) - 1) == 0 ||
        strncmp(device, VFS_TYPE_SYSFS, sizeof(VFS_TYPE_SYSFS) - 1) == 0 ||
        strncmp(device, VFS_TYPE_DEVFS, sizeof(VFS_TYPE_DEVFS) - 1) == 0 ||
        strncmp(device, VFS_TYPE_TMPFS, sizeof(VFS_TYPE_TMPFS) - 1) == 0 ||
        strncmp(device, VFS_TYPE_CGROUP, sizeof(VFS_TYPE_CGROUP) - 1) == 0) {
        return true;
    }
    return false;
}

static void FillFsStats(FileSystemStats *stats, const char *device,
    const char *mountPoint, const char *fsType)
{
    stats->totalBytes = 0;
    stats->usedBytes = 0;
    stats->freeBytes = 0;
    stats->usagePercent = 0;
    stats->inodeUsagePercent = 0;

    int ret = strncpy_s(stats->device, sizeof(stats->device), device, strlen(device));
    INIT_CHECK_ONLY_RETURN(ret == EOK);

    ret = strncpy_s(stats->mountPoint, sizeof(stats->mountPoint), mountPoint, strlen(mountPoint));
    INIT_CHECK_ONLY_RETURN(ret == EOK);

    ret = strncpy_s(stats->fsType, sizeof(stats->fsType), fsType, strlen(fsType));
    INIT_CHECK_ONLY_RETURN(ret == EOK);
}

int GetFileSystemStats(FileSystemStats *stats, uint32_t maxCount, uint32_t *actualCount)
{
    INIT_CHECK_RETURN_VALUE(stats != NULL && actualCount != NULL, RESOURCE_INVALID_PARAM);

    FILE *fp = fopen(PROC_MOUNTS_PATH, "r");
    INIT_CHECK_RETURN_VALUE(fp != NULL, RESOURCE_ERROR);

    char line[MAX_BUFFER_SIZE] = {0};
    uint32_t count = 0;

    while (fgets(line, sizeof(line), fp) != NULL && count < maxCount) {
        char device[MOUNT_POINT_MAX_LEN] = {0};
        char mountPoint[MOUNT_POINT_MAX_LEN] = {0};
        char fsType[FS_TYPE_MAX_LEN] = {0};

        int parsed = sscanf_s(line, "%255s %255s %63s",
            device, sizeof(device),
            mountPoint, sizeof(mountPoint),
            fsType, sizeof(fsType));
        if (parsed < 3) {
            continue;
        }

        if (IsVirtualFileSystem(device)) {
            continue;
        }

        FillFsStats(&stats[count], device, mountPoint, fsType);
        count++;
    }

    fclose(fp);
    *actualCount = count;
    return RESOURCE_OK;
}

static void PrintLoadAverage(void)
{
    double avg1 = 0;
    double avg5 = 0;
    double avg15 = 0;
    if (GetSystemLoadAverage(&avg1, &avg5, &avg15) == RESOURCE_OK) {
        INIT_LOGI("Load Average: %.2f, %.2f, %.2f", avg1, avg5, avg15);
    }
}

static void PrintUptime(void)
{
    double uptime = 0;
    double idleTime = 0;
    if (GetSystemUptime(&uptime, &idleTime) == RESOURCE_OK) {
        INIT_LOGI("System Uptime: %.0f seconds", uptime);
        INIT_LOGI("Idle Time: %.0f seconds", idleTime);
    }
}

static void PrintCpuCoreUsage(void)
{
    CpuCoreStats coreStats[MAX_CPU_CORES];
    uint32_t coreCount = 0;
    if (GetCpuCoreStats(coreStats, &coreCount) != RESOURCE_OK) {
        return;
    }

    for (uint32_t i = 0; i < coreCount && i < MAX_DISPLAY_CPU_CORES; i++) {
        INIT_LOGI("CPU %u: %u.%02u%%", i,
            coreStats[i].usagePercent / PERCENT_MULTIPLIER,
            coreStats[i].usagePercent % PERCENT_MULTIPLIER);
    }
}

static void PrintHistoryStats(void)
{
    INIT_LOGI("History samples: %u", g_historyCount);
    if (g_historyCount == 0) {
        return;
    }

    uint32_t avgCpu = 0;
    uint32_t avgMem = 0;
    if (CalculateAvgCpuUsage(AVG_CALC_DURATION_SEC, &avgCpu) == RESOURCE_OK) {
        INIT_LOGI("Average CPU usage (%ds): %u.%02u%%",
            AVG_CALC_DURATION_SEC, avgCpu / PERCENT_MULTIPLIER, avgCpu % PERCENT_MULTIPLIER);
    }
    if (CalculateAvgMemUsage(AVG_CALC_DURATION_SEC, &avgMem) == RESOURCE_OK) {
        INIT_LOGI("Average Memory usage (%ds): %u.%02u%%",
            AVG_CALC_DURATION_SEC, avgMem / PERCENT_MULTIPLIER, avgMem % PERCENT_MULTIPLIER);
    }
}

void PrintResourceStatsReport(void)
{
    INIT_LOGI("=== Resource Statistics Report ===");
    PrintLoadAverage();
    PrintUptime();
    PrintCpuCoreUsage();
    PrintHistoryStats();
}

static void ExportLoadAverage(FILE *fp)
{
    double avg1 = 0;
    double avg5 = 0;
    double avg15 = 0;
    if (GetSystemLoadAverage(&avg1, &avg5, &avg15) != RESOURCE_OK) {
        return;
    }

    fprintf(fp, "[Load Average]\n");
    fprintf(fp, "  1 minute: %.2f\n", avg1);
    fprintf(fp, "  5 minutes: %.2f\n", avg5);
    fprintf(fp, "  15 minutes: %.2f\n\n", avg15);
}

static void ExportUptime(FILE *fp)
{
    double uptime = 0;
    double idleTime = 0;
    if (GetSystemUptime(&uptime, &idleTime) != RESOURCE_OK) {
        return;
    }

    fprintf(fp, "[Uptime]\n");
    fprintf(fp, "  System uptime: %.0f seconds (%.2f days)\n", uptime, uptime / SECONDS_PER_DAY);
    fprintf(fp, "  Idle time: %.0f seconds\n\n", idleTime);
}

static void ExportCpuCores(FILE *fp)
{
    CpuCoreStats coreStats[MAX_CPU_CORES];
    uint32_t coreCount = 0;
    if (GetCpuCoreStats(coreStats, &coreCount) != RESOURCE_OK) {
        return;
    }

    fprintf(fp, "[CPU Cores] (%u total)\n", coreCount);
    for (uint32_t i = 0; i < coreCount; i++) {
        fprintf(fp, "  CPU%u: %u.%02u%% (User:%llu System:%llu Idle:%llu)\n",
            i, coreStats[i].usagePercent / PERCENT_MULTIPLIER,
            coreStats[i].usagePercent % PERCENT_MULTIPLIER,
            (unsigned long long)coreStats[i].userModeTime,
            (unsigned long long)coreStats[i].systemTime,
            (unsigned long long)coreStats[i].idleTime);
    }
    fprintf(fp, "\n");
}

static void ExportHistory(FILE *fp)
{
    fprintf(fp, "[History] (%u samples)\n", g_historyCount);
    for (uint32_t i = 0; i < g_historyCount && i < MAX_DISPLAY_HISTORY; i++) {
        uint32_t actualIndex = CalcHistoryIndex(i);
        ResourceHistory *hist = &g_history[actualIndex];
        fprintf(fp, "  Sample %u: CPU=%u.%02u%% MEM=%u.%02u%% Time=%llu\n",
            i,
            (uint32_t)(hist->cpuStats.totalUsage / PERCENT_MULTIPLIER),
            (uint32_t)(hist->cpuStats.totalUsage % PERCENT_MULTIPLIER),
            hist->memStats.usagePercent / PERCENT_MULTIPLIER,
            hist->memStats.usagePercent % PERCENT_MULTIPLIER,
            (unsigned long long)hist->timestamp);
    }
}

int ExportResourceStats(const char *filePath)
{
    INIT_CHECK_RETURN_VALUE(filePath != NULL, RESOURCE_INVALID_PARAM);

    FILE *fp = fopen(filePath, "w");
    INIT_CHECK_RETURN_VALUE(fp != NULL, RESOURCE_ERROR);

    fprintf(fp, "=== Resource Statistics Export ===\n\n");
    ExportLoadAverage(fp);
    ExportUptime(fp);
    ExportCpuCores(fp);
    ExportHistory(fp);

    fclose(fp);
    INIT_LOGI("Resource stats exported to: %s", filePath);
    return RESOURCE_OK;
}