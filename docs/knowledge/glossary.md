# 术语表

## init 进程（begetd）
内核启动的第一个用户态进程（PID 1，`services/init/main.c` 校验 `getpid()==INIT_PROCESS_PID(1)`）。标准系统以 `/bin/init --second-stage` 进入第二阶段后常驻主事件循环。与"begetd"常互换使用（构建产物名 `begetd`，见 `begetd.gni`）。区别于 appspawn：init 只管原生系统服务，应用进程由 appspawn 孵化。

## 两阶段启动（two-stage boot）
第一阶段（ramdisk 中 `init` 或 `init_early`，`services/init/standard/init_firststage.c` 的 `SystemPrepare`）挂载 fstab.required 必需分区并 `execv("/bin/init", {"--second-stage"})`；第二阶段（`services/init/standard/init.c`）完成参数服务、cfg 解析与服务拉起。updater 模式（`InUpdaterMode()`）跳过挂载/切根逻辑。区别于"switch root"（`SwitchRoot` 将根切换到 `/usr`，`interfaces/innerkits/fs_manager/switch_root/switch_root.c`）。

## 启动配置（cfg）
JSON 结构启动脚本，顶层 `jobs`/`services`/`import` 三键（`ParseInitCfgContents`，`services/init/init_config.c`）。加载优先级 `/system` < `/chipset` < `/sys_prod` < `/chip_prod`（`ParseCfgByPriority` 经 config_policy 的 `GetCfgFiles`）。import 支持路径 `${param}` 参数展开，递归上限 `IMPORT_MAX_LEVEL=100`。区别于 group cfg：后者只声明名字数组，不定义内容。

## 分组配置（group cfg / bootgroup）
`/system/etc/<group>.cfg`（如 `device.boot.group.cfg`、`device.charge.group`），JSON 键 `jobs`/`services`/`groups` 为字符串数组，决定该启动分组允许哪些 job/service（`AddGroupNode`，`services/init/init_group_manager.c`）。分组名来自 cmdline `bootgroup` 键（`BOOT_GROUP_NAME`），充电模式由 misc 分区 `GetBootModeFromMisc()` 判定。GROUP_BOOT 模式下未列出的 job/service 默认放行（仅 `charger` 服务被禁止）。

## 作业（job / trigger）
cfg `jobs` 数组中的启动单元：`{name, condition, cmds[]}`。三类（`GetTriggerType`，`services/param/trigger/trigger_processor.c`）：保留名 job（pre-init/init/post-init/early-fs/reboot 等）为 `TRIGGER_BOOT`；`param:` 前缀（条件 job，如 `param:sys.boot_completed=true`）为 `TRIGGER_PARAM`；`boot-service:` 为服务打包 job（`AddCompleteJob` 动态生成）。lite 系统（`services/init/lite/init_jobs.c`）无 trigger 引擎，`DoJob` 同步执行。注意 `trigger` 也是一条命令（`DoTrigger`，standard 命令表）。

## 命令（cmd）
job 内的最小执行单元，按 `g_cmdTable`（通用表 `services/init/init_common_cmds.c:846` + 标准表 `services/init/standard/init_cmds.c:1307` + 插件命令）匹配。每 job 命令数上限 `MAX_CMD_CNT_IN_ONE_JOB=200`。带上下文概念（ConfigContext，`INIT_CONTEXT_MAIN`/`INIT_CONTEXT_CHIPSET`）：chipset 上下文的命令经 `ExecuteCmdInSubInit` 转发给 sub-init 进程执行。

## 服务（Service）
cfg `services` 数组定义的常驻进程实体，运行期结构 `Service`（`services/init/include/init_service.h:185`），由 init fork+exec 并按 SIGCHLD 监管。关键字段：`path[]`（≤20 段）、`uid`/`gid[]`、`once`、`important`（-20..19）、`critical`（`[enable, crashCount, crashTime]`）、`ondemand`、`sandbox`（0/1）、`start-mode`（condition/boot/normal）、`secon`、`socket[]`、`file[]`、`caps[]`、`cpucore[]`、`period`、`writepid[]`、`jobs`（on-start/on-stop/on-restart 等）、`permission[]`、`apl`、`cgroup`、`envs[]`。

