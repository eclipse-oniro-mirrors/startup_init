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

#ifndef STARTUP_INIT_SYSMONITOR_H
#define STARTUP_INIT_SYSMONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MONITOR_OK              0
#define MONITOR_ERROR           (-1)
#define MONITOR_INVALID_PARAM   (-2)

#define DEFAULT_SAMPLE_INTERVAL_MS  5000
#define DEFAULT_HISTORY_SIZE        60
#define MAX_PROCESS_COUNT           1024
#define MAX_LINE_LENGTH             512
#define MAX_ALARM_COUNT             100
#define MAX_PERF_RECORD_COUNT       500
#define MAX_DISK_STATS_COUNT        16
#define MAX_NET_STATS_COUNT         16
#define MAX_CPU_CORES               128
#define STAT_HISTORY_SIZE           60

#define CPU_THRESHOLD_DEFAULT       80
#define MEM_THRESHOLD_DEFAULT       85
#define DISK_THRESHOLD_DEFAULT      90

#define PERCENT_MULTIPLIER          100
#define PERCENT_SCALE               10000ULL
#define BYTES_PER_KB                1024
#define BYTES_PER_MB                ((uint64_t)1024 * (uint64_t)1024)
#define NSEC_PER_SEC                1000000000LL
#define NSEC_PER_MSEC               1000000LL
#define NSEC_PER_USEC               1000LL

#define PROC_STAT_PATH              "/proc/stat"
#define PROC_MEMINFO_PATH           "/proc/meminfo"
#define PROC_DISKSTATS_PATH         "/proc/diskstats"
#define PROC_NET_DEV_PATH           "/proc/net/dev"
#define PROC_LOADAVG_PATH           "/proc/loadavg"
#define PROC_UPTIME_PATH            "/proc/uptime"
#define PROC_MOUNTS_PATH            "/proc/mounts"
#define PROC_DIR_PATH               "/proc"

#define CPU_LABEL_PREFIX            "cpu"
#define CPU_LABEL_LEN               16
#define MEM_KEY_LEN                 64
#define MEM_UNIT_LEN                16
#define DEVICE_NAME_MAX_LEN         64
#define NET_IFACE_MAX_LEN           32
#define ALARM_MSG_MAX_LEN           256
#define PERF_NAME_MAX_LEN           64
#define FS_TYPE_MAX_LEN             64
#define MOUNT_POINT_MAX_LEN         256
#define PROCESS_NAME_MAX_LEN        64

typedef enum {
    MONITOR_TYPE_CPU = 0,
    MONITOR_TYPE_MEMORY,
    MONITOR_TYPE_PROCESS,
    MONITOR_TYPE_DISK,
    MONITOR_TYPE_NETWORK,
    MONITOR_TYPE_MAX
} MonitorType;

typedef enum {
    ALARM_LEVEL_INFO = 0,
    ALARM_LEVEL_WARNING,
    ALARM_LEVEL_CRITICAL,
    ALARM_LEVEL_EMERGENCY,
    ALARM_LEVEL_MAX
} AlarmLevel;

typedef enum {
    MONITOR_STATE_IDLE = 0,
    MONITOR_STATE_RUNNING,
    MONITOR_STATE_PAUSED,
    MONITOR_STATE_ERROR,
    MONITOR_STATE_MAX
} MonitorState;

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
    uint64_t totalUsage;
    uint32_t cpuCoreNum;
    uint32_t contextSwitches;
    uint32_t processesCreated;
    uint32_t processesRunning;
    uint32_t processesBlocked;
} CpuStats;

typedef struct {
    uint64_t totalMem;
    uint64_t freeMem;
    uint64_t availableMem;
    uint64_t buffers;
    uint64_t cached;
    uint64_t swapTotal;
    uint64_t swapFree;
    uint32_t usagePercent;
    uint32_t swapUsagePercent;
} MemoryStats;

typedef struct ProcessInfo {
    ListNode node;
    int32_t pid;
    char name[PROCESS_NAME_MAX_LEN];
    uint64_t cpuUsage;
    uint64_t memUsage;
} ProcessInfo;

typedef struct {
    char deviceName[DEVICE_NAME_MAX_LEN];
    uint64_t readsCompleted;
    uint64_t writesCompleted;
    uint64_t readBytesPerSec;
    uint64_t writeBytesPerSec;
    uint32_t utilization;
} DiskStats;

typedef struct {
    char interface[NET_IFACE_MAX_LEN];
    uint64_t rxBytes;
    uint64_t txBytes;
    uint64_t rxBytesPerSec;
    uint64_t txBytesPerSec;
} NetworkStats;

typedef struct PerfRecord {
    ListNode node;
    char name[PERF_NAME_MAX_LEN];
    struct timespec startTime;
    struct timespec endTime;
    uint64_t durationNs;
    uint32_t type;
    uint32_t flags;
    void *userData;
} PerfRecord;

typedef struct MonitorAlarm {
    ListNode node;
    MonitorType type;
    AlarmLevel level;
    char message[ALARM_MSG_MAX_LEN];
    struct timespec timestamp;
    uint32_t threshold;
    uint32_t actualValue;
    bool handled;
} MonitorAlarm;

typedef struct {
    uint32_t sampleIntervalMs;
    uint32_t historySize;
    bool enableCpuMonitor;
    bool enableMemMonitor;
    bool enableProcMonitor;
    bool enableDiskMonitor;
    bool enableNetMonitor;
    uint32_t cpuThreshold;
    uint32_t memThreshold;
    uint32_t diskThreshold;
} MonitorConfig;

typedef struct {
    MonitorState state;
    MonitorConfig config;
    ListNode processList;
    ListNode alarmList;
    ListNode perfRecordList;
    CpuStats cpuStats;
    MemoryStats memStats;
    DiskStats diskStats[MAX_DISK_STATS_COUNT];
    NetworkStats netStats[MAX_NET_STATS_COUNT];
    uint64_t lastUpdateTime;
    uint32_t alarmCount;
    uint32_t recordCount;
} MonitorContext;

int InitMonitor(const MonitorConfig *config);
void DestroyMonitor(void);
int StartMonitor(void);
int StopMonitor(void);
int PauseMonitor(void);
int ResumeMonitor(void);
MonitorState GetMonitorState(void);

int UpdateCpuStats(CpuStats *stats);
int UpdateMemoryStats(MemoryStats *stats);
int UpdateProcessStats(void);
int UpdateDiskStats(DiskStats *stats, uint32_t maxCount);
int UpdateNetworkStats(NetworkStats *stats, uint32_t maxCount);

const CpuStats *GetCpuStats(void);
const MemoryStats *GetMemoryStats(void);
const ListNode *GetProcessList(void);
ProcessInfo *GetProcessInfo(int pid);

int AddMonitorAlarm(MonitorType type, AlarmLevel level,
    const char *message, uint32_t threshold, uint32_t actualValue);
const ListNode *GetAlarmList(void);
void ClearHandledAlarms(void);

PerfRecord *BeginPerfRecord(const char *name, uint32_t type);
int EndPerfRecord(PerfRecord *record);
const ListNode *GetPerfRecords(void);

uint32_t CalculateCpuUsage(const CpuStats *prev, const CpuStats *curr);
uint32_t CalculateMemUsage(const MemoryStats *stats);

ProcessInfo *FindTopCpuProcess(void);
ProcessInfo *FindTopMemProcess(void);

int CheckThresholds(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* STARTUP_INIT_SYSMONITOR_H */
