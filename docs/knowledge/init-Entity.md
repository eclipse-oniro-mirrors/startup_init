# 业务实体知识

init 部件核心业务实体的完整知识定义，涵盖静态概念、动态运行、规则约束与代码映射。

## 服务（Service）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 服务（Service，init 管理的原生系统服务进程） |
| 实体定义 | 由 cfg `services` 数组声明、init fork 出并持续监管的用户态进程单元；init 与其关系是"父进程 + 监管者"，实体身份 = 服务名（全局唯一）+ pid（运行期） |
| 核心特征 | 运行态结构 `Service`（`services/init/include/init_service.h:185`）：`name`、`path[]`（≤`MAX_PATH_ARGS_CNT=20`）、`pid`、`status`（ServiceStatus）、属性位 `attribute`、`crashCnt/firstCrashTime`、`onDemandArgs`、`fds/fdCount`；状态枚举 `ServiceStatus`（`interfaces/innerkits/include/service_control.h:30`）：IDLE/STARTING/STARTED/READY/STOPPING/STOPPED/ERROR/SUSPENDED/FREEZED/DISABLED/CRITICAL |
| 类型/分类 | boot（start-mode=boot）/normal（start-mode=normal）/condition（start-mode=condition，按需）三类拉起时机；ondemand 与非 ondemand；critical 与普通 |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 创建：`ParseAllServices`→`ParseOneService`（`services/init/init_service_manager.c:1088`）解析 cfg → `AddService`（:92，置 IDLE、pid=-1）；启动：`TriggerServices`/`StartServiceByName`→`ServiceStart`（`services/init/init_common_service.c:902`）→ fork → 子进程 `RunChildProcessInternal`（:767）→ `ServiceExec`（execv）；退出：SIGCHLD → `HandleSigChild`→`ServiceReap`（:1182）→`HandleServiceExit`（:1131）按策略处置 |
| 状态流转 | IDLE →（ServiceStart fork）→ STARTING →（exec 成功、`NotifyServiceChange(SERVICE_STARTED)`）→ STARTED →（SetServiceReady 写 `startup.service.ctl.<name>`=READY）→ READY；STOP 命令/`ohos.ctl.stop` → STOPPING（NEED_STOP）→ STOPPED；崩溃 → 按 once/critical/period/ondemand 分支重启或保持退出；路径失效置 `SERVICE_ATTR_INVALID` |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | 名字唯一（group hashmap 查重，`CheckNodeValid`）；`critical` 与 `ondemand` 互斥（`GetServiceOnDemand` 校验）；critical 默认 240 秒 4 次退出触发整机 panic（`CRITICAL_DEFAULT_CRASH_COUNT=4`/`CRASH_DEFAULT_CRASH_TIME=240`，init_service_manager.c:52-54）；once=1 不重启；`period` 周期重启（毫秒=配置值×BASE_MS_UNIT）；stop 时 appspawn 族收 SIGTERM、其余 SIGKILL（`GetKillServiceSig`）；`"name|extraArg"` 形式支持带参按需启动（`GetServiceByExtServName`） |
| 并发约束 | 全部生命周期操作发生在 init 单线程事件循环内，无锁；外部只能经参数（`ohos.servicectrl.*`）或 control_fd 间接触发；子进程在 fork 后、exec 前的属性设置序列固定（见 SetPerms） |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | `InitWorkspace` 的 services hashmap（`InitServiceSpace`，`services/init/init_group_manager.c:202`）；group cfg 的 services 列表约束可见性 |
| 下游影响 | 状态变化写参数 `startup.service.ctl.<name>`/`.pid`（samgr/begetctl 消费）；服务退出驱动重启策略与 `INIT_SERVICE_REAP` hook；`INIT_SERVICE_FORK_AFTER` hook 供 bootevent 统计 |
| 平级关联 | ServiceSocket（pre-fork socket 归属）、Job（on-start/on-stop/on-restart/ pre-start 生命周期 job）、fd_holder（退重启间代持 fd）、Sandbox（`sandbox:1` 进入沙箱） |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `struct Service`（`services/init/include/init_service.h`）；属性位 SERVICE_ATTR_ONCE/NEED_RESTART/NEED_STOP/IMPORTANT/CRITICAL/DISABLED/CONSOLE/ONDEMAND/...（同文件 :41-63） |
| 核心方法 | `AddService`/`GetServiceByName`/`ParseOneService`（init_service_manager.c）；`ServiceStart`/`ServiceStop`/`ServiceReap`/`SetPerms`/`RunChildProcessInternal`（init_common_service.c）；`StartServiceByName`/`StopAllServices`（init_service_manager.c:1334/:1608） |