## once / importance / critical（服务保活三字段）
- `once=1`：进程退出不重启（`SERVICE_ATTR_ONCE`）。
- `importance`：标准系统仅做 setpriority 调度优先级；lite 系统非 0 表示重要进程，其退出触发整机 `RebootSystem()`（`services/init/lite/init_signal_handler.c`）。
- `critical`：崩溃监管字段，默认 240 秒内 4 次（`CRITICAL_DEFAULT_CRASH_TIME/CRASH_COUNT`，`services/init/init_service_manager.c`）超限 `ExecReboot("panic")` 整机重启；与 `ondemand` 互斥。

## 按需启动（ondemand / start-mode）
`ondemand:true` 的服务不随 boot job 拉起，由外部事件触发：socket 连接触发（init 预建 socket 后 `AddSocketWatcher` 监听，收到连接即 `ServiceStart`，`services/init/init_service_socket.c`）、参数触发（`start-mode=condition` + samgr 写 `ohos.servicectrl.*`）、console 触发（`WatchConsoleDevice`）。区别于 `start_ondemand` 命令/begetctl `start_ondemand`（单次手动拉起）。

## 服务控制前缀参数
写参数即控制服务的约定前缀（`services/param/include/param_utils.h`）：`ohos.ctl.start=`/`ohos.ctl.stop=`、`ohos.servicectrl.`（samgr 用）、`ohos.startup.powerctrl=`（reboot/shutdown）。执行结果经 `startup.service.ctl.<name>`（值=ServiceStatus）与 `.pid` 参数回告（`NotifyServiceChange`）。

## 系统参数（syspara / param）
全局键值对服务，key/value 各上限 96 字节、const 值 4096 字节（`PARAM_NAME_LEN_MAX` 等，`interfaces/innerkits/include/param/init_param.h`）。三类标签：`const.`（不可变，如产品名）、`persist.`（跨重启，存 `/storage/data/system/param/public_persist_parameters`，私有持久化变体在 `/data/service/el1/public/startup/parameters/`，`param_utils.h` 的 `PARAM_PUBLIC/PRIVATE_PERSIST_SAVE_PATH`）、普通可写。`ro.`/`ohos.boot.` 前缀只读（`IS_READY_ONLY`）。存储为按 `.` 分段的二叉 trie，mmap 于 `/dev/__parameters__`（`services/param/base/param_trie.c`）。客户端 API：`SystemGetParameter`（`SystemReadParam`）、`SystemSetParameter`、`SystemWaitParameter`、`WatchParamCheck`/`SystemWatchParameter`。

## 参数 DAC / MAC
- DAC：`*.para.dac` 文件按前缀定义 `user:group:mode[:type]`（如 `services/etc/param/ohos.para.dac`），位含读 0x0100/写 0x0080/watch 0x0040（`services/param/adapter/param_dac.c`）。
- MAC：SELinux 标签从 `parameter_contexts` 加载（`services/param/adapter/param_selinux.c`），每标签独立参数子空间，大小由 `/etc/param/ohos.para.size` 配置。
违反两者拒绝时错误码为 `DAC_RESULT_FORBIDED=1001` / `SELINUX_RESULT_FORBIDED=1002`。

## 参数监听（watcher / param_watcher SA 3901）
`SystemWatchParameter` 的服务端是独立 SA 进程 param_watcher（`services/param/watcher/`，`sa_profile/param_watcher.json`），SA 再经 paramservice socket 向 init 发 MSG_ADD_WATCHER/MSG_DEL_WATCHER。区别于 `SystemWaitParameter`（一次性等待，超时默认 `DEFAULT_PARAM_WAIT_TIMEOUT=30` 秒，在 init 内直接挂 WaitNode 完成，不经 SA）。

## fd 代持（fd_holder）
服务退出重启期间由 init 代管其 fd 的机制：服务经 `/dev/unix/socket/fd_holder` 以 SCM_RIGHTS 发送 `serviceName|hold|poll` 请求（`interfaces/innerkits/fd_holder/`）；规则是只允许代持本服务自己的 fd（ucred 校验）；上限 `MAX_HOLD_FDS=64`；重启后经环境变量 `OHOS_FD_HOLD_<service>` 归还（`PublishHoldFds`）。参见 `ServiceSaveFd/ServiceGetFd`。

## 控制通道（control_fd）
`/dev/unix/socket/init_control_fd` 上的带外控制协议（`interfaces/innerkits/control_fd/`），消息 `CmdMessage`，动作 ACTION_SANDBOX（进入沙箱）、ACTION_DUMP（dump 服务/参数/trigger/loop）、ACTION_MODULEMGR、ACTION_APP_SPAWNTIME、ACTION_APP_SANDBOX（begetctl app 沙箱 PTY）。begetctl 是其主要客户端。

