# AGENTS.md — base/startup/init

Guidance for coding agents working in the OpenHarmony **startup_init** repository
(the `init` process, `ueventd`, system-parameter service, sandbox, seccomp, and
related tooling). Read this before editing any file under `base/startup/init/`.

> Scope: repository root (`base/startup/init`). Applies to the standard system
> (Linux kernel) unless a path is explicitly marked `lite/` (mini/small system).

## 1. Code map

### 1.1 What this repo owns

`init` is PID 1 — the first user-space process the kernel starts. It:
- parses `*.cfg` / `*.para` / `*.group.cfg` boot scripts,
- forks and supervises native system services (with uid/gid/caps/secon/seccomp),
- runs the system-parameter (syspara) service over a local socket,
- runs `ueventd` for hot-plug device-node management,
- sets up system/chipset sandboxes and seccomp filters,
- provides `begetctl` shell tooling and the `libbegetutil`/`libsystemparam`
  inner-kits for other subsystems.

### 1.2 Layering (dependency direction: top depends on bottom; never reverse)

```
interfaces/kits          JS/CJ/TS public APIs (@ohos.systemparameter, deviceInfo)
interfaces/innerkits     libbegetutil / libsystemparam / libbeget_proxy /
                         libinit_module_engine / fs_manager / seccomp /
                         control_fd / fd_holder / socket / service_control /
                         service_watcher / hookmgr / modulemgr / reboot
services/init            init process core (main, jobs, cmds, service mgr, mount,
                         signal handler, group mgr, capability) — standard + lite
services/param           system-parameter store, DAC/MAC, trigger, watcher,
                         param server (socket), adapter (linux/liteos)
services/loopevent       single-threaded event loop (loop/signal/socket/timer/idle)
services/modules         dynamically loadable plugins: bootchart, bootevent,
                         crashhandler, encaps, init_hook, reboot, seccomp,
                         selinux, sysevent, trace, udid, module_update
services/sandbox         system/chipset sandbox build + namespace mgmt
services/begetctl        `begetctl` CLI tool (param/sandbox/bootchart/reboot/...)
services/log             init logging (dmesg/hilog/printf) + per-module macros
services/utils           hashmap, list, generic utils
services/etc             *.cfg, passwd, group, fstab, sandbox json (standard)
services/etc_lite        *.cfg, passwd, group, param (mini/small)
ueventd                 netlink listener, device-node/firmware handler
device_info             DeviceInfo system ability (kits + service + load)
initsync                init sync inner-api
remount, watchdog, simulator, scripts  supporting tools
test                    unittest / moduletest / fuzztest / exec_test / toolstest
```

### 1.3 Key paths and "where to look"

| Task | Go to |
| --- | --- |
| init main entry / two-stage boot | `services/init/main.c`, `services/init/standard/init.c` (`SystemPrepare`/`SystemInit`/`SystemConfig`/`SystemRun`) |
| Job parsing & execution | `services/init/init_jobs.c`, `services/init/init_common_cmds.c`, `services/init/standard/init_cmds.c` |
| Service lifecycle (fork/stop/restart/critical) | `services/init/init_service_manager.c`, `services/init/init_common_service.c`, `services/init/init_service_file.c` |
| Service socket creation (pre-fork socket) | `services/init/init_service_socket.c` |
| Boot config JSON schema & parsing | `services/init/init_config.c`, `services/init/init_group_manager.c` |
| System-parameter store/server | `services/param/manager/`, `services/param/linux/param_service.c` (`StartParamService`), `services/param/base/`, `services/param/trigger/`, `services/param/watcher/` |
| DAC/MAC for parameters | `services/etc/param/ohos.para.dac`, `services/param/...` dac loader |
| Event loop (the init main loop) | `services/loopevent/` (`LE_RunLoop(LE_GetDefaultLoop())`) |
| Plugins / dynamic modules | `services/modules/` (+ `interfaces/innerkits/init_module_engine/`) |
| Sandbox json & namespace | `services/sandbox/sandbox.c`, `services/sandbox/*-sandbox*.json`, `interfaces/innerkits/sandbox/` |
| Seccomp policy gen + loader | `services/modules/seccomp/` (scripts, `seccomp_policy/`, `seccomp_policy.c`) |
| ueventd (device nodes/firmware) | `ueventd/ueventd_*.c`, `ueventd/etc/ueventd.config` |
| DeviceInfo SA | `device_info/device_info_service.cpp`, `device_info/device_info_kits.cpp` |
| begetctl CLI | `services/begetctl/` (`main.c`, `param_cmd.c`, `service_control.c`, `sandbox.cpp`, `bootchart_cmd.c`) |
| Public TS/JS/CJ kits | `interfaces/kits/jskits/`, `interfaces/kits/cj/`, `interfaces/kits/syscap_ts/` |
| Inner-kits exported headers | `interfaces/innerkits/include/` (parameter.h, service_control.h, init_socket.h, hookmgr.h, ...) |
| Build wiring / component manifest | `bundle.json`, `begetd.gni`, `BUILD.gn`, `services/etc/BUILD.gn` |