## 作业与触发器（Trigger/Job）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 触发器（Trigger，运行期）/ 作业（job，cfg 声明形态） |
| 实体定义 | 一组带条件与命令序列的启动单元；cfg 中的 job 静态声明在加载时转为 trigger 节点，运行期按事件（boot 阶段/参数变化）入队执行 |
| 核心特征 | `struct Trigger`（`services/param/include/trigger_manager.h`）：`flags`（QUEUE/RELATED/ONCE/SUBTRIGGER）、`type`（TRIGGER_BOOT/TRIGGER_PARAM/TRIGGER_UNKOWN）、`condition`、命令链表（每 job 命令数上限 `MAX_CMD_CNT_IN_ONE_JOB=200`，单 trigger 命令容量 `TRIGGER_MAX_CMD=4096`）、`triggerCount` |
| 类型/分类 | TRIGGER_BOOT：保留名（pre-init/init/post-init/early-fs/reboot/suspend 等 14 个，`triggerTypeStr[]` 全集见 `services/param/trigger/trigger_processor.c`）与 `boot-service:` 动态 job；TRIGGER_PARAM：`param:<key>=<value>` 条件 job；另有内部等待/观察命令索引 `CMD_INDEX_FOR_PARA_WAIT/WATCH` |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 创建：`ParseAllJobs`→`ParseTriggerConfig`（`services/param/trigger/trigger_processor.c:275`）按 `CheckJobValid` 校验后建 trigger；触发：`PostTrigger(EventType,...)`→`SendTriggerEvent`→`CheckTrigger` 匹配（条件 job 由参数变化事件匹配）→ 入执行队列（容量 `TRIGGER_EXECUTE_QUEUE=64`）→ `ExecuteQueueWork`（单轮 ≤`MAX_TRIGGER_COUNT_RUN_ONCE=20`）→ `DoCmdByIndex` 逐条执行；立即执行：`DoJobExecNow`（reboot 前触发 INIT_SHUT_DETECTOR hook） |
| 状态流转 | 空闲 → 入队（flags QUEUE）→ 执行中 → 完成（一次性 job 置 ONCE 后不再触发）；参数类 trigger 等待条件满足期间被参数事件反复匹配 |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | 条件 job 的 condition 最多 `MAX_CONDITION_NUMBER=64` 个子条件；`trigger` 命令可显式激活具名 job；lite 系统无 trigger 引擎（`DoJob` 同步执行，`services/init/lite/init_jobs.c`）；条件中引用的参数必须可读（DAC），否则永不满足 |
| 并发约束 | trigger 队列消费在 init 单线程 async event task（`InitTriggerWorkSpace`，trigger_processor.c:307）内串行；不得在 hook/命令中同步等待其他 trigger 完成 |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | TriggerWorkSpace（事件任务 + 执行队列），挂在 param 服务初始化路径（`InitParamService`） |
| 下游影响 | 命令执行（`DoCmdByIndex` → 通用/标准/插件命令表）；服务拉起（start/start_ondemand 命令 → ServiceStart） |
| 平级关联 | 系统参数（条件 job 的判定来源）；GroupNode（job 可见性） |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `struct Trigger`/`struct Command`（trigger_manager.h）；`TriggerWorkSpace` |
| 核心方法 | `ParseTriggerConfig`/`PostTrigger`/`CheckTrigger`/`ExecuteQueueWork`/`DoTriggerExecute_`（trigger_processor.c）；`AddCompleteJob`（:385，动态 job）；`DoJobNow`（`interfaces/innerkits/init_module_engine/include/init_module_engine.h`，模块可用） |

