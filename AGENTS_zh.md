# AGENTS.md — base/startup/init

面向在 OpenHarmony **startup_init** 仓库（包含 `init` 进程、`ueventd`、系统参数服务、
沙箱、seccomp 及相关工具）工作的编码 Agent 的指南。在修改 `base/startup/init/` 下任何
文件之前，请先阅读本文档。

> 作用范围：仓库根目录（`base/startup/init`）。适用于标准系统（Linux 内核），除非路径
> 明确标注为 `lite/`（迷你/小型系统）。

## 1. 代码地图

### 1.1 本仓库负责的内容

`init` 是 PID 1 —— 内核启动的第一个用户态进程。它负责：
- 解析 `*.cfg` / `*.para` / `*.group.cfg` 启动脚本；
- fork 并托管原生系统服务（处理 uid/gid/caps/secon/seccomp）；
- 通过本地 socket 运行系统参数（syspara）服务；
- 运行 `ueventd` 进行热插拔设备节点管理；
- 设置 system/chipset 沙箱与 seccomp 过滤器；
- 提供 `begetctl` shell 工具，以及其他子系统使用的
  `libbegetutil`/`libsystemparam` inner-kits。

### 1.2 分层（依赖方向：上层依赖下层；严禁反向）

```
interfaces/kits          JS/CJ/TS 公开 API（@ohos.systemparameter、deviceInfo）
interfaces/innerkits     libbegetutil / libsystemparam / libbeget_proxy /
                         libinit_module_engine / fs_manager / seccomp /
                         control_fd / fd_holder / socket / service_control /
                         service_watcher / hookmgr / modulemgr / reboot
services/init            init 进程核心（main、jobs、cmds、service mgr、mount、
                         signal handler、group mgr、capability）— 标准 + lite
services/param           系统参数存储、DAC/MAC、trigger、watcher、
                         param server（socket）、适配层（linux/liteos）
services/loopevent       单线程事件循环（loop/signal/socket/timer/idle）
services/modules         可动态加载插件：bootchart、bootevent、
                         crashhandler、encaps、init_hook、reboot、seccomp、
                         selinux、sysevent、trace、udid、module_update
services/sandbox         system/chipset 沙箱构建 + namespace 管理
services/begetctl        `begetctl` 命令行工具（param/sandbox/bootchart/reboot/...）
services/log             init 日志（dmesg/hilog/printf）+ 各模块日志宏
services/utils           hashmap、list、通用工具
services/etc             *.cfg、passwd、group、fstab、sandbox json（标准系统）
services/etc_lite        *.cfg、passwd、group、param（迷你/小型系统）
ueventd                 netlink 监听器、设备节点/firmware 处理器
device_info             DeviceInfo 系统能力（kits + service + load）
initsync                init 同步 inner-api
remount, watchdog, simulator, scripts  辅助工具
test                    unittest / moduletest / fuzztest / exec_test / toolstest
```

### 1.3 关键路径与"去哪找"

| 任务 | 去哪里找 |
| --- | --- |
| init 主入口 / 两段式启动 | `services/init/main.c`、`services/init/standard/init.c`（`SystemPrepare`/`SystemInit`/`SystemConfig`/`SystemRun`） |
| Job 解析与执行 | `services/init/init_jobs.c`、`services/init/init_common_cmds.c`、`services/init/standard/init_cmds.c` |
| 服务生命周期（fork/stop/restart/critical） | `services/init/init_service_manager.c`、`services/init/init_common_service.c`、`services/init/init_service_file.c` |
| 服务 socket 创建（pre-fork socket） | `services/init/init_service_socket.c` |
| 启动配置 JSON schema 与解析 | `services/init/init_config.c`、`services/init/init_group_manager.c` |
| 系统参数存储/服务 | `services/param/manager/`、`services/param/linux/param_service.c`（`StartParamService`）、`services/param/base/`、`services/param/trigger/`、`services/param/watcher/` |
| 参数的 DAC/MAC | `services/etc/param/ohos.para.dac`、`services/param/...` dac 加载器 |
| 事件循环（init 主循环） | `services/loopevent/`（`LE_RunLoop(LE_GetDefaultLoop())`） |
| 插件 / 动态模块 | `services/modules/`（+ `interfaces/innerkits/init_module_engine/`） |
| 沙箱 json 与 namespace | `services/sandbox/sandbox.c`、`services/sandbox/*-sandbox*.json`、`interfaces/innerkits/sandbox/` |
| seccomp 策略生成与加载 | `services/modules/seccomp/`（scripts、`seccomp_policy/`、`seccomp_policy.c`） |
| ueventd（设备节点/firmware） | `ueventd/ueventd_*.c`、`ueventd/etc/ueventd.config` |
| DeviceInfo SA | `device_info/device_info_service.cpp`、`device_info/device_info_kits.cpp` |
| begetctl 命令行 | `services/begetctl/`（`main.c`、`param_cmd.c`、`service_control.c`、`sandbox.cpp`、`bootchart_cmd.c`） |
| 公开 TS/JS/CJ kits | `interfaces/kits/jskits/`、`interfaces/kits/cj/`、`interfaces/kits/syscap_ts/` |
| inner-kits 导出头文件 | `interfaces/innerkits/include/`（parameter.h、service_control.h、init_socket.h、hookmgr.h、...） |
| 构建配置 / 组件清单 | `bundle.json`、`begetd.gni`、`BUILD.gn`、`services/etc/BUILD.gn` |