High-risk / frequently-changed paths (treat changes here with extra care):
`services/init/standard/init.c`, `services/init/init_service_manager.c`,
`services/init/init_common_service.c` (seccomp/uid/gid/caps ordering),
`services/param/linux/param_service.c` + `param_msgadp.c` (the main loop),
`services/loopevent/`, `services/modules/seccomp/seccomp_policy/`,
`services/etc/param/ohos.para.dac`, `ueventd/ueventd_device_handler.c`.

Nested guidance: none yet. Add module-level `AGENTS.md` under
`services/modules/seccomp/` or `services/param/` when guidance grows.

## 2. Knowledge routing — read before you edit

Domain docs live in the OpenHarmony docs repo under
`docs/zh-cn/device-dev/subsystems/`. They define the **concepts** (jobs, cfg,
services, syspara, sandbox, seccomp, ueventd, bootevent, begetctl, deviceInfo,
boot group, componentized boot, two-stage boot). Load the matching doc before
touching the related code; do **not** guess semantics from source alone.

Before editing, state to yourself: (1) task category, (2) which doc(s) below
you read, (3) which constraint in §3 applies. If you skip this, stop and read.

### 2.1 Task-based routing

| Working on / task | Read this doc first | Key concept to load |
| --- | --- | --- |
| `init.c`, two-stage boot, fstab, required partitions, A/B slot | `subsys-boot-deviceboot.md` | ramdisk vs system-as-root, required fstab, `default_boot_device`, `bootslots`/`currentslot`, partition symlink creation |
| `init_config.c`, `*.cfg`, `device.*.group.cfg`, import | `subsys-boot-init-cfg.md` | JSON cfg structure: `jobs`/`services`/`import`; load order `/system/etc/init` < `/vendor/etc/init`; group cfg (boot/charge) |
| `init_jobs.c`, `init_cmds.c`, trigger/condition jobs | `subsys-boot-init-jobs.md` | pre-init/init/post-init, custom jobs, condition jobs (`param:xxx=`), `trigger`, the full command table + per-command limits |
| `init_service_manager.c`, service cfg fields, on-demand start | `subsys-boot-init-service.md` | service fields (uid/gid/caps/once/importance/critical/cpucore/apl/secon/start-mode/ondemand/socket/env/period/cgroup), on-start/on-stop/on-restart, socket pre-fork, error codes INIT_OK..INIT_EEXEC |
| `services/param/**`, param server, `ohos.para*` | `subsys-boot-init-sysparam.md` | const/persist/writable, key rules (96B), DAC `.para.dac`, MAC selinux labels + `parameter.te`/`parameter_contexts`, label memory sizing `ohos.para.size`, load order cmdline→const→vendor→system→persist, error codes 100..114 |
| `services/sandbox/**`, `*-sandbox*.json` | `subsys-boot-init-sandbox.md` | system/chipset sandbox, `sandbox-root`/`mount-bind-paths`/`symbol-links`, `sandbox:0/1` per service, innerapi_tags chipsetsdk/passthrough install dirs |
| `services/modules/seccomp/**`, policy files | `subsys-boot-init-seccomp.md` | BPF filter, `ohos_prebuilt_seccomp` template, `filtername`==service name, blocklist baseline, privileged_process list, returnValue TRAP/KILL, strace/audit/collect_elf stats |
| `ueventd/**`, `ueventd.config` | `subsys-boot-init-ueventd.md` | netlink uevent, device/sysfs/firmware sections, `/dev/block/by-name` symlinks, parameter notification (`startup.uevent.device_xxx`) |
| `services/begetctl/**`, `services/modules/bootchart`, `bootevent` | `subsys-boot-init-plugin.md` | begetctl command table, bootchart/bootevent plugins, `MODULE_CONSTRUCTOR`/`AddCmdExecutor`, `modulectl install/uninstall/list` |
| `services/log/**`, `beget_ext.h`, per-module log macros | `subsys-boot-init-log.md` | dmesg vs hilog, INIT_DEBUG..INIT_FATAL, `INIT_DMESG`/`INIT_FILE`/`INIT_AGENT` macros, `persist.init.debug.loglevel`, per-module LOGI macros |
| `device_info/**`, deviceInfo adapters, vendor `.para` | `subsys-boot-init-deviceInfo.md` | const.product.* mapping, OHOS-fixed vs vendor-fixed vs dynamic (cmdline/build/BUILD.gn), `hal_sys_param.c`/`vendor.para` for lite |
| New product / componentized build, `sys_prod`/`chip_prod` | `subsys-boot-init-sub-unit.md` | system/sys_prod/chipset/chip_prod components, cfg+param scan priority `/system` < `/chipset` < `/sys_prod` < `/chip_prod` |
| Boot stuck / reboot loop / service not restarting | `subsys-boot-init-faqs.md` | once/importance/critical behavior, "Cfg error!", required-mount failures, bootevent completeness, fd-holder ownership rule |
| Subsystem overview / where a feature belongs | `subsys-boot-overview.md` | init vs appspawn vs bootstrap vs ueventd responsibilities |

