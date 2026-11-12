# 技术术语知识

init 部件核心技术框架和机制的知识定义，涵盖价值认知、原理、使用方式和代码关联。

## 两阶段启动与必需分区挂载

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 系统分区在 ramdisk 之外，PID 1 必须先把根文件系统与数据分区挂起来才能进入完整用户态；同时分区完整性（防篡改）需要在挂载时校验 |
| 技术目标 | 用最小 ramdisk 环境完成必需分区挂载、签名校验（hvb/dm-verity）、A/B 槽位处理与切根，再进入第二阶段完整初始化 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 入口 `services/init/main.c` `main()`：argv 含 `--second-stage` 即第二阶段，否则第一阶段并校验 `getpid()==1`。第一阶段 `SystemPrepare`（`services/init/standard/init_firststage.c:243`）：SIGPIPE 忽略 → `EarlyLogInit` → `CreateFsAndDeviceNode` → INIT_FIRST_STAGE hook → updater 模式直接返回（不挂载不切根）→ `MountRequiredPartitions`（:148）→ `StartSecondStageInit`（:206，`CloseStdio` 后 `execv("/bin/init",{"--second-stage",uptime})`；ASAN 场景可切换 `/system/asan/bin/init`）。挂载流程：`LoadRequiredFstab`（cmdline 优先，回退 `/system/etc/fstab.required`）→ `GetRequiredDevices` → `StartUeventd`（init 进程内先跑一轮 `RetriggerUevent` 拉起 required 设备）→ `DoMountRequired`：A/B（`GetBootSlots()>=2`）执行 INIT_SNAPSHOT_ACTIVE hook、EROFS overlay `SetupOverlay`、`MountAllWithFstab(fstab,1)`，失败 `abort()` 并打 `[startup_failed]...FSTAB_MOUNT_FAILED` |
| 关键流程 | hvb/dm-verity：`MountAllWithFstab`（`interfaces/innerkits/fs_manager/fstab_mount.c:1082`）required 时先 `HvbDmVerityinit`；每项 `CheckRequiredAndMount`→`HvbDmVeritySetUp(item)`→`MountOneItem`，结束 `HvbDmVerityFinal`；fstab flag 字符串 `hvb` 映射 `FS_MANAGER_HVB`；使能条件 cmdline `ohos.boot.hvb.oem_swtype`（`HVB_CMDLINE_HVB_FORCE_ENABLE`，`dm_verity/dm_verity.c:31`）。切根：`SwitchRoot("/usr")`（`switch_root/switch_root.c:142`）由 fstab_mount.c/erofs_overlay 调用。另有 `main_early.c`（只跑 `SystemPrepare`，编译为 ramdisk 内 `init_early`） |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | 无对外独立 API。产品配置经 fstab.required（flag `required`/`wait`/`hvb`）与 cmdline（fstab 路径、hvb 使能、`bootslots`/`currentslot`）介入；调试可用 `begetctl` dump 与 `startup_failed` 日志定位挂载失败 |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `FstabItem`（fs_manager）；hvb 句柄结构（`libfs_hvb/fs_hvb.h`） |
| 核心成员 | fstab 项的 `fsType/mountPoint/deviceName/fsmountFlags`；A/B 槽位状态 |
| 核心方法 | `SystemPrepare`/`MountRequiredPartitions`/`StartSecondStageInit`（init_firststage.c）；`LoadRequiredFstab`/`DoMountRequired`（init_firststage.c 与 `services/init/standard/init_mount.c:34`）；`MountAllWithFstab`/`CheckRequiredAndMount`（fstab_mount.c）；`FsHvbInit/FsHvbSetupHashtree/FsHvbFinal`（libfs_hvb/fs_hvb.c）；`SwitchRoot`（switch_root.c） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 必需分区挂载失败整机 abort（无法进入第二阶段）；updater 模式跳过全部挂载逻辑；`wait` flag 的等待上限 `WAIT_MAX_SECOND=5` 秒 |
| 并发约束 | 第一阶段在 exec 第二阶段前串行完成；不可引入并发挂载依赖 |
| 安全约束 | hvb/dm-verity 链路（`libfs_hvb`、`dm_verity`）与签名相关，改动需安全评审；cmdline 是可信输入边界 |

## 启动配置解析与分组加载

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 标准系统配置分散在 system/vendor/chipset/sys_prod/chip_prod 多个分区目录，需统一的解析、导入与优先级规则；不同启动模式（充电/恢复/updater）需加载不同配置集 |
| 技术目标 | 以 JSON cfg 为唯一事实源，支持 import 递归、`${param}` 展开、分组裁剪与多分区覆盖 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | `ParseInitCfg`（`services/init/init_config.c:54`）读取文件 → cJSON 解析 → `ParseInitCfgContents`：`ParseAllServices`→`ParseAllJobs`→`ParseAllImports`（import 项经 `GetParamValue` 参数展开后递归，深度上限 `IMPORT_MAX_LEVEL=100`）。路径前缀 `/vendor|/chipset|/chip_prod/etc/init/` 的配置标记 chipset 上下文（`GetConfigContextType`），其命令经 sub-init 执行。`ReadConfig`（:111）按 `ohos.boot.mode`/misc 分流：充电（`/system/etc/charger`）、penglai、rescue、maintenance、updater、普通（`ReadFileInDir("/etc",".cfg")`）；`ParseCfgByPriority`（standard/init.c:414）经 `GetCfgFiles` 按目录优先级 `/system`<`/chipset`<`/sys_prod`<`/chip_prod` 加载 |
| 关键流程 | 分组：`InitServiceSpace`（`services/init/init_group_manager.c:202`）确定 bootgroup（cmdline `bootgroup`，默认 `device.boot.group`；充电模式 `device.charge.group`）→ `InitParseGroupCfg`（:241）解析组文件 import（深度 ≤`GROUP_IMPORT_MAX_LEVEL=5`）→ 服务/job 添加时 `CheckNodeValid` 过滤 |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | 配置文件即可视接口：`{"jobs":[{name,condition,cmds[]}],"services":[...],"import":["..."]}`；分组文件 `{"jobs":[],"services":[],"groups":[]}`（名字数组）。产品通过放置目录表达优先级，不需要代码改动 |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `Service`/`Trigger`/`Command`（见实体文档）；`ConfigContext`（INIT_CONTEXT_MAIN/CHIPSET） |
| 核心成员 | `g_cmdTable` 命令表（通用+标准+插件三级）；`InitWorkspace` 分组 hashmap |
| 核心方法 | `ParseInitCfg`/`ParseAllImports`/`ReadConfig`（init_config.c）；`GetConfigContextType`/`ParseCfgByPriority`（init_config.c / standard/init.c:414）；`InitServiceSpace`/`InitParseGroupCfg`/`CheckNodeValid`（init_group_manager.c） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 单 job 命令数 ≤200（`MAX_CMD_CNT_IN_ONE_JOB`）；lite `loadcfg` ≤50KB/20 条；JSON 非法即整文件失效（`Cfg error` 日志） |
| 并发约束 | 解析发生在 init 单线程 `SystemConfig` 阶段，此时事件循环未起 |
| 安全约束 | cfg/para 属产品配置面，加载顺序（const 不可覆盖、目录优先级）是覆盖语义的一部分，不可调整；`ohos.para.dac` 修改需安全评审 |