高风险 / 频繁变更路径（此处改动需格外谨慎）：
`services/init/standard/init.c`、`services/init/init_service_manager.c`、
`services/init/init_common_service.c`（seccomp/uid/gid/caps 顺序）、
`services/param/linux/param_service.c` + `param_msgadp.c`（主循环）、
`services/loopevent/`、`services/modules/seccomp/seccomp_policy/`、
`services/etc/param/ohos.para.dac`、`ueventd/ueventd_device_handler.c`。

嵌套指南：暂无。当内容增多时，可在 `services/modules/seccomp/` 或
`services/param/` 下添加模块级 `AGENTS.md`。

## 2. 知识路由 —— 编辑前先阅读

领域文档位于 OpenHarmony 文档仓库的 `docs/zh-cn/device-dev/subsystems/` 下。它们定义了
相关**概念**（jobs、cfg、services、syspara、sandbox、seccomp、ueventd、bootevent、
begetctl、deviceInfo、boot group、组件化启动、两段式启动）。在修改相关代码之前，请先
加载对应文档；**不要**仅凭源码猜测语义。

编辑前，先在心中明确：（1）任务类别，（2）已阅读下面哪份文档，（3）适用 §3 中的哪条
约束。若跳过此步骤，请停下来先阅读。

### 2.1 按任务路由