External but tightly coupled: `base/security/selinux_adapter` (`*.te`,
`parameter.te`, `parameter_contexts`, `file_contexts`, service `secon`),
`systemabilitymgr_samgr` (SA on-demand), `startup_appspawn` (spawn seccomp).

### 2.2 Path-based routing

| Changing files under | Read |
| --- | --- |
| `services/init/**` | deviceboot + jobs + cfg + service + faqs |
| `services/param/**`, `services/etc/param/**` | sysparam + deviceInfo (for const.* keys) |
| `services/sandbox/**`, `*-sandbox*.json` | sandbox + sub-unit (for install dirs) |
| `services/modules/seccomp/**` | seccomp |
| `ueventd/**` | ueventd + deviceboot (block-device creation) |
| `services/begetctl/**`, `services/modules/{bootchart,bootevent,reboot}` | plugin |
| `device_info/**` | deviceInfo |
| `interfaces/**` (any public/inner API header) | service (service_control), sysparam, and §3 API rules |

### 2.3 Vocabulary routing (when a term appears in a task/log/issue/API)

| Term / acronym | Concept doc |
| --- | --- |
| job, pre-init/init/post-init, trigger, condition job | jobs.md |
| cfg, group.cfg, import, services array | cfg.md + service.md |
| syspara, `.para`, `.para.dac`, persist., const., `param set/get/wait/watch` | sysparam.md |
| sandbox, system-sandbox.json, chipset-sandbox.json, namespace, mount-bind | sandbox.md |
| seccomp, BPF, filtername, blocklist, privileged_process, SIGSYS | seccomp.md |
| ueventd, netlink, `/dev/block/by-name`, device node, firmware | ueventd.md + deviceboot.md |
| begetctl, bootchart, bootevent, modulectl, `MODULE_CONSTRUCTOR` | plugin.md |
| critical, once, importance, ondemand, start-mode, on-start/on-stop/on-restart | service.md |
| required fstab, default_boot_device, bootslots, currentslot, ramdisk, two-stage | deviceboot.md |
| system/sys_prod/chipset/chip_prod, componentized boot | sub-unit.md |
| `ohos.required_mount.*`, fstab.required | deviceboot.md |
| DeviceInfo, const.product.*, `GetSerial`/`GetDevUdid`/`AclGet*` | deviceInfo.md |
| `INIT_DMESG`/`INIT_FILE`/`INIT_AGENT`, `persist.init.debug.loglevel`, INIT_LOG* | log.md |