## 系统参数（ParamNode）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 系统参数（键值对实体，ParamNode；安全标签 ParamSecurityNode） |
| 实体定义 | 全系统共享的键值数据单元，key 按 `.` 分段存入共享内存 trie；身份 = key（≤96 字节）；值随标签类型有不同上限与可见性 |
| 核心特征 | `struct ParamNode`（`services/param/include/param_common.h`）：`commitId`（u32 原子）、`type`、`keyLength/valueLength`、`data[]`（存 "key=value"）；`ParamSecurityNode`：`uid/gid/selinuxIndex/mode/type/memberNum/members[]`；commitId 标志位：MODIFY/TRIGGED/WAITED/PERSIST/COMMITID |
| 类型/分类 | 标签三类：`const.`（只读、值上限 4096）、`persist.`（跨重启）、普通可写（值上限 96）；类型 string/int/bool（`GetParamMaxLen`：96/32/8，`services/param/base/param_trie.c:404`） |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 创建/更新：客户端 `SystemSetParameter` → paramservice socket MSG_SET_PARAM → `SystemSetParam`（`services/param/linux/param_service.c:139`）→ `CheckParameterSet`（DAC+MAC）→ `WriteParam`（trie 写入、commitId 原子递增）→ persist 前缀再 `WritePersistParam` → `CheckAndSendTrigger`（触发条件 job/wait/watch 事件）；读取：客户端 mmap 只读工作区（`services/param/linux/param_request.c`），`SystemReadParam`/`CachedParameterGet`；删除不适用（参数一经写入仅可改值） |
| 状态流转 | 不存在 → 存在（首次 set）→ 值更新（commitId 递增、清 TRIGGED/WAITED 标志）→（persist）批量落盘；写中状态由读端 futex 等 commitId 稳定感知 |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | key/value 长度上限 96/96、const 值 4096（`interfaces/innerkits/include/param/init_param.h:32-40`）；`const.`/`ro.`/`ohos.boot.` 只读；写需通过 DAC（`.para.dac` 前缀匹配，组员上限 `MAX_MEMBER_IN_GROUP=128`）+ SELinux 双检；参数文件加载顺序固定（cmdline→const→vendor→system→persist，`InitLoadParamFiles`）；persist 保存路径 `/storage/data/system/param/public_persist_parameters`（`PARAM_PUBLIC_PERSIST_SAVE_PATH`，标准系统 `__LINUX__` 分支 `DATA_PATH`；私有变体 `PARAM_PRIVATE_PERSIST_SAVE_PATH` 位于 `/data/service/el1/public/startup/parameters/`，旧文件名 `persist_parameters` 仅启动迁移 rename 使用）；wait 超时默认 30 秒、set 超时 15 秒 |
| 并发约束 | 写只发生在 init 单线程；多进程读共享内存无锁（commitId 原子量 + futex 等待，`ReadCommitId`）；每标签子空间大小固定（`/etc/param/ohos.para.size`），写满返回 `PARAM_CODE_REACHED_MAX=103`/内存类错误码 |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | `WorkSpace`（`WORKSPACE_INDEX_BASE`，名字 `param_storage`；DAC 工作区 `param_sec_dac`）；ParamTrieHeader 管理分区元数据 |
| 下游影响 | 条件 job（TRIGGER_PARAM）；WaitNode（SystemWaitParameter）；watcher SA 3901（SystemWatchParameter）；服务控制（`ohos.servicectrl.*` 写入即启停服务） |
| 平级关联 | Trigger（消费参数变化）；Service（状态参数 `startup.service.ctl.*` 的主体） |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `ParamNode`/`ParamTrieNode`/`ParamTrieHeader`/`ParamSecurityNode`/`WorkSpace`（`services/param/include/param_common.h`） |
| 核心方法 | `WriteParam`/`FindParam_`（`services/param/base/param_trie.c`）；`SystemSetParam`/`ProcessMessage`/`HandleParamSet`/`HandleParamWaitAdd`（param_service.c）；`SystemSetParameter`/`SystemReadParam`/`SystemWaitParameter`（param_request.c）；`BatchSavePersistParam`（`services/param/manager/param_persist.c:97`） |