| 涉及的工作 / 任务 | 先读这份文档 | 需要加载的关键概念 |
| --- | --- | --- |
| `init.c`、两段式启动、fstab、required 分区、A/B slot | `subsys-boot-deviceboot.md` | ramdisk vs system-as-root、required fstab、`default_boot_device`、`bootslots`/`currentslot`、分区符号链接创建 |
| `init_config.c`、`*.cfg`、`device.*.group.cfg`、import | `subsys-boot-init-cfg.md` | JSON cfg 结构：`jobs`/`services`/`import`；加载顺序 `/system/etc/init` < `/vendor/etc/init`；group cfg（boot/charge） |
| `init_jobs.c`、`init_cmds.c`、trigger/condition jobs | `subsys-boot-init-jobs.md` | pre-init/init/post-init、自定义 jobs、条件 jobs（`param:xxx=`）、`trigger`、完整命令表 + 各命令限制 |
| `init_service_manager.c`、服务 cfg 字段、按需启动 | `subsys-boot-init-service.md` | 服务字段（uid/gid/caps/once/importance/critical/cpucore/apl/secon/start-mode/ondemand/socket/env/period/cgroup）、on-start/on-stop/on-restart、socket pre-fork、错误码 INIT_OK..INIT_EEXEC |
| `services/param/**`、param server、`ohos.para*` | `subsys-boot-init-sysparam.md` | const/persist/writable、key 规则（96B）、DAC `.para.dac`、MAC selinux 标签 + `parameter.te`/`parameter_contexts`、标签内存大小 `ohos.para.size`、加载顺序 cmdline→const→vendor→system→persist、错误码 100..114 |
| `services/sandbox/**`、`*-sandbox*.json` | `subsys-boot-init-sandbox.md` | system/chipset 沙箱、`sandbox-root`/`mount-bind-paths`/`symbol-links`、每个服务 `sandbox:0/1`、innerapi_tags chipsetsdk/passthrough 安装目录 |
| `services/modules/seccomp/**`、策略文件 | `subsys-boot-init-seccomp.md` | BPF 过滤器、`ohos_prebuilt_seccomp` 模板、`filtername`==服务名、blocklist 基线、privileged_process 列表、returnValue TRAP/KILL、strace/audit/collect_elf 统计 |
| `ueventd/**`、`ueventd.config` | `subsys-boot-init-ueventd.md` | netlink uevent、device/sysfs/firmware 段、`/dev/block/by-name` 符号链接、参数通知（`startup.uevent.device_xxx`） |
| `services/begetctl/**`、`services/modules/bootchart`、`bootevent` | `subsys-boot-init-plugin.md` | begetctl 命令表、bootchart/bootevent 插件、`MODULE_CONSTRUCTOR`/`AddCmdExecutor`、`modulectl install/uninstall/list` |
| `services/log/**`、`beget_ext.h`、各模块日志宏 | `subsys-boot-init-log.md` | dmesg vs hilog、INIT_DEBUG..INIT_FATAL、`INIT_DMESG`/`INIT_FILE`/`INIT_AGENT` 宏、`persist.init.debug.loglevel`、各模块 LOGI 宏 |
| `device_info/**`、deviceInfo 适配器、vendor `.para` | `subsys-boot-init-deviceInfo.md` | const.product.* 映射、OHOS 固定 vs vendor 固定 vs 动态（cmdline/build/BUILD.gn）、lite 的 `hal_sys_param.c`/`vendor.para` |
| 新产品 / 组件化构建、`sys_prod`/`chip_prod` | `subsys-boot-init-sub-unit.md` | system/sys_prod/chipset/chip_prod 组件、cfg+param 扫描优先级 `/system` < `/chipset` < `/sys_prod` < `/chip_prod` |
| 启动卡死 / 重启循环 / 服务不重启 | `subsys-boot-init-faqs.md` | once/importance/critical 行为、"Cfg error!"、required-mount 失败、bootevent 完整性、fd-holder 归属规则 |
| 子系统概览 / 某特性应归属何处 | `subsys-boot-overview.md` | init 与 appspawn、bootstrap、ueventd 各自职责 |

外部但紧耦合：`base/security/selinux_adapter`（`*.te`、`parameter.te`、
`parameter_contexts`、`file_contexts`、服务 `secon`）、`systemabilitymgr_samgr`
（SA 按需启动）、`startup_appspawn`（spawn seccomp）。

### 2.2 按路径路由

| 修改以下路径下的文件 | 需阅读 |
| --- | --- |
| `services/init/**` | deviceboot + jobs + cfg + service + faqs |
| `services/param/**`、`services/etc/param/**` | sysparam + deviceInfo（针对 const.* 键） |
| `services/sandbox/**`、`*-sandbox*.json` | sandbox + sub-unit（针对安装目录） |
| `services/modules/seccomp/**` | seccomp |
| `ueventd/**` | ueventd + deviceboot（块设备创建） |
| `services/begetctl/**`、`services/modules/{bootchart,bootevent,reboot}` | plugin |
| `device_info/**` | deviceInfo |
| `interfaces/**`（任何公开/内部 API 头文件） | service（service_control）、sysparam，以及 §3 API 规则 |

### 2.3 按术语路由（当某术语出现在任务/日志/issue/API 中时）

| 术语 / 缩写 | 概念文档 |
| --- | --- |
| job、pre-init/init/post-init、trigger、condition job | jobs.md |
| cfg、group.cfg、import、services 数组 | cfg.md + service.md |
| syspara、`.para`、`.para.dac`、persist.、const.、`param set/get/wait/watch` | sysparam.md |
| sandbox、system-sandbox.json、chipset-sandbox.json、namespace、mount-bind | sandbox.md |
| seccomp、BPF、filtername、blocklist、privileged_process、SIGSYS | seccomp.md |
| ueventd、netlink、`/dev/block/by-name`、device node、firmware | ueventd.md + deviceboot.md |
| begetctl、bootchart、bootevent、modulectl、`MODULE_CONSTRUCTOR` | plugin.md |
| critical、once、importance、ondemand、start-mode、on-start/on-stop/on-restart | service.md |
| required fstab、default_boot_device、bootslots、currentslot、ramdisk、two-stage | deviceboot.md |
| system/sys_prod/chipset/chip_prod、组件化启动 | sub-unit.md |
| `ohos.required_mount.*`、fstab.required | deviceboot.md |
| DeviceInfo、const.product.*、`GetSerial`/`GetDevUdid`/`AclGet*` | deviceInfo.md |
| `INIT_DMESG`/`INIT_FILE`/`INIT_AGENT`、`persist.init.debug.loglevel`、INIT_LOG* | log.md |