## 服务孵化与权限设置（fork + SetPerms）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 每个系统服务需要不同的 uid/gid/组、Linux capabilities、SELinux 域与 syscall 过滤，且这些设置必须在一个确定的顺序下完成，否则能力丢失或越权 |
| 技术目标 | init 在 fork 与 exec 之间一次性、按固定顺序完成全部身份/能力/过滤器设置，保证 exec 后的进程即目标形态 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | `ServiceStart`（`services/init/init_common_service.c:902`）：`IsServiceInvalid` 路径校验（`/data/` 前缀容错）→ INIT_SERVICE_FORK_BEFORE hook → pre-start job → `fork()`。父进程 `HandleServicePostFork`（记 pid、`ProcessServiceAdd`、`NotifyServiceChange(SERVICE_STARTED)`、INIT_SERVICE_FORK_AFTER hook）。子进程 `RunChildProcessInternal`（:767）：chipset 上下文 `SetSubInitContext` → 关闭其余服务 ondemand fd → `SetServiceEnterSandbox`（失败仅告警）→ `SetAccessToken` → `InitServiceResources`（:551）：`SetServiceEnv` → on-start job → `ClearEnvironment`（解阻塞 SIGCHLD/SIGTERM）→ 非 ondemand `CreateSocketForService`+`SetSocketEnvForService` → `CreateServiceFile` → console/kmsg → `PublishHoldFds` → `BindCpuCore` → `WritePid` → `SetPerms` → `SetServiceContent`（secon setexeccon）→ `ServiceExec`（execv；saspawn 走 `InitServiceBySaspawn`） |
| 关键流程 | `SetPerms`（:295-355）固定顺序：①INIT_SERVICE_SET_PERMS_BEFORE hook ②`KeepCapability`（securebits：setuid 场景 SECBIT_NO_SETUID_FIXUP，否则 SECBIT_KEEP_CAPS，`services/init/adapter/init_adapter.c:32`）③`ServiceSetGid`（setgid+setgroups）④**`SetSystemSeccompPolicy`（先于 setuid；跳过 appspawn/nwebspawn，转插件命令 "SetSeccompPolicy"）** ⑤`setuid`（uid=0 且配 caps 时 `DropCapability` 逐项收紧 capbset）⑥`SetCapabilities`（capset + ambient raise）⑦INIT_SERVICE_SET_PERMS hook。失败分支分别以 `INIT_EGIDSET/INIT_ESECCOMP/INIT_EUIDSET/INIT_ECAP` 退出 |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | 服务 cfg 字段（uid/gid/caps/permission/apl/secon/setuid/writepid/cpucore/cgroup/socket/file/envs/period...）即配置面；代码侧 `StartServiceByName`/`StartOnDemandServiceByName`/`StopServiceByName`（`services/init/init_service_manager.c`）；模块侧可挂 INIT_SERVICE_FORK_BEFORE/AFTER、INIT_SERVICE_SET_PERMS(_BEFORE) hook |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `Service`（含 caps 数组、socket/file 列表、jobs 指针） |
| 核心成员 | `attribute`（属性位）、`servPerm`（caps）、`onRestart`/`restartArg` |
| 核心方法 | `ServiceStart`/`RunChildProcessInternal`/`InitServiceResources`/`SetPerms`/`KeepCapability`/`SetCapabilities`/`ServiceExec`（init_common_service.c）；`SetServiceEnterSandbox`（standard/init_service.c:400） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | pre-fork socket 需在 fork 前创建（ondemand 在解析期）；exec 前所有失败均为子进程退出+父进程记 `lastErrno`；高优先级启动走 `/dev/sched_state_report` ioctl 提升调度（`StartServiceByNameHighPriority`） |
| 并发约束 | fork 与 hook 全在 init 单线程；子进程从 fork 到 exec 之间只做自身设置，不反向调用 init 内部接口 |
| 安全约束 | 顺序（caps 保活→gid→seccomp→uid→caps）不可调换；`secon` 缺失时使用 `u:r:limit_domain:s0` 并告警（`services/modules/selinux/selinux_adp.c`）；seccomp 策略只允许从受控路径 dlopen 唯一匹配文件 |