## 服务套接字（ServiceSocket）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 服务套接字（pre-fork socket，cfg `socket` 字段实体化） |
| 实体定义 | init 在 fork 服务之前创建并 bind/chown 的本地 socket，供服务以固定路径被客户端连接；解决"服务未启动 socket 已可用"与 ondemand 触发问题 |
| 核心特征 | cfg 字段：`name`（≤32）、`family`（AF_UNIX/AF_NETLINK）、`type`（SOCK_SEQPACKET/STREAM/DGRAM）、`protocol`（default/NETLINK_KOBJECT_UEVENT）、`permissions/uid/gid`、`option[]`（SOCKET_OPTION_PASSCRED/RCVBUFFORCE/CLOEXEC/NONBLOCK）；路径 = `/dev/unix/socket/<name>` |
| 类型/分类 | 连接型（STREAM/SEQPACKET，可挂 watcher 触发按需启动）与数据报型；普通 socket 与 netlink socket（ueventd 用） |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 创建：cfg 解析（`ParseOneService`）或服务拉起时 `CreateSocketForService`（`services/init/init_service_socket.c:199`）→ `SetSocketOptionAndBind`（SO_PASSCRED/SO_RCVBUFFORCE 256KB、bind、lchown、fchmodat、restorecon）；传递：fork 前保留 fd，子进程 `SetSocketEnvForService`（:229）设 `OHOS_SOCKET_<name>=<fd>` 并清 FD_CLOEXEC；按需触发：ondemand + 连接型 socket `listen(fd, MAX_SOCKET_FD_LEN=16)` + `AddSocketWatcher`，连接到达即 `ServiceStart` |
| 状态流转 | 创建 → 绑定/监听 → 服务持有（或等待触发）；服务停止 `CloseServiceSocket` 关闭并清理路径（ueventd 的 netlink socket 不挂 watcher：无连接概念） |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | socket 名即路径名，须全系统唯一；ueventd 服务由 init 预建 netlink socket（`GetControlSocket("ueventd")` 取回）；appspawn 族 socket 同样由 init 创建、appspawn 经 `GetControlSocket` 取 fd（见 appspawn 仓 `appspawn.cfg`） |
| 并发约束 | watcher 回调在 init 单线程执行（`LE_StartWatcher`，WATCHER_ONCE）；socket 读写由服务进程自治 |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | `Service`（socket 列表归其所有，`ServiceSocket` 结构） |
| 下游影响 | ondemand 服务的 socket 触发启动；客户端连接的服务寻址（`GetControlSocket`，`interfaces/innerkits/include/init_socket.h`） |
| 平级关联 | loopevent watcher 任务；selinux 插件 `setSockCreateCon`（创建上下文） |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `ServiceSocket`（`services/init/include/init_service.h`） |
| 核心方法 | `CreateSocketForService`/`SetSocketEnvForService`/`CloseServiceSocket`/`AddSocketWatcher`（`services/init/init_service_socket.c`） |