## 3. Constraints and boundaries (do not break without escalation)

### 3.1 Architecture invariants (hard rules)

1. **init is single-threaded.** The whole init process runs one event loop:
   `SystemRun()` → `StartParamService()` → `ParamServiceStart()` →
   `LE_RunLoop(LE_GetDefaultLoop())` (see `services/init/standard/init.c:523`,
   `services/param/linux/param_msgadp.c:143`). Do **not** introduce threads,
   `pthread_create`, or blocking calls in init's path. Any long work must be
   scheduled as a non-blocking task on `services/loopevent/` (timer/socket/idle
   tasks). A blocking call freezes service-forking, signal handling, and the
   parameter service for every client.
2. **init talks to other processes only over local sockets.** Cross-process
   control (service start/stop, parameter get/set/watch, control-fd, fd-holder)
   goes through AF_UNIX sockets under `/dev/unix/socket` (see
   `interfaces/innerkits/include/init_socket.h`: `OHOS_SOCKET_DIR`). Do not add
   new IPC channels (Binder, shared memory, pipes) as init-side servers without
   escalation; do not call into samgr/appspawn IPC synchronously from the init
   main loop. Service-control and parameter clients reach init via these sockets
   through `libbegetutil`/`libsystemparam`.
3. **Dependency direction is interfaces → services/init → loopevent/utils.**
   `services/init` may call `services/loopevent` and `services/utils`, but
   `loopevent` must not depend on `services/init`. `interfaces/innerkits` is the
   only outward surface other subsystems may link; do not move service-internal
   headers into innerkits without review.
4. **Plugin model owns "optional" features.** bootchart/bootevent/reboot/seccomp
   are dynamic modules via `interfaces/innerkits/init_module_engine` +
   `services/modules`. Do not hard-wire a plugin's logic into `services/init`
   core; register through hooks (`InitAddPostPersistParamLoadHook`,
   `AddCmdExecutor`).

### 3.2 Do-not rules

- Do **not** change public API signatures, return values, or error-code enums
  listed in `subsys-boot-init-service.md` (`INIT_OK..INIT_EEXEC`) and
  `subsys-boot-init-sysparam.md` (`PARAM_CODE_*` 100..114) without
  compatibility review. These are stabilized in
  `interfaces/innerkits/libbegetutil.versionscript` /
  `libsystemparam.versionscript` / `libbeget_proxy.versionscript`.
- Do **not** reorder permission setup in `init_common_service.c`
  (`SetPerms`): seccomp policy must be set **before** `setuid` (see seccomp doc
  strace example); caps/uid/gid ordering affects whether a child retains
  capabilities. Changing the order is a security regression.
- Do **not** edit `services/modules/seccomp/seccomp_policy/*.seccomp.policy` or
  `privileged_process.seccomp.policy` casually — these are the syscall baseline
  for every native service / app. Adding to a blocklist can kill processes
  (SIGSYS); removing from a blocklist widens attack surface.
- Do **not** weaken `services/etc/param/ohos.para.dac` (DAC) or remove a
  selinux label/`parameter_contexts` entry to "make it work" — both are
  access-control. Third-party apps may get/watch but not set by default; raising
  UGO or dropping a `parameter_service set` allow rule is a privilege escalation.
- Do **not** let one service hold another service's fd via fd-holder
  (`fd_holder_service.c`): only a service may request holding of **its own** fd
  (faqs.md: "只支持代持本服务的fd").
- Do **not** change `once`/`importance`/`critical` defaults silently:
  `once=0` restarts on SIGCHLD; non-once process exiting 5x in 4 min stops
  restart; `importance=1` (small system) resets the system; `critical=[M,N,T]`
  reboots the device. Wrong defaults can cause boot loops or device wipes.