## 服务监管与重启策略

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 系统服务会崩溃；不同重要度的服务需要不同的处置（不重启/延时重启/整机复位），且必须防止崩溃风暴 |
| 技术目标 | 用单线程 SIGCHLD 回收 + 时间窗计数实现分级保活，保证关键服务可用性的同时不陷入重启循环 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 信号经 signalfd 融入主循环：`SignalInit`（`services/init/standard/init_signal_handler.c:149`）注册 SIGCHLD/SIGTERM → `ProcessSignal`→`HandleSigChild` 循环 waitpid → `ServiceReap`（`services/init/init_common_service.c:1182`）→ `HandleServiceExit`（:1131）策略链：INVALID 忽略；NEED_STOP 清标志；once 无 NEED_RESTART 不重启；PERIOD 起定时器按 `period` 重启；ondemand 无 NEED_RESTART 走 `CheckOndemandService` 重新挂 socket watcher；critical 用 `CalculateCrashTime`（CLOCK_MONOTONIC 时间窗）判定，超限 `ExecReboot("panic")`；普通服务超限则 `ServiceReStartTimer(crashTime*1000)` 延后再试并清 crashCnt；否则 `ExecuteServiceRestart`（onrestart 命令 + on-restart job + ServiceStart）。停机：SIGTERM → 写 `startup.device.ctl="stop"` + `ExecReboot("reboot")`；`StopAllServices` 先 `StopAppSpawnBeforeReboot`（SIGTERM appspawn、5 秒等待）再逐个 ServiceStop |
| 关键流程 | `NotifyServiceChange`（standard/init_service.c:98）把状态写为参数 `startup.service.ctl.<name>` 与 `.pid`，供 samgr/begetctl 感知；ondemand 服务启动后写 oom_score_adj=-900 |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | cfg 保活字段：`once`、`critical`（`[enable, crashCount, crashTime]`，默认 [true,4,240]）、`period`、`importance`（standard 仅调度）、`ondemand`；`ServiceControl`/`StartServiceByTimer`/`ServiceWaitForStatus`/`ServiceSetReady`（`interfaces/innerkits/include/service_control.h`，经参数通道实现） |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `Service`（crashCnt/firstCrashTime/onDemandArgs） |
| 核心成员 | `attribute` 的 NEED_RESTART/NEED_STOP/PERIOD/CRITICAL/ONDEMAND 位 |
| 核心方法 | `SignalInit`/`ProcessSignal`/`HandleSigChild`（init_signal_handler.c）；`ServiceReap`/`HandleServiceExit`/`CalculateCrashTime`/`ServiceStartTimer`/`ServiceStop`（init_common_service.c）；`StopAllServices`/`GetKillServiceSig`（init_service_manager.c） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | critical 与 ondemand 互斥；`critical` 超限直接整机 panic（无法撤销）；lite 系统重要进程退出整机重启 |
| 并发约束 | 回收与重启决策在同一单线程内，无竞态；`ServiceStartTimer` 使用 loopevent timer，不可用阻塞 sleep |
| 安全约束 | `once/importance/critical` 默认值变更属产品级行为变更；`GetKillServiceSig` 对 appspawn 族的 SIGTERM 差异不可移除（应用需优雅退出） |

## 按需启动机制（ondemand）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 常驻服务全量开机拉起拖慢启动并占用内存；但服务 socket 路径又必须在服务启动前就可被客户端连接 |
| 技术目标 | init 预建 socket 并监听，服务真正被使用时才 fork；对 samgr 管理的 SA 提供参数触发接口 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 三个触发源：①socket 触发——ondemand + 连接型 socket 在解析期由 `CreateSocketForService` 创建并 `listen(fd,16)` + `AddSocketWatcher`（`LE_StartWatcher` WATCHER_ONCE），连接到达回调 `ServiceStart`（`services/init/init_service_socket.c`）；②参数触发——`start-mode=condition` 服务由 `ohos.servicectrl.*` 参数变化驱动（参数服务端识别控制前缀后走 trigger）；③console 触发——`WatchConsoleDevice`（init_service_manager.c:1203）监视 `/dev/console` 可读再启动。服务运行期退出且无 NEED_RESTART 时 `CheckOndemandService` 重新挂 watcher 等待下次触发；`StartOnDemandServiceByName` 支持 `"name|args"` 带参启动（`GetServiceByExtServName` 拆分，暂存 onDemandArgs） |
| 关键流程 | `start_ondemand` 命令与 begetctl `start_ondemand` 走 `StartOnDemandServiceByName`（init_service_manager.c:1434）：pid>0 时置 NEED_RESTART + 暂存参数待退出后重启；否则立即启动 |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | cfg：`"ondemand": true` + `socket` 字段（family/type/permissions/uid/gid/option）；运行期命令 `start_ondemand <name>`；samgr 侧经 libbegetutil `ServiceControl` 写 `ohos.ctl.start`/`ohos.ctl.stop` 参数（`ohos.servicectrl.` 前缀下另有 `timer_start`/`timer_stop`/`reboot.*` 等专用键） |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `ServiceSocket`（watcher 状态）；`Service.onDemandArgs` |
| 核心成员 | `attribute` 的 ONDEMAND 位 |
| 核心方法 | `CreateSocketForService`/`AddSocketWatcher`/`SetSocketEnvForService`（init_service_socket.c）；`StartOnDemandServiceByName`/`CheckOndemandService`/`WatchConsoleDevice`（init_service_manager.c / init_common_service.c） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | ueventd 的 netlink socket 不挂 watcher（无连接概念，特判服务名）；ondemand 服务不参与 boot job 拉起 |
| 并发约束 | watcher 回调在 init 单线程，`ServiceStart` 重入由 `pid>0` 判定挡住 |
| 安全约束 | socket 属主/权限由 cfg 定义并经 lchown/fchmodat 落盘，不可放宽以"方便调试" |

