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

#define MAX_BUFFER_SIZE 4096
#define MAX_CPU_CORES 128
#define MAX_DISK_DEVICES 32
#define MAX_NET_INTERFACES 32
#define STAT_HISTORY_SIZE 60

// CPU核心统计
typedef struct {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t guest;
    uint64_t guest_nice;
    uint32_t usagePercent;
} CpuCoreStats;

// 资源统计历史记录
typedef struct {
    uint64_t timestamp;
    CpuStats cpuStats;
    MemoryStats memStats;
    uint32_t cpuHistory[MAX_CPU_CORES];
} ResourceHistory;

// 全局变量
static CpuCoreStats g_cpuCoreStats[MAX_CPU_CORES];
static ResourceHistory g_history[STAT_HISTORY_SIZE];
static uint32_t g_historyIndex = 0;
static uint32_t g_historyCount = 0;
static uint32_t g_cpuCoreCount = 0;

// 前向声明
static int ParseCpuCoreStats(const char *line, CpuCoreStats *stats);
static int ReadLoadAverage(double *avg1, double *avg5, double *avg15);
static int ReadUptime(double *uptime, double *idleTime);
static uint64_t GetTimestampMs(void);

// 初始化资源统计
int InitResourceStats(void)
{
    // 获取CPU核心数
    g_cpuCoreCount = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
    if (g_cpuCoreCount > MAX_CPU_CORES) {
        g_cpuCoreCount = MAX_CPU_CORES;
    }

    // 初始化历史记录
    (void)memset_s(g_history, sizeof(g_history), 0, sizeof(g_history));
    g_historyIndex = 0;
    g_historyCount = 0;

    INIT_LOGI("Resource stats initialized, CPU cores: %u", g_cpuCoreCount);
    return 0;
}

// 获取每个CPU核心的统计信息
int GetCpuCoreStats(CpuCoreStats *stats, uint32_t *count)
{
    if (stats == NULL || count == NULL) {
        return -1;
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/stat: %d", errno);
        return -1;
    }

    char line[MAX_BUFFER_SIZE];
    uint32_t coreCount = 0;

    while (fgets(line, sizeof(line), fp) != NULL && coreCount < MAX_CPU_CORES) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
            if (ParseCpuCoreStats(line, &g_cpuCoreStats[coreCount]) == 0) {
                if (memcpy_s(&stats[coreCount], sizeof(CpuCoreStats),
                    &g_cpuCoreStats[coreCount], sizeof(CpuCoreStats)) == EOK) {
                    coreCount++;
                }
            }
        }
    }

    fclose(fp);
    *count = coreCount;
    return 0;
}

// 计算CPU核心使用率
int CalculateCpuCoreUsage(CpuCoreStats *prev, CpuCoreStats *curr, uint32_t *usage)
{
    if (prev == NULL || curr == NULL || usage == NULL) {
        return -1;
    }

    uint64_t prevIdle = prev->idle + prev->iowait;
    uint64_t currIdle = curr->idle + curr->iowait;

    uint64_t prevTotal = prev->user + prev->nice + prev->system +
                         prev->idle + prev->iowait + prev->irq +
                         prev->softirq + prev->steal;

    uint64_t currTotal = curr->user + curr->nice + curr->system +
                         curr->idle + curr->iowait + curr->irq +
                         curr->softirq + curr->steal;

    uint64_t totalDiff = currTotal - prevTotal;
    uint64_t idleDiff = currIdle - prevIdle;

    if (totalDiff == 0) {
        *usage = 0;
        return 0;
    }

    *usage = (uint32_t)(((totalDiff - idleDiff) * 10000ULL) / totalDiff);
    return 0;
}

// 获取系统负载平均值
int GetSystemLoadAverage(double *avg1, double *avg5, double *avg15)
{
    return ReadLoadAverage(avg1, avg5, avg15);
}

// 获取系统运行时间和空闲时间
int GetSystemUptime(double *uptime, double *idleTime)
{
    return ReadUptime(uptime, idleTime);
}

