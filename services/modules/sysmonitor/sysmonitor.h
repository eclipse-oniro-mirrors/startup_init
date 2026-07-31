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

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

// 监控类型定义
typedef enum {
    MONITOR_TYPE_CPU = 0,
    MONITOR_TYPE_MEMORY,
    MONITOR_TYPE_PROCESS,
    MONITOR_TYPE_DISK,
    MONITOR_TYPE_NETWORK,
    MONITOR_TYPE_MAX
} MonitorType;

// 告警级别
typedef enum {
    ALARM_LEVEL_INFO = 0,
    ALARM_LEVEL_WARNING,
    ALARM_LEVEL_CRITICAL,
    ALARM_LEVEL_EMERGENCY,
    ALARM_LEVEL_MAX
} AlarmLevel;

// 监控状态
typedef enum {
    MONITOR_STATE_IDLE = 0,
    MONITOR_STATE_RUNNING,
    MONITOR_STATE_PAUSED,
    MONITOR_STATE_ERROR,
    MONITOR_STATE_MAX
} MonitorState;

// CPU统计信息
typedef struct {
    uint64_t userModeTime;      // 用户态时间(毫秒)
    uint64_t niceTime;          // 低优先级进程时间(毫秒)
    uint64_t systemTime;        // 内核态时间(毫秒)
    uint64_t idleTime;          // 空闲时间(毫秒)
    uint64_t ioWaitTime;        // IO等待时间(毫秒)
    uint64_t irqTime;           // 硬中断时间(毫秒)
    uint64_t softIrqTime;       // 软中断时间(毫秒)
    uint64_t stealTime;         // 虚拟化环境下的被窃取时间(毫秒)
    uint64_t guestTime;         // 客户机操作系统时间(毫秒)
    uint64_t guestNiceTime;     // 客户机低优先级进程时间(毫秒)
    uint64_t totalUsage;        // 总使用率(百分比*100)
    uint32_t cpuCoreNum;        // CPU核心数
    uint32_t contextSwitches;   // 上下文切换次数
    uint32_t processesCreated;  // 创建的进程数
    uint32_t processesRunning;  // 运行中的进程数
    uint32_t processesBlocked;  // 阻塞的进程数
} CpuStats;

// 内存统计信息
typedef struct {
    uint64_t totalMem;          // 总内存(字节)
    uint64_t freeMem;           // 空闲内存(字节)
    uint64_t availableMem;      // 可用内存(字节)
    uint64_t buffers;           // 缓冲区内存(字节)
    uint64_t cached;            // 缓存内存(字节)
    uint64_t swapCached;        // 交换缓存(字节)
    uint64_t activeMem;         // 活跃内存(字节)
    uint64_t inactiveMem;       // 非活跃内存(字节)
    uint64_t activeAnon;        // 活跃匿名内存(字节)
    uint64_t inactiveAnon;      // 非活跃匿名内存(字节)
    uint64_t activeFile;        // 活跃文件缓存(字节)
    uint64_t inactiveFile;      // 非活跃文件缓存(字节)
    uint64_t unevictable;       // 不可回收内存(字节)
    uint64_t mlocked;           // 锁定内存(字节)
    uint64_t swapTotal;         // 总交换空间(字节)
    uint64_t swapFree;          // 空闲交换空间(字节)
    uint64_t dirtyPages;        // 脏页数(字节)
    uint64_t writeback;         // 回写中页数(字节)
    uint64_t anonPages;         // 匿名页(字节)
    uint64_t mapped;            // 映射页(字节)
    uint64_t shmem;             // 共享内存(字节)
    uint64_t slab;              // Slab内存(字节)
    uint64_t slabReclaimable;   // 可回收Slab(字节)
    uint64_t slabUnreclaimable; // 不可回收Slab(字节)
    uint64_t kernelStack;       // 内核栈(字节)
    uint64_t pageTables;        // 页表(字节)
    uint64_t nfsUnstable;       // NFS不稳定页(字节)
    uint64_t bounce;            // 弹性缓冲区(字节)
    uint64_t writebackTmp;      // 临时回写(字节)
    uint64_t commitLimit;       // 提交限制(字节)
    uint64_t committedAs;       // 已提交内存(字节)
    uint64_t vmallocTotal;      // 虚拟内存总量(字节)
    uint64_t vmallocUsed;       // 已用虚拟内存(字节)
    uint64_t vmallocChunk;      // 虚拟内存块(字节)
    uint32_t usagePercent;      // 使用率(百分比*100)
    uint32_t swapUsagePercent;  // 交换区使用率(百分比*100)
} MemoryStats;