## 系统参数服务（trie 共享内存 + DAC/MAC + wait/watch）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 全系统需要一致的键值配置读取（高频、跨进程），同时写操作必须严格鉴权，且要支持"等到某参数变成某值"与"参数变化回调"两类异步语义 |
| 技术目标 | 读零 IPC（共享内存只读 mmap）、写单点鉴权（init 服务端）、异步等待与监听不占线程 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 存储：key 按 `.` 分段的二叉查找 trie（`ParamTrieNode`，`services/param/base/param_trie.c`），整体 mmap 于 `/dev/__parameters__`；每标签独立子空间，大小取自 `/etc/param/ohos.para.size`（`LoadParamAreaSize`）；默认区 `PARAM_WORKSPACE_MAX=80KB`、DAC 区 2MB。写路径：客户端 `SystemSetParameter`（`services/param/linux/param_request.c:214`）构造 MSG_SET_PARAM 连 `/dev/unix/socket/paramservice`（15 秒超时、2 次重试）→ `ProcessMessage`（param_service.c:349）→ `HandleParamSet` 以 SO_PEERCRED 取 ucred → `CheckParameterSet`（DAC：`param_dac.c` 按 `.para.dac` 前缀匹配；SELinux：`param_selinux.c`）→ `WriteParam`（commitId 原子递增）→ persist 前缀 `WritePersistParam` → `CheckAndSendTrigger`（清/查 TRIGGED/WAITED 标志并派发 trigger/wait/watch 事件）。wait：`HandleParamWaitAdd` 先查当前值，命中即回包；否则挂 WaitNode + 1ms 周期 timer 检查（默认超时 30 秒）。watch：SA 3901（`services/param/watcher/`）持有客户端回调，经 MSG_ADD_WATCHER/MSG_DEL_WATCHER 向 init 注册。读：`SystemReadParam`/`CachedParameterGet` 直接 mmap 查找，`ReadCommitId` 用 futex 等写完成 |
| 关键流程 | 特殊参数：`LoadSpecialParam`（param_service.c:458）从 cmdline/build 注入；控制前缀（`ohos.servicectrl.`/`ohos.ctl.start|stop=`/`ohos.startup.powerctrl=`）在 set 时联动服务控制或重启；`SystemSaveParameters` 要求 uid 在 servicectrl 组（`CheckIfUidInGroup`） |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | `interfaces/innerkits/include/param/init_param.h`：`SystemSetParameter/SystemSetParameterNoWait`、`SystemGetParameter`（`SystemReadParam`）、`SystemWaitParameter`、`SystemCheckParamExist`、`CachedParameterCreate/Get/Destroy`、`SystemSaveParameters`；C++/JS 封装见 `syspara/parameter.h`、jskits；监听 `WatchParamCheck`/`SystemWatchParameter`（watcher_manager_kits.cpp:335，走 SA 3901）；产品信息 `GetSerial/GetDevUdid/GetOSFullName` 等（`syspara/`） |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `WorkSpace`/`ParamTrieHeader`/`ParamTrieNode`/`ParamNode`/`ParamSecurityNode`（`services/param/include/param_common.h`）；`ParamMessage`/`ParamMsgContent`/`ParamResponseMessage`（`services/param/linux/param_message.h`） |
| 核心成员 | commitId 标志位（MODIFY/TRIGGED/WAITED/PERSIST）；DAC 模式位（读 0x0100/写 0x0080/watch 0x0040） |
| 核心方法 | `InitParamService`/`StartParamService`/`SystemSetParam`/`ProcessMessage`/`HandleParamSet`/`HandleParamWaitAdd`（param_service.c）；`WriteParam`/`FindParam_`（param_trie.c）；`ParamServiceStart`→`LE_RunLoop`（param_msgadp.c:143）；`SystemSetParameter`/`SystemWaitParameter`（param_request.c）；`BatchSavePersistParam`（manager/param_persist.c:97） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | key/value ≤96/96 字节、const 值 ≤4096；每标签空间有限（写满报 `PARAM_CODE_REACHED_MAX=103`/`PARAM_CODE_MEMORY_NOT_ENOUGH=115`）；wait 默认 30 秒超时、set 15 秒；persist 批量落盘在 `/data` 就绪后 |
| 并发约束 | 写单点（init 单线程）；读端多进程并发只读，靠 commitId 原子量 + futex 一致性；客户端库用 mutex 保护长连接 fd |
| 安全约束 | DAC+MAC 双检不可绕过或弱化；`const.`/`ro.`/`ohos.boot.` 只读；JS 应用侧默认仅 get/watch；错误码 1001（DAC_RESULT_FORBIDED）/1002（SELINUX_RESULT_FORBIDED）表示拒绝 |

## 触发器引擎（trigger/条件 job）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 启动流程需要"事件驱动"：参数达到某值才执行某组命令（如 `sys.boot_completed=true` 后启动依赖服务），同步等待会阻塞主循环 |
| 技术目标 | 把 boot 阶段推进与参数条件判定统一为 trigger 队列模型，异步、串行、可观测 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | `InitTriggerWorkSpace`（`services/param/trigger/trigger_processor.c:307`）创建 async event task（`ParamEventTaskCreate`）+ 执行队列（容量 64）。事件类型 EVENT_TRIGGER_BOOT/EVENT_TRIGGER_PARAM/EVENT_TRIGGER_PARAM_WAIT/EVENT_TRIGGER_PARAM_WATCH 经 `PostTrigger`→`SendTriggerEvent` 投递；`CheckTrigger` 匹配：boot 类按名字，param 类按 `param:<key>=<value>` 条件（最多 64 个子条件）评估；命中入队后 `ExecuteQueueWork`（单轮 ≤20 条）逐个 `DoTriggerExecute_`→`DoCmdByIndex`。服务批量拉起用 `AddCompleteJob` 动态构造 `boot-service:service-<mode>-<num>` job（`TriggerServices`，每 4 个服务一组） |
| 关键流程 | 启动收尾 `PostInitTriggers`（standard/init.c:448）：pre-init → init → boot 服务 → post-init → normal 服务；重启前 `DoJobExecNow` 同步执行并触发 INIT_SHUT_DETECTOR hook |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | cfg `jobs` 数组（name/condition/cmds）；命令 `trigger <jobName>`；模块侧 `DoJobNow`（init_module_engine.h）；`AddCompleteJob` 供内部动态建 job |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `Trigger`/`Command`/`TriggerWorkSpace`（`services/param/include/trigger_manager.h`） |
| 核心成员 | trigger flags（QUEUE/RELATED/ONCE/SUBTRIGGER）；`TRIGGER_MAX_CMD=4096` |
| 核心方法 | `ParseTriggerConfig`/`CheckTrigger`/`ExecuteQueueWork`/`PostTrigger`/`AddCompleteJob`/`DoJobExecNow`（trigger_processor.c）；命令分发 `DoCmdByIndex`（init_common_cmds.c:1014） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | lite 系统无此引擎（同步 DoJob）；命令执行耗时 >200ms 打日志，长命令应拆分 |
| 并发约束 | 队列消费串行；trigger 内不得调用阻塞等待参数接口（`SystemWaitParameter` 面向客户端进程） |
| 安全约束 | 条件 job 的条件参数必须可读，否则永久不满足（表现为"job 不跑"故障） |

