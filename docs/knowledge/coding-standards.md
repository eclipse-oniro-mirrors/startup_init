# 编码铁律

## 并发模型

- **禁止**在 `services/init`、`services/param` 代码路径引入线程（`pthread_create`）、长阻塞 IO、可阻塞等待，采用 loopevent 任务（`LE_CreateTimer`/`LE_StartTimer`、idle 任务、socket 任务）或 fork 子进程执行（`SyncExecCommand`，超时 `SYNC_EXEC_TIMEOUT_MS=30000`，`services/init/standard/init_cmds.c`）。init 是 PID 1 单线程进程，阻塞即冻结全系统服务孵化、信号回收与参数服务。当前已知线程仅存在于独立进程：bootchart 插件采样线程、ueventd 参数通知线程（`ueventd/standard/ueventd_parameter.c`）。
- **禁止**在参数读端做任何写操作或持有写锁跨进程；读端只 mmap `/dev/__parameters__` 并用 `ReadCommitId` + futex 等待 commitId 稳定（`services/param/base/param_base.h`），写一律经 paramservice socket 由 init 单点执行。依据：`services/param/linux/param_request.c` 客户端只读实现。

## 错误处理

- **禁止**忽略服务 fork/权限设置失败继续 exec，采用 `RunChildProcessInternal`（`services/init/init_common_service.c`）的 fail-fast：`SetServiceEnterSandbox` 失败仅告警（`INIT_ESANDBOX`，沙箱非强制时），`SetAccessToken`/`SetPerms`/socket/文件创建失败分别以 `INIT_EACCESSTOKEN`/`INIT_ESECCOMP`/`INIT_EGIDSET`/`INIT_EUIDSET`/`INIT_ECAP`/`INIT_ESOCKET`/`INIT_EFILE` 退出，父进程 `ServiceStart` 记 `lastErrno`。错误码枚举定义于 `interfaces/innerkits/include/init_error.h`。
- **禁止**改动 `INIT_OK..INIT_EEXEC` 与 `PARAM_CODE_*`（100..117，另 DAC_RESULT_* 1000..1002）的数值与语义，新增能力沿用打包规则而非裸数字。这些是 inner-kits 稳定 ABI（`libbegetutil.versionscript` 等版本脚本锁定）。
- **禁止**吞掉 SELinux/参数权限错误继续降级运行：`setexeccon` 失败 `_exit(INIT_EEXEC_CONTENT)`（`services/modules/selinux/selinux_adp.c`）；参数 set 被 DAC/MAC 拒绝即返回 `DAC_RESULT_FORBIDED`(1001)/`SELINUX_RESULT_FORBIDED`(1002)，不得改为放行。

## 权限与安全顺序

- **禁止**调整 `SetPerms` 步骤顺序（`services/init/init_common_service.c`）：securebits 保 caps → `setgid`/`setgroups` → **seccomp（必须先于 setuid）** → `setuid` → `capset`/ambient。顺序错误会造成子进程能力丢失或以错误身份装载 seccomp，属安全回归。
- **禁止**为"跑通"而放宽 `services/etc/param/ohos.para.dac` 的 UGO 模式位、删除 SELinux 标签/`parameter_contexts` 条目、抬高 fd_holder 或 servicectrl 的授权面。参数默认第三方只 get/watch 不可 set。
- **禁止**让 fd-holder 代持非本服务 fd：`CheckFdHolderPermission`（`services/init/standard/fd_holder_service.c`）以 SO_PASSCRED 的 ucred 校验 `requestPid == service->pid`，任何绕过该判定的改动都意味着 fd 泄漏/提权。
- **禁止**手改生成的 seccomp 过滤库 `lib*_filter.z.so`；只允许修改 `*.seccomp.policy` 后用 `services/modules/seccomp/scripts/generate_code_from_policy.py` 重新生成。手改会在下次生成时丢失且无法审计。blocklist 增删都可能引发进程被 SIGSYS 杀死或攻击面扩大。
- **禁止**随意新增/修改 `secon` 标签、uid/gid（`services/etc/passwd`、`group`）与 `privileged_process.seccomp.policy`（`@privilegedProcessName` + `@allowBlockList`）；高权限进程名单变更需安全评审。

## 命令与配置约束

- **禁止**在 job 中编写长阻塞命令：`sleep` 上限 5 秒（`DoSleep`，`services/init/init_common_cmds.c:194`）；`wait`/mount `wait` 默认 `WAIT_MAX_SECOND=5` 秒（`interfaces/innerkits/include/init_utils.h`）；`exec`/`syncexec` 必须带超时；lite `loadcfg` 上限 50KB（`LOADCFG_MAX_FILE_LEN`）/20 条（`LOADCFG_MAX_LOOP`）。单条命令耗时超过 200ms 会输出耗时日志（init_common_cmds.c:1044）。
- **禁止**提交非法 JSON 的 cfg/para 文件：解析失败会输出 `Cfg error` 并中断该文件加载（`ParseInitCfg`，`services/init/init_config.c`），启动脚本缺失直接卡开机。cfg 字段合法性以 `ParseOneService`（init_service_manager.c）与命令表 `GetMatchCmd`（init_common_cmds.c:919）为准。
- **禁止**静默修改 `once`/`importance`/`critical`/`ondemand` 语义与默认值：`once=0` 退出即重启；critical 默认 240 秒 4 次（`CRITICAL_DEFAULT_CRASH_COUNT=4`、`CRITICAL_DEFAULT_CRASH_TIME=240`，init_service_manager.c）超限整机 panic；`ondemand` 与 `critical` 互斥（`GetServiceOnDemand` 校验）；importance 在 lite 上退出会复位系统。错误默认值 = 卡开机循环或非预期重启。

## API 与构建

- **禁止**在 inner-kits 头文件中增删函数、改签名或返回值而不更新 `libbegetutil.versionscript`/`libsystemparam.versionscript`/`libbeget_proxy.versionscript` 与 `bundle.json` inner_kits 清单；导出面泄漏或缺失都会破坏其他子系统构建。
- **禁止**在 `services/loopevent`、`services/utils` 中引用 `services/init` 的头文件（依赖方向见架构文档）；初始化内部结构体（Service/Trigger/ParamWorkspace）时使用对应 `Init*`/`Add*` 函数，不手工构造后直接挂入全局表。
- **禁止**在标准系统代码路径使用 lite-only 能力或反之：`loadcfg`、lite 服务字段（无 trigger/条件 job、无 Linux-only 字段）只存在于 `services/init/lite/`；标准系统新增命令必须同时登记通用表或标准表并说明 `careContext`（chipset 上下文命令需支持 sub-init 转发，见 `ExecuteCmdInSubInit`）。

## 日志与调试

- **禁止**使用裸 printf/自定义输出，采用 `beget_ext.h`/`services/log` 的 `INIT_LOGV/INIT_LOGD/INIT_LOGI/INIT_LOGW/INIT_LOGE/INIT_LOGF`（`services/log/init_log.h`）与模块级宏（如 `INIT_DMESG`、`INIT_FILE`、`INIT_AGENT`），级别受 `persist.init.debug.loglevel` 控制；破坏统一的日志通道会丢失开机阶段可观测性。
- **禁止**在无任务要求时执行破坏性命令（`reboot [updater|flashd|suspend]`、`partitionslot setactive`、`param set persist.*` 系统级键、修改 fstab/misc）；这些命令直接作用于真实设备启动状态。