- Do **not** add blocking `exec`/`syncexec` calls in early jobs without a
  timeout — `sleep` is capped at 5s; `wait` at 5s; `loadcfg` max 50KB / 20 cmds.
- Do **not** modify generated policy libraries (`lib*_filter.z.so`) by hand;
  they are produced from `*.seccomp.policy` by
  `services/modules/seccomp/scripts/generate_code_from_policy.py`.
- Do **not** run destructive device commands (`reboot`, `reboot updater/flashd`,
  `partitionslot setactive`, `param set persist.*`) unless the task explicitly
  asks. They affect real hardware/boot state.

### 3.3 Ask before

- Any change to `bundle.json` / `begetd.gni` feature flags (e.g.
  `init_feature_ab_partition`, `init_feature_seccomp_privilege`,
  `init_feature_custom_sandbox`, `init_feature_support_saspawn`) — these change
  product behavior across the tree.
- Any new `secon` label, new uid/gid in `services/etc/passwd`|`group`, or a new
  `high_privilege_process_list.json` entry (needs security review).
- Any change to cfg/para load order or component priority
  (`/system` < `/chipset` < `/sys_prod` < `/chip_prod`).
- Any change to the init main-loop scheduling model (§3.1.1).

### 3.4 Common agent failure modes here

- Treating init like a normal multi-threaded daemon and adding mutexes/threads
  → deadlocks the event loop.
- "Fixing" a permission-denied by relaxing DAC/MAC instead of configuring the
  right label/group → silent privilege escalation.
- Editing a `.cfg`/`.para` with invalid JSON (commas, brackets) →
  `Cfg error, failed to parse json` → boot stops (faqs.md).
- Forgetting that mini/small systems (`services/init/lite/`, `services/etc_lite/`)
  have no trigger/condition jobs and no Linux-only service fields.

## 4. Verification loop

### 4.1 Pre-build (download prebuilt toolchain once)

```bash
bash build/prebuilts_download.sh
```

### 4.2 Build the init component (rk3568, debug)

```bash
./build.sh --product-name rk3568 --gn-args is_debug=true --build-target init
```

### 4.3 Build & run unit tests

```bash
# compile the init unit-test suite
./build.sh --product-name rk3568 --gn-args is_debug=true --build-target init_unittest
```

Test targets are wired in `test/BUILD.gn` (`testgroup`). Per-area unit tests
live under `test/unittest/{init,param,begetctl,loopevent,modules,seccomp,
ueventd,deviceinfo,syspara,fs_manager,remount,innerkits}`, fuzz under
`test/fuzztest`, moduletests under `test/moduletest`. When changing seccomp,
also build `seccomp_filter`; when changing sandbox json, re-check the
`mksandbox` command path.

### 4.4 Minimum checks before declaring done

- [ ] Build target `init` succeeds with the command in §4.2.
- [ ] `--build-target init_unittest` compiles; run the affected `unittest`
      binary and confirm no new failures.
- [ ] No new compile warnings in the files you touched.
- [ ] If you changed a public/inner header: confirm it is still listed in the
      right `versionscript` and `bundle.json` `inner_kits` block — no accidental
      export.
- [ ] If you changed cfg/para/sandbox/seccomp policy: validate JSON / re-run the
      seccomp policy generator so generated `.so` matches the policy file.
- [ ] Re-read §3 for every rule that touches your diff; none violated.

### 4.5 Done definition & final response

When finished, report: (1) the task category and which §2 doc(s) you loaded,
(2) files changed with `path:line` of the key edits, (3) which §3 constraint
applied and how you respected it, (4) the exact build/test commands you ran and
their pass/fail result, (5) anything you could not validate and why. If you
cannot run the build (no toolchain/device), say so explicitly and list the
commands the reviewer should run instead — do not claim success without
evidence.

### 4.6 If validation cannot run

State plainly "build/test not executed: <reason>" and provide the exact
commands for a human to run. Do not mark the task complete.