## 事件循环框架（loopevent）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | init 需要同时处理 socket 服务、信号、定时器、idle 任务，且全部必须在单线程内非阻塞完成 |
| 技术目标 | 提供统一的对象化事件循环原语，成为 init/param（及 appspawn 复用）的调度基座 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 对象模型：`LoopBase` 派生体系，`LoopHandle/TaskHandle/TimerHandle/SignalHandle/WatcherHandle` 均为 `LoopBase*`（`interfaces/innerkits/include/loop_event.h:55-64`）。默认单例 `g_defaultLoop` 懒创建（`LE_GetDefaultLoop`，`services/loopevent/loop/le_loop.c:140`）；`LE_RunLoop` 进入 epoll 循环。任务类型 flags：STREAM/TCP/PIPE/SERVER/CONNECT/EVENT/ASYNC_EVENT/TIME/SIGNAL/WATCHER。信号任务基于 signalfd（`LE_ProcessSignal(const struct signalfd_siginfo*)` 回调）；watcher 支持 `WATCHER_ONCE`；流任务带 `LE_Send` 发送缓冲 |
| 关键流程 | init 主循环装配：`SystemRun`→`StartParamService`→`ParamServiceStart`→`LE_RunLoop(LE_GetDefaultLoop())`；参数 wait 超时检查、服务定时器、console watcher、socket accept 全部以任务形式挂载 |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | `LE_CreateLoop/LE_GetDefaultLoop/LE_RunLoop/LE_StopLoop`；流式：`LE_CreateStreamServer/LE_CreateStreamClient/LE_AcceptStreamClient/LE_Send`；异步事件：`LE_CreateAsyncTask/LE_StartAsyncEvent`；定时器：`LE_CreateTimer/LE_StartTimer/LE_StopTimer`；信号：`LE_CreateSignalTask/LE_AddSignal/LE_RemoveSignal`；观察：`LE_StartWatcher/LE_RemoveWatcher`；空闲：`LE_AddIdle/LE_DelIdle/LE_DelayProc`。状态码 `LE_STATUS`（LE_SUCCESS=0，LE_FAILURE=10000 起） |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `EventLoop`（taskMap hashmap + mutex）、`BaseTask`（`services/loopevent/task/le_task.h`） |
| 核心成员 | `LOOP_MAX_SOCKET=1024`、`LOOP_DEFAULT_BUFFER=5KB`、`LOOP_MAX_BUFFER=64KB`、`LOOP_MAX_CLIENT=1024` |
| 核心方法 | `le_epoll.c`（epoll 封装）、`le_signal.c`（signalfd）、`le_socket.c`、`le_timer.c`、`le_idle.c`、`le_asynctask.c` |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 依赖 Linux（epoll/signalfd）；单 loop socket 上限 1024 |
| 并发约束 | loopevent 内部仅 mutex 保护 taskMap，不建线程；任务回调不可阻塞（同 init 单线程约束） |
| 安全约束 | 不适用（基础设施无权限面） |

## 插件与钩子机制（modulemgr/hookmgr）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | bootchart/bootevent/seccomp/selinux/trace/udid 等特性并非所有产品都需要，硬编码进核心会让裁剪困难、核心路径膨胀 |
| 技术目标 | 核心流程预留确定性扩展点（bootstage hook + 命令执行器 + 动态库目录扫描），可选能力全部后置到模块 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 两层引擎：①hookmgr（`interfaces/innerkits/include/hookmgr.h`）：`HookMgrCreate/Add(stage,prio,hook,cookie)/Execute/Traversal`，`HOOK_INFO` 按 stage+优先级排序；bootstage 封装（`interfaces/innerkits/init_module_engine/include/bootstage.h`）定义 `INIT_BOOTSTAGE` 全集（GLOBAL_INIT=0、FIRST_STAGE=1、...、PRE_PARAM_SERVICE=10、PRE_PARAM_LOAD=20、PRE_CFG_LOAD=30、SERVICE_FORK_BEFORE=58、SERVICE_SET_PERMS=59/60、SERVICE_FORK_AFTER=61、SERVICE_REAP=65、BOOT_COMPLETE=100 等）与 `InitAdd*Hook` 内联注册函数，`GetBootStageHookMgr()`（`services/init/bootstagehooker.c`）为全局实例。②modulemgr（`interfaces/innerkits/include/modulemgr.h`）：`MODULE_CONSTRUCTOR`（`__attribute__((constructor))`）让模块加载即注册；`ModuleMgrInstall` 从 `/system/lib/<name>/` 加载 `<module>.z.so`。命令扩展：`AddCmdExecutor(cmdName, executor)` 注册插件命令（实现 `AddCmdExecutor_`，`services/init/standard/init_cmdexecutor.c:29`），索引编码 `(hash+1)<<16|cmdId`；核心用 `PluginExecCmdByName("loadSelinuxPolicy",...)` 等调用 |
| 关键流程 | 模块安装点：selinux 插件 `InitModuleMgrInstall("selinuxadp")`（selinux_static.c:25）；模块管理命令 `modulectl list/install/uninstall`（begetctl→control_fd ACTION_MODULEMGR） |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | `interfaces/innerkits/init_module_engine/include/init_module_engine.h`：`AddCmdExecutor/RemoveCmdExecutor/DoJobNow/GetParameterFromCmdLine/LoadParamsFile`；`init_modulemgr.h`：`InitModuleMgrInstall/UnInstall/AutorunModuleMgrUnInstall/InitModuleMgrDump`；`bootstage.h` 的 `InitAdd*Hook` 系列 |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `HOOK_INFO`/`HookMgr`（hookmgr.h）；`PluginCmd`/`PluginCmdExecutor`（init_cmdexecutor.h） |
| 核心成员 | bootstagehooker 全局 HookMgr；group cmds hashmap（NODE_TYPE_CMDS） |
| 核心方法 | `InitModuleMgrInstall`（modulemgr）；`AddCmdExecutor_`/`PluginExecCmdByName`/`PluginGetCmdIndex`（init_cmdexecutor.c）；`HookMgrExecute`（hookmgr） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 插件命令参数上限 `MAX_CMD_ARGC=10`；bootstage 顺序是核心启动序的一部分，不可重排既有 stage |
| 并发约束 | hook 执行在 init 单线程；模块构造函数在 dlopen 时同步执行，不得阻塞 |
| 安全约束 | 模块从固定目录受控加载；`init_extra_static_modules` 等注入面由产品配置掌控，仓内默认值不可擅改 |