## 3. 约束与边界（未经升级审批不得破坏）

### 3.1 架构不变式（硬性规则）

1. **init 是单线程的。** 整个 init 进程运行一个事件循环：
   `SystemRun()` → `StartParamService()` → `ParamServiceStart()` →
   `LE_RunLoop(LE_GetDefaultLoop())`（见 `services/init/standard/init.c:523`、
   `services/param/linux/param_msgadp.c:143`）。**不要**在 init 路径中引入线程、
   `pthread_create` 或阻塞调用。任何耗时工作必须作为非阻塞任务调度到
   `services/loopevent/`（timer/socket/idle 任务）上执行。一个阻塞调用会冻结所有
   客户端的服务 fork、信号处理和参数服务。
2. **init 仅通过本地 socket 与其他进程通信。** 跨进程控制（服务启动/停止、参数
   get/set/watch、control-fd、fd-holder）通过 `/dev/unix/socket` 下的 AF_UNIX socket
   完成（见 `interfaces/innerkits/include/init_socket.h`：`OHOS_SOCKET_DIR`）。未经
   升级审批，不得新增 IPC 通道（Binder、共享内存、管道）作为 init 侧服务端；不得在
   init 主循环中同步调用 samgr/appspawn IPC。服务控制与参数客户端通过
   `libbegetutil`/`libsystemparam` 经由这些 socket 访问 init。
3. **依赖方向为 interfaces → services/init → loopevent/utils。**
   `services/init` 可调用 `services/loopevent` 和 `services/utils`，但 `loopevent`
   不得依赖 `services/init`。`interfaces/innerkits` 是其他子系统可链接的唯一对外接口面；
   未经评审，不得将服务内部头文件移入 innerkits。
4. **插件模型承载"可选"特性。** bootchart/bootevent/reboot/seccomp 是通过
   `interfaces/innerkits/init_module_engine` + `services/modules` 实现的动态模块。不要
   将插件的逻辑硬编码进 `services/init` 核心；应通过 hook 注册
   （`InitAddPostPersistParamLoadHook`、`AddCmdExecutor`）。

### 3.2 禁止事项

- **不得**修改 `subsys-boot-init-service.md`（`INIT_OK..INIT_EEXEC`）和
  `subsys-boot-init-sysparam.md`（`PARAM_CODE_*` 100..114）中列出的公开 API 签名、
  返回值或错误码枚举，未经兼容性评审。这些已稳定固化在
  `interfaces/innerkits/libbegetutil.versionscript` /
  `libsystemparam.versionscript` / `libbeget_proxy.versionscript` 中。
- **不得**调整 `init_common_service.c`（`SetPerms`）中的权限设置顺序：seccomp 策略
  必须在 `setuid` **之前**设置（见 seccomp 文档 strace 示例）；caps/uid/gid 顺序
  会影响子进程是否保留 capabilities。改变顺序属于安全回退。
- **不得**随意修改 `services/modules/seccomp/seccomp_policy/*.seccomp.policy` 或
  `privileged_process.seccomp.policy` —— 这些是每个原生服务/应用的系统调用基线。
  向 blocklist 添加可能杀死进程（SIGSYS）；从 blocklist 移除会扩大攻击面。
- **不得**为"让它跑起来"而削弱 `services/etc/param/ohos.para.dac`（DAC）或移除
  selinux 标签/`parameter_contexts` 条目 —— 二者均为访问控制。第三方应用默认只能
  get/watch，不能 set；抬高 UGO 或删除 `parameter_service set` 允许规则属于提权。
- **不得**让一个服务通过 fd-holder（`fd_holder_service.c`）持有另一个服务的 fd：
  只有服务自身可请求持有**自己的** fd（faqs.md："只支持代持本服务的fd"）。
- **不得**静默改变 `once`/`importance`/`critical` 默认值：`once=0` 在 SIGCHLD 时
  重启；非 once 进程 4 分钟内退出 5 次会停止重启；`importance=1`（小型系统）会
  重置系统；`critical=[M,N,T]` 会重启设备。错误的默认值可能导致启动循环或设备擦除。