// 进程信息
typedef struct ProcessInfo {
    ListNode node;
    int32_t pid;                // 进程ID
    int32_t ppid;               // 父进程ID
    char name[64];              // 进程名
    char state;                 // 进程状态
    uint64_t utime;             // 用户态时间(时钟滴答)
    uint64_t stime;             // 内核态时间(时钟滴答)
    int64_t priority;           // 优先级
    int64_t nice;               // Nice值
    int64_t numThreads;         // 线程数
    int64_t vsize;              // 虚拟内存大小(字节)
    int64_t rss;                // 驻留集大小(字节)
    uint64_t startTime;         // 启动时间(时钟滴答)
    uint64_t delayBlkIo;        // IO延迟(时钟滴答)
    uint64_t cpuUsage;          // CPU使用率(百分比*100)
    uint64_t memUsage;          // 内存使用率(百分比*100)
    char cmdline[256];          // 完整命令行
    uint32_t flags;             // 进程标志位
} ProcessInfo;

// 磁盘统计信息
typedef struct {
    char deviceName[64];        // 设备名
    uint64_t readsCompleted;    // 完成的读操作数
    uint64_t readsMerged;       // 合并的读操作数
    uint64_t sectorsRead;       // 读取的扇区数
    uint64_t readTimeMs;        // 读操作耗时(毫秒)
    uint64_t writesCompleted;   // 完成的写操作数
    uint64_t writesMerged;      // 合并的写操作数
    uint64_t sectorsWritten;    // 写入的扇区数
    uint64_t writeTimeMs;       // 写操作耗时(毫秒)
    uint64_t ioInProgress;      // 正在进行的IO数
    uint64_t ioTimeMs;          // IO总耗时(毫秒)
    uint64_t weightedIoTimeMs;  // 加权IO耗时(毫秒)
    uint64_t readBytesPerSec;   // 读速率(字节/秒)
    uint64_t writeBytesPerSec;  // 写速率(字节/秒)
    uint32_t utilization;       // 利用率(百分比*100)
} DiskStats;

// 网络统计信息
typedef struct {
    char interface[32];         // 网络接口名
    uint64_t rxBytes;           // 接收字节数
    uint64_t rxPackets;         // 接收包数
    uint64_t rxErrors;          // 接收错误数
    uint64_t rxDropped;         // 接收丢包数
    uint64_t rxFifo;            // 接收FIFO错误数
    uint64_t rxFrame;           // 接收帧错误数
    uint64_t rxCompressed;      // 接收压缩包数
    uint64_t rxMulticast;       // 接收多播包数
    uint64_t txBytes;           // 发送字节数
    uint64_t txPackets;         // 发送包数
    uint64_t txErrors;          // 发送错误数
    uint64_t txDropped;         // 发送丢包数
    uint64_t txFifo;            // 发送FIFO错误数
    uint64_t txCollisions;      // 发送冲突数
    uint64_t txCarrier;         // 发送载波错误数
    uint64_t txCompressed;      // 发送压缩包数
    uint64_t rxBytesPerSec;     // 接收速率(字节/秒)
    uint64_t txBytesPerSec;     // 发送速率(字节/秒)
} NetworkStats;

// 性能分析记录
typedef struct PerfRecord {
    ListNode node;
    char name[64];              // 记录名称
    struct timespec startTime;  // 开始时间
    struct timespec endTime;    // 结束时间
    uint64_t durationNs;        // 持续时间(纳秒)
    uint32_t type;              // 记录类型
    uint32_t flags;             // 标志位
    void *userData;             // 用户数据指针
} PerfRecord;

// 告警信息
typedef struct MonitorAlarm {
    ListNode node;
    MonitorType type;           // 监控类型
    AlarmLevel level;           // 告警级别
    char message[256];          // 告警消息
    struct timespec timestamp;  // 告警时间
    uint32_t threshold;         // 阈值
    uint32_t actualValue;       // 实际值
    bool handled;               // 是否已处理
} MonitorAlarm;