// 记录历史统计
int RecordHistoryStats(const CpuStats *cpuStats, const MemoryStats *memStats)
{
    if (cpuStats == NULL || memStats == NULL) {
        return -1;
    }

    ResourceHistory *history = &g_history[g_historyIndex];
    history->timestamp = GetTimestampMs();

    if (memcpy_s(&history->cpuStats, sizeof(CpuStats),
        cpuStats, sizeof(CpuStats)) != EOK) {
        return -1;
    }

    if (memcpy_s(&history->memStats, sizeof(MemoryStats),
        memStats, sizeof(MemoryStats)) != EOK) {
        return -1;
    }

    // 记录每个核心的使用率
    for (uint32_t i = 0; i < g_cpuCoreCount && i < MAX_CPU_CORES; i++) {
        history->cpuHistory[i] = g_cpuCoreStats[i].usagePercent;
    }

    g_historyIndex = (g_historyIndex + 1) % STAT_HISTORY_SIZE;
    if (g_historyCount < STAT_HISTORY_SIZE) {
        g_historyCount++;
    }

    return 0;
}

// 获取历史统计记录
int GetHistoryStatsByIndex(uint32_t index, CpuStats *cpuStats, MemoryStats *memStats)
{
    if (cpuStats == NULL || memStats == NULL || index >= g_historyCount) {
        return -1;
    }

    uint32_t actualIndex = (g_historyIndex + STAT_HISTORY_SIZE - index - 1) % STAT_HISTORY_SIZE;
    ResourceHistory *history = &g_history[actualIndex];

    if (memcpy_s(cpuStats, sizeof(CpuStats),
        &history->cpuStats, sizeof(CpuStats)) != EOK) {
        return -1;
    }

    if (memcpy_s(memStats, sizeof(MemoryStats),
        &history->memStats, sizeof(MemoryStats)) != EOK) {
        return -1;
    }

    return 0;
}

// 计算平均CPU使用率（基于历史记录）
int CalculateAvgCpuUsage(uint32_t durationSec, uint32_t *avgUsage)
{
    if (avgUsage == NULL || g_historyCount == 0) {
        return -1;
    }

    uint64_t totalUsage = 0;
    uint32_t count = 0;

    // 计算最近N秒的平均使用率
    uint32_t sampleCount = durationSec / 5;  // 假设每5秒采样一次
    if (sampleCount == 0) {
        sampleCount = 1;
    }
    if (sampleCount > g_historyCount) {
        sampleCount = g_historyCount;
    }

    for (uint32_t i = 0; i < sampleCount; i++) {
        uint32_t actualIndex = (g_historyIndex + STAT_HISTORY_SIZE - i - 1) % STAT_HISTORY_SIZE;
        totalUsage += g_history[actualIndex].cpuStats.totalUsage;
        count++;
    }

    if (count > 0) {
        *avgUsage = (uint32_t)(totalUsage / count);
    } else {
        *avgUsage = 0;
    }

    return 0;
}

// 计算平均内存使用率（基于历史记录）
int CalculateAvgMemUsage(uint32_t durationSec, uint32_t *avgUsage)
{
    if (avgUsage == NULL || g_historyCount == 0) {
        return -1;
    }

    uint64_t totalUsage = 0;
    uint32_t count = 0;

    uint32_t sampleCount = durationSec / 5;
    if (sampleCount == 0) {
        sampleCount = 1;
    }
    if (sampleCount > g_historyCount) {
        sampleCount = g_historyCount;
    }

    for (uint32_t i = 0; i < sampleCount; i++) {
        uint32_t actualIndex = (g_historyIndex + STAT_HISTORY_SIZE - i - 1) % STAT_HISTORY_SIZE;
        totalUsage += g_history[actualIndex].memStats.usagePercent;
        count++;
    }

    if (count > 0) {
        *avgUsage = (uint32_t)(totalUsage / count);
    } else {
        *avgUsage = 0;
    }

    return 0;
}

// 获取内存使用趋势
typedef enum {
    TREND_STABLE = 0,
    TREND_INCREASING,
    TREND_DECREASING,
    TREND_UNKNOWN
} MemoryTrend;

int GetMemoryTrend(MemoryTrend *trend)
{
    if (trend == NULL || g_historyCount < 3) {
        *trend = TREND_UNKNOWN;
        return -1;
    }

    // 获取最近3个样本的内存使用率
    uint32_t recent[3];
    for (int i = 0; i < 3; i++) {
        uint32_t actualIndex = (g_historyIndex + STAT_HISTORY_SIZE - i - 1) % STAT_HISTORY_SIZE;
        recent[i] = g_history[actualIndex].memStats.usagePercent;
    }

    // 分析趋势
    int32_t diff1 = recent[0] - recent[1];
    int32_t diff2 = recent[1] - recent[2];

    if (diff1 > 100 && diff2 > 100) {  // 连续上升超过1%
        *trend = TREND_INCREASING;
    } else if (diff1 < -100 && diff2 < -100) {  // 连续下降超过1%
        *trend = TREND_DECREASING;
    } else {
        *trend = TREND_STABLE;
    }

    return 0;
}