## 沙箱（Sandbox）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 沙箱（system/chipset sandbox 实例，`sandbox_t`） |
| 实体定义 | 由 json 配置描述、init 构建的独立 mount namespace 挂载视图；一个实例对应一份"根目录 + bind 挂载列表 + 符号链接列表"的收敛环境 |
| 核心特征 | `struct sandbox_t`（`services/sandbox/include/sandbox.h`）：`rootPath`（如 `/mnt/sandbox/system`）、`pathMountsHead/fileMountsHead/linksHead`、`name`、`isCreated`、`ns`（namespace fd）；实例 `g_systemSandbox`/`g_chipsetSandbox`（`services/sandbox/sandbox.c:93` 的 `SandboxMap`） |
| 类型/分类 | system 沙箱（`etc/sandbox/system-sandbox.json`）、chipset 沙箱（`chipset-sandbox.json`）、64 位变体（`*64.json`）、appservice 沙箱；json tag：`sandbox-root`、`mount-bind-paths/files`、`symbol-links`（`target-name/link-name`）、`src-path/sandbox-path/sandbox-flags/ignore` |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 构建：`mksandbox system|chipset` 命令（`DoMkSandbox`，`services/init/standard/init_cmds.c:1225`）或 `InitSandboxWithName`（sandbox.c:576）→ `InitSandbox`（:389，`UnshareNamespace(CLONE_NEWNS)` + 记录 ns fd + 解析 json）→ `PrepareSandbox`（:540，bind 挂载逐项 + symlink + `SYS_pivot_root` + `umount2(".", MNT_DETACH)`）→ 回默认 namespace；进入：`EnterSandbox(name)`（:625，`SetNamespace`）；销毁：`DestroySandbox`（:595） |
| 状态流转 | 未创建（isCreated=false）→ 构建完成（true，rootPath 可用）→ 服务进程 `SetServiceEnterSandbox` 切入；全局开关参数 `const.sandbox`（`IsEnableSandbox`）可禁用整套行为 |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | 服务 cfg `sandbox:0` 表示不进沙箱（`SERVICE_ATTR_WITHOUT_SANDBOX`）；构建沙箱需要 root 与 mount 能力；`DoUmountOtherNsData`（standard/init_cmds.c:838）依赖默认 namespace fd 与 `EnterSandbox` 配合卸载 `/data` |
| 并发约束 | 沙箱构建/进入均在 init 单线程（或被 fork 的服务子进程）中执行；pivot_root 期间路径解析以 rootPath 为准 |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | `InitDefaultNamespace`（`services/sandbox/sandbox_namespace.c`）保存默认 ns 供切回 |
| 下游影响 | `sandbox:1` 服务（`SetServiceEnterSandbox`，失败仅告警 INIT_ESANDBOX）；sub-init（chipset 命令在 chipset 沙箱上下文执行） |
| 平级关联 | begetctl sandbox 命令 / control_fd ACTION_SANDBOX（交互式进入）；appspawn 应用沙箱（本仓外，仅概念对照） |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `sandbox_t`/`mountlist_t`（`services/sandbox/include/sandbox.h`） |
| 核心方法 | `InitSandboxWithName`/`InitSandbox`/`PrepareSandbox`/`EnterSandbox`/`DestroySandbox`/`DumpSandboxByName`（`services/sandbox/sandbox.c`）；`GetNamespaceFd`/`UnshareNamespace`/`SetNamespace`（sandbox_namespace.c） |

## 分组节点（GroupNode）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 分组节点（group 节点，bootgroup 的成员登记项） |
| 实体定义 | 当前启动分组（bootgroup）允许出现的 job/service/plugin/command 名字集合元素；不承载执行逻辑，仅做可见性过滤 |
| 核心特征 | `InitWorkspace`（`services/init/init_group_manager.c`）含 4 个 hashmap（`GROUP_HASHMAP_BUCKET=32`）：jobs/services/plugins/cmds（NODE_TYPE_*），及 `groupNodes[]` 与 `groupModeStr`（当前分组名） |
| 类型/分类 | 节点四类：NODE_TYPE_JOBS/SERVICES/PLUGINS/CMDS；分组模式由 cmdline `bootgroup`（默认 `device.boot.group`）或 misc 充电模式（`device.charge.group`）决定 |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 创建：`InitParseGroupCfg`（:241）解析 `/system/etc/<group>.cfg` 的 `jobs/services/groups` 数组 → `AddGroupNode`；使用：`AddService`/`CheckJobValid` 前置 `CheckNodeValid`（:333）；分组 import 深度上限 `GROUP_IMPORT_MAX_LEVEL=5` |
| 状态流转 | 启动早期 `InitServiceSpace()`（:202）建立工作区并确定分组名，之后只读；GROUP_BOOT 模式下未列出项默认放行（除 charger 服务被禁止），GROUP_CHARGE 模式收敛为充电最小集 |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | 分组文件决定裁剪面：不在组内的服务/命令在严格模式下不可用；启动模式判定还受 `ohos.boot.mode` 参数与 misc 分区影响（`ReadConfig` 分支） |
| 并发约束 | 仅 init 单线程访问，无并发问题 |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | cmdline/misc（分组名来源） |
| 下游影响 | Service/Trigger/插件命令的准入（`CheckNodeValid`） |
| 平级关联 | cfg 解析（init_config.c）、充电模式目录 `/system/etc/charger` |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `InitWorkspace`/`GroupNode`（`services/init/init_group_manager.c` 及其头文件） |
| 核心方法 | `InitServiceSpace`/`InitParseGroupCfg`/`AddGroupNode`/`CheckNodeValid`（init_group_manager.c） |