// 监控配置
typedef struct {
    uint32_t sampleIntervalMs;  // 采样间隔(毫秒)
    uint32_t historySize;       // 历史记录数量
    bool enableCpuMonitor;      // 启用CPU监控
    bool enableMemMonitor;      // 启用内存监控
    bool enableProcMonitor;     // 启用进程监控
    bool enableDiskMonitor;     // 启用磁盘监控
    bool enableNetMonitor;      // 启用网络监控
    uint32_t cpuThreshold;      // CPU告警阈值(百分比)
    uint32_t memThreshold;      // 内存告警阈值(百分比)
    uint32_t diskThreshold;     // 磁盘利用率阈值(百分比)
} MonitorConfig;

// 监控上下文
typedef struct {
    MonitorState state;         // 监控状态
    MonitorConfig config;       // 配置信息
    ListNode processList;       // 进程列表
    ListNode alarmList;         // 告警列表
    ListNode perfRecordList;    // 性能记录列表
    CpuStats cpuStats;          // CPU统计
    MemoryStats memStats;       // 内存统计
    DiskStats diskStats[16];    // 磁盘统计
    NetworkStats netStats[16];  // 网络统计
    uint64_t lastUpdateTime;    // 最后更新时间
    uint32_t alarmCount;        // 告警计数
    uint32_t recordCount;       // 记录计数
} MonitorContext;

// 初始化监控模块
int InitMonitor(const MonitorConfig *config);

// 销毁监控模块
void DestroyMonitor(void);

// 启动监控
int StartMonitor(void);

// 停止监控
int StopMonitor(void);

// 暂停监控
int PauseMonitor(void);

// 恢复监控
int ResumeMonitor(void);

// 获取监控状态
MonitorState GetMonitorState(void);

// 更新CPU统计
int UpdateCpuStats(CpuStats *stats);

// 更新内存统计
int UpdateMemoryStats(MemoryStats *stats);

// 更新进程统计
int UpdateProcessStats(void);

// 更新磁盘统计
int UpdateDiskStats(DiskStats *stats, uint32_t maxCount);

// 更新网络统计
int UpdateNetworkStats(NetworkStats *stats, uint32_t maxCount);

// 获取CPU统计
const CpuStats* GetCpuStats(void);

// 获取内存统计
const MemoryStats* GetMemoryStats(void);

// 获取进程列表
const ListNode* GetProcessList(void);

// 获取进程信息
ProcessInfo* GetProcessInfo(int pid);

// 添加告警
int AddMonitorAlarm(MonitorType type, AlarmLevel level,
    const char *message, uint32_t threshold, uint32_t actualValue);

// 获取告警列表
const ListNode* GetAlarmList(void);

// 清除已处理告警
void ClearHandledAlarms(void);

// 开始性能记录
PerfRecord* BeginPerfRecord(const char *name, uint32_t type);

// 结束性能记录
int EndPerfRecord(PerfRecord *record);

// 获取性能记录列表
const ListNode* GetPerfRecords(void);

// 打印监控摘要
void PrintMonitorSummary(void);

// 导出监控数据到文件
int ExportMonitorData(const char *filePath);

// 计算CPU使用率
uint32_t CalculateCpuUsage(const CpuStats *prev, const CpuStats *curr);

// 计算内存使用率
uint32_t CalculateMemUsage(const MemoryStats *stats);

// 查找占用资源最多的进程
ProcessInfo* FindTopCpuProcess(void);
ProcessInfo* FindTopMemProcess(void);

// 系统诊断
int RunSystemDiagnosis(const char *outputPath);

// 监控回调类型
typedef void (*MonitorCallback)(MonitorType type, const void *data, void *context);

// 注册监控回调
int RegisterMonitorCallback(MonitorType type, MonitorCallback callback, void *context);

// 注销监控回调
int UnregisterMonitorCallback(MonitorType type);

// 阈值检查
int CheckThresholds(void);

// 获取历史统计
int GetHistoryStats(MonitorType type, void *stats, uint32_t index);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* STARTUP_INIT_SYSMONITOR_H */