## Seccomp 策略机制

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 原生服务与应用若可调用全部内核 syscall，攻击面过大；策略需要按进程粒度差异化且不能拖慢启动 |
| 技术目标 | 构建期把文本策略编译为 BPF 过滤库，运行期按服务名装载，实现 syscall 白名单/黑名单与违规处置 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 策略文件（`services/modules/seccomp/seccomp_policy/*.seccomp.policy`，16 个）以节组织：`@labelName/@priority/@allowList/@blockList/@priorityWithArgs/@allowListWithArgs/@headFiles/@selfDefineSyscall/@returnValue/@mode/@privilegedProcessName/@allowBlockList`，行格式 `name;arch`（arm/arm64/riscv64/x86_64/all）。`scripts/generate_code_from_policy.py` 用 BPF 宏（BPF_JGE/JGT/JEQ/LOAD/RET）生成 C 源并编译为 `lib<name>_filter.z.so`。运行期 `SetSeccompPolicyWithName(type, filterName)`（`services/modules/seccomp/seccomp_policy.c:211`）：`GetFilterFileByName`（`lib/seccomp/lib%s_filter.z.so`，config_policy 查找且只接受唯一路径）→ dlopen + dlsym `g_<name>SeccompFilter` → `InstallSeccompPolicy`（`syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, TSYNC|LOG, &prog)`，flag 能力探测 `IsSupportFilterFlag`）。类型回退：SYSTEM_SA（服务名 system 或路径 sa_main）找不到专属 filter 回退 `system`；SYSTEM_OTHERS 找不到即跳过；filter 类型 SYSTEM_SA/SYSTEM_OTHERS/APP/INDIVIDUAL（`interfaces/innerkits/seccomp/include/seccomp_policy.h`）。`privileged_process.seccomp.policy` 用 `@privilegedProcessName app_custom` + `@allowBlockList setuid;arm64...` 放行特定高权限进程的高危调用 |
| 关键流程 | 装载时机：`SetPerms` 中 `SetSystemSeccompPolicy`（跳过 appspawn/nwebspawn——它们自带应用策略）在 setuid 之前调用；失败 `exit(PROCESS_EXIT_CODE=0x7f)` |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | `SetSeccompPolicyWithName`（innerkits seccomp_policy.h）；调试开关 `persist.init.debug.seccomp.enable`（WITH_SECCOMP_DEBUG）；辅助工具 `scripts/tools/{strace_log_analysis,audit_log_analysis,collect_elf_syscall,libc_static_analysis,merge_policy}.py` |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | 过滤器生成物 `lib*_filter.z.so`（符号 `g_<name>SeccompFilter`/`Size`） |
| 核心成员 | `@returnValue`（KILL_PROCESS/KILL_THREAD/TRAP/ERRNO/USER_NOTIF/TRACE/LOG/ALLOW）；`@mode`（DEFAULT/ONLY_CHECK_ARGS） |
| 核心方法 | `SetSeccompPolicyWithName`/`GetFilterFileByName`/`InstallSeccompPolicy`（seccomp_policy.c）；`SetSystemSeccompPolicy`（seccomp_policy_static.c，经 `AddCmdExecutor("SetSeccompPolicy",...)` 暴露给 init 核心）；生成器 `scripts/generate_code_from_policy.py`、模板 `scripts/seccomp_policy_fixer.gni` |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 只在 Linux 标准系统生效；策略与架构绑定（arch 段） |
| 并发约束 | 装载发生在 fork 后、exec 前的子进程上下文，TSYNC 保证线程组一致 |
| 安全约束 | 禁止手改生成的 `.so`；`blocklist` 基线（`app.blocklist`/`system.blocklist`）与 `privileged_process` 名单变更需安全评审；`returnValue` 从 KILL 改 TRAP/LOG 属于攻击面变更 |