## ueventd 设备节点配置（DeviceUdevConf）

### 实体概念
| 子维度 | 内容 |
|---|---|
| 实体名称 | 设备节点权限配置（`DeviceUdevConf`） |
| 实体定义 | `ueventd.config` INI 文件中一行设备规则的内存形态，定义某设备节点/属性节点的 mode/uid/gid 与可选动作，是 ueventd 创建节点时的权限查表依据 |
| 核心特征 | `struct DeviceUdevConf`（`ueventd/include/ueventd_read_cfg.h:27`）：`name/mode/uid/gid/parameter/action`；配置段 `[device]`（`/dev` 节点）、`[sysfs]`、`[firmware]` |
| 类型/分类 | 精确名匹配规则；by-name 链接规则（如 `/dev/block/by-name/misc 0660 update update`） |

### 上下文与场景
| 子维度 | 内容 |
|---|---|
| 交互流程 | 加载：ueventd 启动 `ParseUeventdConfigFile`（`/etc/ueventd.config`、`/vendor/etc/ueventd.config`、`/vendor/etc/ueventd_factory.config`）；使用：`HandleUevent`（`ueventd/ueventd.c:110`）按 subsystem 分发 → `CreateDeviceNodeWithPermissions`（`ueventd/ueventd_device_handler.c:148`）经 `GetDeviceNodePermissions` 查表后 mknod/chown/chmod；`CreateSymbolLinks`（:55）生成 by-name 链接 |
| 状态流转 | 配置只读；设备节点随 uevent add/remove 创建与移除；`SetDeviceLable`（:95）逐级 restorecon |

### 规格与约束
| 子维度 | 内容 |
|---|---|
| 业务规则 | 规则缺失时使用默认权限；early-boot 阶段 `HandleRequiredDynamicDeviceNodes` 直接 mknod 必需动态节点（tty/binder/console 等 `DYNAMIC_DEVICES[]`）；固件事件处理写 `/sys<syspath>/loading` 为 "-1" 取消内核等待（`ueventd_firmware_handler.c`） |
| 并发约束 | ueventd 单线程处理 netlink 消息（`UEVENT_BUFFER_SIZE` 2048+1）；参数通知经独立工作线程异步 `SystemSetParameter("startup.uevent.<parameter>", "added"/"removed")`（5 秒 cond 超时重试） |

### 知识关联
| 子维度 | 内容 |
|---|---|
| 上层依赖 | 内核 netlink（`NETLINK_KOBJECT_UEVENT`，`nl_groups=0xffffffff`、SO_PASSCRED） |
| 下游影响 | 块设备 by-name 访问（fstab 挂载、升级）；`startup.uevent.*` 参数消费者 |
| 平级关联 | init 第一阶段（`StartUeventd`/`RetriggerUevent` 拉起必需分区设备）；ueventd 服务 cfg（init 预建 socket） |

### 代码与符号
| 子维度 | 内容 |
|---|---|
| 核心类 | `DeviceUdevConf`（ueventd_read_cfg.h）；uevent 消息解析结构（`ueventd/include/ueventd.h`） |
| 核心方法 | `ParseUeventdConfigFile`/`GetDeviceNodePermissions`（ueventd_read_cfg.c）；`ProcessUevent`/`HandleUevent`（ueventd.c）；`HandleBlockDeviceEvent`/`CreateDeviceNodeWithPermissions`/`CreateSymbolLinks`（ueventd_device_handler.c）；`UeventdSocketInit`（ueventd_socket.c） |