// 获取文件系统使用情况
typedef struct {
    char mountPoint[256];
    char device[256];
    char fsType[64];
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t freeBytes;
    uint32_t usagePercent;
    uint32_t inodeUsagePercent;
} FileSystemStats;

int GetFileSystemStats(FileSystemStats *stats, uint32_t maxCount, uint32_t *actualCount)
{
    if (stats == NULL || actualCount == NULL) {
        return -1;
    }

    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/mounts: %d", errno);
        return -1;
    }

    char line[MAX_BUFFER_SIZE];
    uint32_t count = 0;

    while (fgets(line, sizeof(line), fp) != NULL && count < maxCount) {
        char device[256];
        char mountPoint[256];
        char fsType[64];

        if (sscanf_s(line, "%255s %255s %63s",
            device, sizeof(device),
            mountPoint, sizeof(mountPoint),
            fsType, sizeof(fsType)) < 3) {
            continue;
        }

        // 跳过某些虚拟文件系统
        if (strncmp(device, "proc", 4) == 0 ||
            strncmp(device, "sysfs", 5) == 0 ||
            strncmp(device, "devfs", 5) == 0 ||
            strncmp(device, "tmpfs", 5) == 0 ||
            strncmp(device, "cgroup", 6) == 0) {
            continue;
        }

        // 使用statvfs获取详细信息（这里简化处理）
        strncpy_s(stats[count].device, sizeof(stats[count].device),
            device, strlen(device));
        strncpy_s(stats[count].mountPoint, sizeof(stats[count].mountPoint),
            mountPoint, strlen(mountPoint));
        strncpy_s(stats[count].fsType, sizeof(stats[count].fsType),
            fsType, strlen(fsType));

        // 简化：设置默认值
        stats[count].totalBytes = 0;
        stats[count].usedBytes = 0;
        stats[count].freeBytes = 0;
        stats[count].usagePercent = 0;
        stats[count].inodeUsagePercent = 0;

        count++;
    }

    fclose(fp);
    *actualCount = count;
    return 0;
}

// ============== 内部函数实现 ==============

static int ParseCpuCoreStats(const char *line, CpuCoreStats *stats)
{
    if (line == NULL || stats == NULL) {
        return -1;
    }

    char cpuLabel[16];
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;

    int parsed = sscanf_s(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        cpuLabel, sizeof(cpuLabel),
        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

    if (parsed < 5) {
        return -1;
    }

    stats->user = user;
    stats->nice = nice;
    stats->system = system;
    stats->idle = idle;
    stats->iowait = (parsed >= 6) ? iowait : 0;
    stats->irq = (parsed >= 7) ? irq : 0;
    stats->softirq = (parsed >= 8) ? softirq : 0;
    stats->steal = (parsed >= 9) ? steal : 0;
    stats->guest = (parsed >= 10) ? guest : 0;
    stats->guest_nice = (parsed >= 11) ? guest_nice : 0;

    return 0;
}

static int ReadLoadAverage(double *avg1, double *avg5, double *avg15)
{
    if (avg1 == NULL || avg5 == NULL || avg15 == NULL) {
        return -1;
    }

    FILE *fp = fopen("/proc/loadavg", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/loadavg: %d", errno);
        return -1;
    }

    char line[MAX_BUFFER_SIZE];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    int parsed = sscanf_s(line, "%lf %lf %lf", avg1, avg5, avg15);
    if (parsed != 3) {
        return -1;
    }

    return 0;
}

static int ReadUptime(double *uptime, double *idleTime)
{
    if (uptime == NULL || idleTime == NULL) {
        return -1;
    }

    FILE *fp = fopen("/proc/uptime", "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open /proc/uptime: %d", errno);
        return -1;
    }

    char line[MAX_BUFFER_SIZE];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    int parsed = sscanf_s(line, "%lf %lf", uptime, idleTime);
    if (parsed != 2) {
        return -1;
    }

    return 0;
}