## 沙箱机制（namespace + pivot_root）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | system 与 chipset 两域服务的可见挂载面需要收敛（尤其芯片侧目录不得反向污染主域），同时不能影响其余进程的挂载视图 |
| 技术目标 | 用私有 mount namespace + pivot_root 为每域构建独立根，并允许单个服务按需切入 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | `InitSandbox`（`services/sandbox/sandbox.c:389`）：`UnshareNamespace(CLONE_NEWNS)` + `GetNamespaceFd("/proc/self/ns/mnt")` 保存句柄 + 解析 json 到挂载链表。`PrepareSandbox`（:540）：mkdir rootPath → `mount(NULL,"/",NULL,MS_REC|MS_SLAVE,NULL)`（断开传播）→ rootPath 自 bind → 逐项 bind mount path/file 列表（flags 表 bind/private/rec/move）→ symlink 列表 → `chdir(rootPath)` → `syscall(SYS_pivot_root, rootPath, rootPath)` → `umount2(".",MNT_DETACH)`。进入既有沙箱：`EnterSandbox(name)`（:625）`SetNamespace(sandbox->ns, CLONE_NEWNS)`。服务粒度：cfg `sandbox:1` + 全局开关参数 `const.sandbox`（`IsEnableSandbox`）→ `SetServiceEnterSandbox`（standard/init_service.c:400）。sub-init 上下文配合 `DoUmountOtherNsData`（standard/init_cmds.c:838）在对应沙箱内卸载 `/data` |
| 关键流程 | `mksandbox system|chipset` 命令（`DoMkSandbox`，standard/init_cmds.c:1225）：进默认 ns → 构建 → 回默认 ns |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | json 配置（`etc/sandbox/system-sandbox.json`、`chipset-sandbox.json`，tag：`sandbox-root`/`mount-bind-paths`/`mount-bind-files`/`symbol-links`）；命令 `mksandbox`；调试 `begetctl sandbox`（control_fd ACTION_SANDBOX）与 `DumpSandboxByName` |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `sandbox_t`/`mountlist_t`（`services/sandbox/include/sandbox.h`）；实例 `g_systemSandbox`/`g_chipsetSandbox` |
| 核心成员 | `ns`（namespace fd）、`isCreated`、`rootPath` |
| 核心方法 | `InitSandboxWithName/InitSandbox/PrepareSandbox/EnterSandbox/DestroySandbox/DumpSandboxByName`（sandbox.c）；`InitDefaultNamespace/GetNamespaceFd/UnshareNamespace/SetNamespace`（sandbox_namespace.c）；`DoMkSandbox`（standard/init_cmds.c:1225） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 需要 root 与 mount 权限；pivot_root 期间进程 cwd 必须在 rootPath 内；64 位变体 json（`*64.json`）与主 json 并存 |
| 并发约束 | 构建与进入都在 init 单线程或被 fork 子进程内；namespace fd 由 init 持有 |
| 安全约束 | 沙箱 json 属挂载面定义，修改影响域隔离（对应 AGENTS 约束）；`sandbox:0` 是显式豁免，不可默认化 |

## fd 代持机制（fd_holder）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 服务重启（升级/崩溃重启）会丢失已打开的 fd，客户端连接断开、状态丢失 |
| 技术目标 | init 作为可信第三方在服务退重启间隙保管 fd，重启后原样归还 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 服务端 `services/init/standard/fd_holder_service.c`：socket `/dev/unix/socket/fd_holder`（SOCK_DGRAM + SO_RCVBUFFORCE 4096 + SO_PASSCRED，root:root 0660）。协议：SCM_RIGHTS 传 fd，消息体 `serviceName|action|poll`（action=hold/get，`HandlerFdHolder` :118）。权限：`CheckFdHolderPermission`（:91）以 ucred 校验请求方 pid 必须等于服务自身 pid——只允许代持本服务 fd。存储：`service->fds/fdCount`（`UpdaterServiceFds`，init_common_service.c:1201），上限 `MAX_HOLD_FDS=64`。归还：服务重启后 `PublishHoldFds`（init_common_service.c:409）经环境变量 `OHOS_FD_HOLD_<serviceName>` 传回 |
| 关键流程 | hold：服务运行中调 `ServiceSaveFd`；get：重启后的新实例调 `ServiceGetFd`；服务彻底停止时 `CloseServiceFds` 关闭全部持有 fd |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | `interfaces/innerkits/include/fd_holder.h`：`ServiceSaveFd/ServiceSaveFdWithPoll/ServiceGetFd`；协议常量在 `interfaces/innerkits/fd_holder/fd_holder_internal.h`（`ACTION_HOLD/ACTION_GET/WITHPOLL`，`MAX_HOLD_FDS=64`） |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `Service.fds/fdCount` |
| 核心成员 | `INIT_HOLDER_SOCKET_PATH`、`FD_HOLDER_BUFFER_SIZE` |
| 核心方法 | `FdHolderSockInit`（standard/init.c:188）、`HandlerFdHolder`/`CheckFdHolderPermission`/`BuildControlMessage`/`ReceiveFds`（fd_holder_service.c）；`UpdaterServiceFds`/`PublishHoldFds`/`CloseServiceFds`（init_common_service.c） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | 仅支持本服务 fd；poll 选项当前未实现（仅日志）；重启期间 init 崩溃则 fd 丢失 |
| 并发约束 | 请求处理在 init 单线程，SCM_RIGHTS 原子送达 |
| 安全约束 | ucred 校验不可移除；socket 权限 0660 root:root 不可放宽（否则任意进程可窃取/塞入 fd） |

## 调试与控制通道（control_fd + begetctl + 参数控制）

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | init 内部状态（服务表、参数、trigger、loop 任务、模块列表）无 Binder 可查；沙箱等操作需要以 init 身份执行；产品/开发需要 CLI 与程序化控制入口 |
| 技术目标 | 提供带鉴权的带外控制 socket 与命令行工具，覆盖 dump、沙箱进入、模块管理、服务控制与重启 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | ①control_fd：`InitControlFd`（`services/init/standard/init_control_fd_service.c:291`）在 `/dev/unix/socket/init_control_fd` 上提供 `CmdMessage{type,ptyName,cmd}` 处理，动作 ACTION_SANDBOX（`ProcessSandboxControlFd`→`EnterServiceSandbox`）、ACTION_DUMP（`ProcessDumpServiceControlFd`→`DumpOneService/DumpAllServices/DumpAllExtData/SystemDumpTriggers/SystemDumpParameters/DumpLoop`）、ACTION_MODULEMGR（`InitModuleMgrDump`）、ACTION_APP_SPAWNTIME、ACTION_APP_SANDBOX（PTY 交互）。②begetctl（`services/begetctl/`）：mini shell（`BShellEnv*`）+ `MODULE_CONSTRUCTOR` 注册子命令（param ls/get/set/wait、service_control、sandbox、bootchart、modulectl、reboot、dump_service、setloglevel、partitionslot、paramshell 等）。③服务控制：`interfaces/innerkits/service_control/service_control.c` 把 `ServiceControl`/`StartServiceByTimer`/`ServiceWaitForStatus`/`ServiceSetReady` 全部落到参数通道（`ohos.servicectrl.timer_start`、`startup.service.ctl.<name>` 等） |
| 关键流程 | dump_service all → control_fd ACTION_DUMP → init 主循环内拼装服务表输出；begetctl sandbox app → ACTION_APP_SANDBOX + PTY |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | `interfaces/innerkits/control_fd/control_fd.h`（`CmdServiceInit`、ActionType）；`GetControlSocket`（init_socket.h）；inner-kits：`ServiceControl/ServiceWaitForStatus/StartServiceByTimer`（service_control.h）、`DoReboot/DoRebootExt`（init_reboot.h） |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `CmdMessage`/`ActionType`（control_fd.h：66-73） |
| 核心成员 | `PTY_PATH_SIZE=128` |
| 核心方法 | `InitControlFd`/`ProcessControlFd`/`ProcessSandboxControlFd`/`ProcessDumpServiceControlFd`（init_control_fd_service.c）；begetctl 各 cmd 文件（`main.c`、`param_cmd.c`、`service_control.c`、`sandbox.cpp`、`dump_service.c`、`modulectl.c`、`init_cmd_reboot.c` 等） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | dump/控制动作在 init 单线程内执行，超大输出会占用主循环（谨慎频繁全量 dump） |
| 并发约束 | 单客户端请求串行处理；PTY 模式占用交互会话 |
| 安全约束 | control_fd 与 fd_holder socket 权限不可放宽；`reboot updater/flashd`、`partitionslot setactive`、`param set persist.*` 属破坏性命令，仅在任务明确要求时执行 |