## begetctl
本仓自带 CLI（`services/begetctl/`），自研 mini shell（`BShellEnv*`）。子命令：param ls/get/set/wait、service_control、sandbox、bootchart、modulectl、reboot、dump_service、setloglevel、partitionslot、paramshell 等。经 libbegetutil 与 control_fd 工作，不直接读 init 内部状态。

## 沙箱（system/chipset sandbox）
init 用 mount namespace + `pivot_root` 构建的隔离挂载视图（`services/sandbox/sandbox.c`）：配置 `etc/sandbox/system-sandbox.json`、`chipset-sandbox.json`，元素为 `sandbox-root`、`mount-bind-paths/files`、`symbol-links`。`mksandbox` 命令或服务 `sandbox:1` 触发。区别于应用沙箱（appspawn 的 `/mnt/sandbox`）：这里是面向 system/chipset 服务提供方的挂载收敛。

## sub-init（chipset 上下文）
chipset 域命令的执行代理进程：cfg 中 `/chipset`、`/vendor`、`/chip_prod` 前缀文件的命令标记为 `INIT_CONTEXT_CHIPSET`（`GetConfigContextType`），执行时经 `ExecuteCmdInSubInit` 转发（`services/init/standard/init_cmdexecutor.c`、`services/modules/init_context`），用于芯片域与主域解耦。

## seccomp（系统调用过滤）
服务/应用的 syscall 白名单机制：策略文件 `services/modules/seccomp/seccomp_policy/*.seccomp.policy`（节：`@allowList/@blockList/@returnValue/@priority...`），构建期由 `scripts/generate_code_from_policy.py` 生成 `lib<name>_filter.z.so`，运行期 `SetSeccompPolicyWithName` dlopen 后 `seccomp(SECCOMP_SET_MODE_FILTER, TSYNC|LOG)` 装载（`services/modules/seccomp/seccomp_policy.c`）。`filtername` 等于服务名或 `system`/`spawn` 等类型名；`@returnValue` 决定 KILL_PROCESS/TRAP/ERRNO 等动作。

## ueventd
独立服务进程（`services/etc/ueventd.cfg`，ondemand，init 预建其 netlink socket）：监听 `NETLINK_KOBJECT_UEVENT`，为 block/firmware/其他子系统创建设备节点（权限查 `ueventd.config` INI 表）、生成 `/dev/block/by-name` 符号链接、写 `startup.uevent.<parameter>` 参数通知。init 第一阶段 `StartUeventd` 在 init 进程内做一轮 `RetriggerUevent` 用于拉起必需分区设备。

## bootevent / bootchart
启动性能观测的两种机制：bootevent 插件（`services/modules/bootevent/`）记录服务 fork 时间与启动事件参数；bootchart 插件（`services/modules/bootchart/`）采样 CPU/IO 输出 bootchart 数据，是 init 唯一含采样线程的组件。

## DeviceInfo（SA 3902）
设备信息服务（`device_info/`），SA id 3902，进程 `deviceinfoservice`，提供 `GetUdid/GetSerialID/GetDiskSN` 与 CPU/BIOS/板级信息（HDI `iboard_info`），带低内存按需卸载（60 秒空闲）。区别于 `GetDevUdid()` 参数接口：后者由 libbegetutil 直接计算（udid 插件 `InitAddPreCfgLoadHook(UDidCalc)`）。

## 特性开关（begetd.gni）
构建期 `declare_args` 集合（`begetd.gni`），如 `init_feature_ab_partition`、`init_feature_seccomp_privilege`、`init_feature_custom_sandbox`、`init_feature_support_saspawn`、`enable_ohos_startup_init_feature_watcher`、lite 系列 `init_lite_*`。产品在产品配置中赋值，不修改仓内默认值。

## saspawn
`init_feature_support_saspawn` 特性下的另一种孵化路径：服务标记 `saspawn` 时 init 不直接 fork，改走 `InitServiceBySaspawn`（`services/init/init_common_service.c`）。区别于 appspawn 的普通路径，用于特定安全孵化形态（实验/受限产品）。

## loopevent
init 与参数服务共用的单线程事件循环库（`services/loopevent/`，头 `interfaces/innerkits/include/loop_event.h`）：`LE_GetDefaultLoop()` 默认单例、`LE_RunLoop` epoll 循环、流式 socket 任务（`LE_CreateStreamServer/Client`）、signalfd 信号任务、定时器、idle。客户端错误码 `LE_STATUS`（LE_FAILURE=10000 起）。appspawn 也复用该库。