static uint64_t GetTimestampMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 打印资源统计报告
void PrintResourceStatsReport(void)
{
    INIT_LOGI("=== Resource Statistics Report ===");

    // 打印CPU统计
    double avg1, avg5, avg15;
    if (GetSystemLoadAverage(&avg1, &avg5, &avg15) == 0) {
        INIT_LOGI("Load Average: %.2f, %.2f, %.2f", avg1, avg5, avg15);
    }

    // 打印运行时间
    double uptime, idleTime;
    if (GetSystemUptime(&uptime, &idleTime) == 0) {
        INIT_LOGI("System Uptime: %.0f seconds", uptime);
        INIT_LOGI("Idle Time: %.0f seconds", idleTime);
    }

    // 打印CPU核心使用率
    CpuCoreStats coreStats[MAX_CPU_CORES];
    uint32_t coreCount;
    if (GetCpuCoreStats(coreStats, &coreCount) == 0) {
        for (uint32_t i = 0; i < coreCount && i < 8; i++) {  // 只打印前8个核心
            INIT_LOGI("CPU %u: %u.%02u%%", i,
                coreStats[i].usagePercent / 100,
                coreStats[i].usagePercent % 100);
        }
    }

    // 打印历史统计
    INIT_LOGI("History samples: %u", g_historyCount);
    if (g_historyCount > 0) {
        uint32_t avgCpu, avgMem;
        if (CalculateAvgCpuUsage(60, &avgCpu) == 0) {
            INIT_LOGI("Average CPU usage (60s): %u.%02u%%",
                avgCpu / 100, avgCpu % 100);
        }
        if (CalculateAvgMemUsage(60, &avgMem) == 0) {
            INIT_LOGI("Average Memory usage (60s): %u.%02u%%",
                avgMem / 100, avgMem % 100);
        }
    }
}

// 导出资源统计到文件
int ExportResourceStats(const char *filePath)
{
    if (filePath == NULL) {
        return -1;
    }

    FILE *fp = fopen(filePath, "w");
    if (fp == NULL) {
        INIT_LOGE("Failed to open resource stats file: %s, err=%d", filePath, errno);
        return -1;
    }

    fprintf(fp, "=== Resource Statistics Export ===\n\n");

    // 系统负载
    double avg1, avg5, avg15;
    if (GetSystemLoadAverage(&avg1, &avg5, &avg15) == 0) {
        fprintf(fp, "[Load Average]\n");
        fprintf(fp, "  1 minute: %.2f\n", avg1);
        fprintf(fp, "  5 minutes: %.2f\n", avg5);
        fprintf(fp, "  15 minutes: %.2f\n\n", avg15);
    }

    // 运行时间
    double uptime, idleTime;
    if (GetSystemUptime(&uptime, &idleTime) == 0) {
        fprintf(fp, "[Uptime]\n");
        fprintf(fp, "  System uptime: %.0f seconds (%.2f days)\n",
            uptime, uptime / 86400.0);
        fprintf(fp, "  Idle time: %.0f seconds\n\n", idleTime);
    }

    // CPU核心统计
    CpuCoreStats coreStats[MAX_CPU_CORES];
    uint32_t coreCount;
    if (GetCpuCoreStats(coreStats, &coreCount) == 0) {
        fprintf(fp, "[CPU Cores] (%u total)\n", coreCount);
        for (uint32_t i = 0; i < coreCount; i++) {
            fprintf(fp, "  CPU%u: %u.%02u%% (User:%llu System:%llu Idle:%llu)\n",
                i, coreStats[i].usagePercent / 100, coreStats[i].usagePercent % 100,
                (unsigned long long)coreStats[i].user,
                (unsigned long long)coreStats[i].system,
                (unsigned long long)coreStats[i].idle);
        }
        fprintf(fp, "\n");
    }

    // 历史记录
    fprintf(fp, "[History] (%u samples)\n", g_historyCount);
    for (uint32_t i = 0; i < g_historyCount && i < 10; i++) {
        uint32_t actualIndex = (g_historyIndex + STAT_HISTORY_SIZE - i - 1) % STAT_HISTORY_SIZE;
        ResourceHistory *hist = &g_history[actualIndex];
        fprintf(fp, "  Sample %u: CPU=%u.%02u%% MEM=%u.%02u%% Time=%llu\n",
            i,
            hist->cpuStats.totalUsage / 100,
            hist->cpuStats.totalUsage % 100,
            hist->memStats.usagePercent / 100,
            hist->memStats.usagePercent % 100,
            (unsigned long long)hist->timestamp);
    }

    fclose(fp);

    INIT_LOGI("Resource stats exported to: %s", filePath);
    return 0;
}