## ueventd 设备节点管理

### 解决的问题（Problem）
| 子维度 | 内容 |
|---|---|
| 业务痛点 | 用户态需要动态响应内核设备热插拔：创建/权限修正设备节点、维护分区别名链接、处理固件加载事件 |
| 技术目标 | 以独立服务常驻监听 netlink，把设备事件转换为设备节点、by-name 链接与参数通知 |

### 原理（Principles）
| 子维度 | 内容 |
|---|---|
| 核心机制 | 入口 `ueventd_main.c` `main()`（:179）支持 `-d`（daemon，默认）/`-r`（retrigger）/`-b`（early-boot）/`-l`（listen）。daemon 流程 `UeventdDaemon`（ueventd/ueventd_main.c:92）：解析 `/etc/ueventd.config`、`/vendor/etc/ueventd.config`、`/vendor/etc/ueventd_factory.config` → ondemand 时 `GetControlSocket("ueventd")` 取 init 预建 netlink socket，否则 `UeventdSocketInit`（PF_NETLINK/SOCK_DGRAM|CLOEXEC|NONBLOCK/NETLINK_KOBJECT_UEVENT，`nl_groups=0xffffffff`，SO_RCVBUFFORCE 256KB，SO_PASSCRED）→ 第一次运行（无 `/dev/.ueventd_trigger_done` 标志）`RetriggerUevent` 重放存量设备 → `ProcessUevent` 循环 → ondemand 空闲轮询（`UEVENTD_POLL_TIME=5 分钟`，`bootevent.boot.completed==true` 后超时退出）。事件处理 `HandleUevent`（:110）按 subsystem 分发：block→`HandleBlockDeviceEvent`（ueventd_device_handler.c:484）、firmware→`HandleFimwareDeviceEvent`（写 `/sys<syspath>/loading="-1"` 取消内核等待）、其余→`HandleOtherDeviceEvent`；add/change/online 先 `ChangeSysAttributePermissions` |
| 关键流程 | 设备节点：`CreateDeviceNodeWithPermissions`（:148）查 `GetDeviceNodePermissions` → mknod → chown/chmod；by-name：`CreateSymbolLinks`（:55）+ block 处理中生成 `/dev/block/platform/<ctrl>/by-name/<part>` 与 `/dev/block/by-name/<part>`（:292/:303）；SELinux `SetDeviceLable`（:95）逐级 restorecon 并把链接 label 复制到节点；参数通知：工作线程 `SystemSetParameter("startup.uevent.<parameter>","added"/"removed")`（ueventd/standard/ueventd_parameter.c:93，5 秒重试）；early-boot 直接 mknod 必需动态节点（tty/binder/console 等） |

### 使用方式（Usage）
| 子维度 | 内容 |
|---|---|
| 核心 API | 配置接口：`ueventd.config` INI（`[device]/[sysfs]/[firmware]` 段，行 `<name> <mode> <uid> <gid> [parameter]`）；消费接口：`startup.uevent.*` 参数（存储管理等监听）；启动联动：init 第一阶段 `StartUeventd` |

### 代码关联（Code）
| 子维度 | 内容 |
|---|---|
| 核心类 | `DeviceUdevConf`（ueventd_read_cfg.h:27）；uevent 缓冲（`UEVENT_BUFFER_SIZE` 2048+1） |
| 核心成员 | `UEVENTD_FLAG`（`/dev/.ueventd_trigger_done`） |
| 核心方法 | `UeventdSocketInit`（ueventd_socket.c:28）、`ProcessUevent`/`HandleUevent`/`RetriggerUevent`/`HandleRequiredDynamicDeviceNodes`（ueventd.c）、`HandleBlockDeviceEvent`/`CreateDeviceNodeWithPermissions`/`CreateSymbolLinks`/`SetDeviceLable`（ueventd_device_handler.c）、`HandleFimwareDeviceEvent`（ueventd_firmware_handler.c:27）、`SetUeventDeviceParameter`（ueventd_parameter.c:122） |

### 约束与限制（Constraints）
| 子维度 | 内容 |
|---|---|
| 系统限制 | netlink socket 由 init 预建（ueventd.cfg socket 段，AF_NETLINK），ueventd 以 ondemand 方式运行；early-boot 阶段节点集合固定（`DYNAMIC_DEVICES[]`） |
| 并发约束 | netlink 处理单线程；参数通知是 ueventd 内唯一工作线程（与 init 单线程约束不冲突：不同进程） |
| 安全约束 | 设备节点权限表（config）即攻击面定义：mode/uid/gid 放宽=越权；by-name 链接的 SELinux label 复制逻辑不可破坏 |