- **不得**在早期 job 中添加无超时的阻塞 `exec`/`syncexec` 调用 —— `sleep` 上限
  5 秒；`wait` 上限 5 秒；`loadcfg` 最大 50KB / 20 条命令。
- **不得**手工修改已生成的策略库（`lib*_filter.z.so`）；它们由
  `services/modules/seccomp/scripts/generate_code_from_policy.py` 从
  `*.seccomp.policy` 生成。
- **不得**运行破坏性设备命令（`reboot`、`reboot updater/flashd`、
  `partitionslot setactive`、`param set persist.*`），除非任务明确要求。它们会影响
  真实硬件/启动状态。

### 3.3 需先询问

- 对 `bundle.json` / `begetd.gni` 特性开关的任何改动（如
  `init_feature_ab_partition`、`init_feature_seccomp_privilege`、
  `init_feature_custom_sandbox`、`init_feature_support_saspawn`）—— 这些会改变
  全树的产品行为。
- 任何新的 `secon` 标签、`services/etc/passwd`|`group` 中新增 uid/gid，或新的
  `high_privilege_process_list.json` 条目（需安全评审）。
- 对 cfg/para 加载顺序或组件优先级的任何改动
  （`/system` < `/chipset` < `/sys_prod` < `/chip_prod`）。
- 对 init 主循环调度模型的任何改动（§3.1.1）。

### 3.4 此处常见的 Agent 失败模式

- 把 init 当作普通多线程守护进程，添加互斥锁/线程
  → 导致事件循环死锁。
- 遇到权限拒绝时通过放宽 DAC/MAC "修复"，而非配置正确的标签/组
  → 静默提权。
- 用无效 JSON（逗号、括号）编辑 `.cfg`/`.para`
  → `Cfg error, failed to parse json` → 启动停止（faqs.md）。
- 忘记迷你/小型系统（`services/init/lite/`、`services/etc_lite/`）
  没有 trigger/condition jobs，也没有 Linux 专属的服务字段。

## 4. 验证循环

### 4.1 预构建（下载预构建工具链，仅一次）

```bash
bash build/prebuilts_download.sh
```

### 4.2 构建 init 组件（rk3568，debug）

```bash
./build.sh --product-name rk3568 --gn-args is_debug=true --build-target init
```

### 4.3 构建并运行单元测试

```bash
# 编译 init 单元测试套件
./build.sh --product-name rk3568 --gn-args is_debug=true --build-target init_unittest
```

测试目标在 `test/BUILD.gn`（`testgroup`）中配置。各领域的单元测试位于
`test/unittest/{init,param,begetctl,loopevent,modules,seccomp,
ueventd,deviceinfo,syspara,fs_manager,remount,innerkits}` 下，模糊测试在
`test/fuzztest`，模块测试在 `test/moduletest`。修改 seccomp 时，还要构建
`seccomp_filter`；修改 sandbox json 时，重新检查 `mksandbox` 命令路径。

### 4.4 声明完成前的最低检查

- [ ] §4.2 中的命令成功构建目标 `init`。
- [ ] `--build-target init_unittest` 编译通过；运行受影响的 `unittest`
      二进制并确认无新增失败。
- [ ] 改动文件无新增编译警告。
- [ ] 若改动了公开/内部头文件：确认其仍列在正确的 `versionscript` 和
      `bundle.json` 的 `inner_kits` 块中 —— 无意外导出。
- [ ] 若改动了 cfg/para/sandbox/seccomp 策略：校验 JSON / 重新运行
      seccomp 策略生成器，使生成的 `.so` 与策略文件一致。
- [ ] 针对 diff 涉及的每条规则重读 §3；均未被违反。

### 4.5 完成定义与最终回复

完成后，报告：（1）任务类别与加载了哪些 §2 文档，（2）改动文件及关键编辑的
`path:line`，（3）适用了哪条 §3 约束及如何遵守，（4）运行的构建/测试命令及其
通过/失败结果，（5）未能验证的内容及原因。若无法运行构建（无工具链/设备），请明确
说明，并列出评审者应运行的命令 —— 不得在无证据的情况下声称成功。

### 4.6 若验证无法运行

明确说明"build/test 未执行：<原因>"，并提供人工运行的确切命令。不得标记任务完成。
