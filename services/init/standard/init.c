/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "init.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/major.h>

#include "config_policy_utils.h"
#include "device.h"
#include "fd_holder_service.h"
#include "fs_manager/fs_manager.h"
#include "init_control_fd_service.h"
#include "init_log.h"
#include "init_mount.h"
#include "init_group_manager.h"
#include "init_param.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"
#include "switch_root.h"
#include "ueventd.h"
#include "ueventd_socket.h"
#include "fd_holder_internal.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#include <selinux/selinux.h>
#endif // WITH_SELINUX
#include "bootstage.h"

static bool g_enableSandbox;

static int FdHolderSockInit(void)
{
    int sock = -1;
    int on = 1;
    int fdHolderBufferSize = FD_HOLDER_BUFFER_SIZE; // 4KiB
    sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        INIT_LOGE("Failed to create fd holder socket, err = %d", errno);
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVBUFFORCE, &fdHolderBufferSize, sizeof(fdHolderBufferSize));
    setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    if (access(INIT_HOLDER_SOCKET_PATH, F_OK) == 0) {
        INIT_LOGI("%s exist, remove it", INIT_HOLDER_SOCKET_PATH);
        unlink(INIT_HOLDER_SOCKET_PATH);
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if (strncpy_s(addr.sun_path, sizeof(addr.sun_path),
        INIT_HOLDER_SOCKET_PATH, strlen(INIT_HOLDER_SOCKET_PATH)) != 0) {
        INIT_LOGE("Faild to copy fd hoder socket path");
        close(sock);
        return -1;
    }
    socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1);
    if (bind(sock, (struct sockaddr *)&addr, len) < 0) {
        INIT_LOGE("Failed to binder fd folder socket %d", errno);
        close(sock);
        return -1;
    }

    // Owned by root
    if (lchown(addr.sun_path, 0, 0)) {
        INIT_LOGW("Failed to change owner of fd holder socket, err = %d", errno);
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    if (fchmodat(AT_FDCWD, addr.sun_path, mode, AT_SYMLINK_NOFOLLOW)) {
        INIT_LOGW("Failed to change mode of fd holder socket, err = %d", errno);
    }
    INIT_LOGI("Init fd holder socket done");
    return sock;
}

void SystemInit(void)
{
    SignalInit();
    // umask call always succeeds and return the previous mask value which is not needed here
    (void)umask(DEFAULT_UMASK_INIT);
    MakeDirRecursive("/dev/unix/socket", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    int sock = FdHolderSockInit();
    if (sock >= 0) {
        RegisterFdHoldWatcher(sock);
    }
    InitControlFd();
}

static void EnableDevKmsg(void)
{
    /* printk_devkmsg default value is ratelimit, We need to set "on" and remove the restrictions */
    int fd = open("/proc/sys/kernel/printk_devkmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0) {
        return;
    }
    char *kmsgStatus = "on";
    write(fd, kmsgStatus, strlen(kmsgStatus) + 1);
    close(fd);
    fd = -1;
    return;
}

void LogInit(void)
{
    int ret = mknod("/dev/kmsg", S_IFCHR | S_IWUSR | S_IRUSR,
        makedev(MEM_MAJOR, DEV_KMSG_MINOR));
    if (ret == 0) {
        OpenLogDevice();
    }
}

static char **GetRequiredDevices(Fstab fstab, int *requiredNum)
{
    int num = 0;
    FstabItem *item = fstab.head;
    while (item != NULL) {
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            num++;
        }
        item = item->next;
    }
    char **devices = (char **)calloc(num, sizeof(char *));
    INIT_ERROR_CHECK(devices != NULL, return NULL, "Failed calloc err=%d", errno);

    int i = 0;
    item = fstab.head;
    while (item != NULL) {
        if (FM_MANAGER_REQUIRED_ENABLED(item->fsManagerFlags)) {
            devices[i] = strdup(item->deviceName);
            INIT_ERROR_CHECK(devices[i] != NULL, FreeStringVector(devices, num); return NULL,
                "Failed strdup err=%d", errno);
            i++;
        }
        item = item->next;
    }
    *requiredNum = num;
    return devices;
}

static int StartUeventd(char **requiredDevices, int num)
{
    INIT_ERROR_CHECK(requiredDevices != NULL && num > 0, return -1, "Failed parameters");
    int ueventSockFd = UeventdSocketInit();
    if (ueventSockFd < 0) {
        INIT_LOGE("Failed to create uevent socket");
        return -1;
    }
    RetriggerUevent(ueventSockFd, requiredDevices, num);
    close(ueventSockFd);
    return 0;
}

static void StartInitSecondStage(void)
{
    int requiredNum = 0;
    Fstab* fstab = LoadRequiredFstab();
    INIT_ERROR_CHECK(fstab != NULL, abort(), "Failed to load required fstab");
    char **devices = GetRequiredDevices(*fstab, &requiredNum);
    if (devices != NULL && requiredNum > 0) {
        int ret = StartUeventd(devices, requiredNum);
        if (ret == 0) {
            ret = MountRequriedPartitions(fstab);
        }
        FreeStringVector(devices, requiredNum);
        devices = NULL;
        ReleaseFstab(fstab);
        fstab = NULL;
        if (ret < 0) {
            // If mount required partitions failure.
            // There is no necessary to continue.
            // Just abort
            INIT_LOGE("Mount requried partitions failed; please check fstab file");
            // Execute sh for debugging
            execv("/bin/sh", NULL);
            abort();
        }
    }

    // It will panic if close stdio before execv("/bin/sh", NULL)
    CloseStdio();

#ifndef DISABLE_INIT_TWO_STAGES
    SwitchRoot("/usr");
    // Execute init second stage
    char * const args[] = {
        "/bin/init",
        "--second-stage",
        NULL,
    };
    if (execv("/bin/init", args) != 0) {
        INIT_LOGE("Failed to exec \"/bin/init\", err = %d", errno);
        exit(-1);
    }
#endif
}

void SystemPrepare(void)
{
    MountBasicFs();
    CreateDeviceNode();
    LogInit();
    // Make sure init log always output to /dev/kmsg.
    EnableDevKmsg();
    // Only ohos normal system support
    // two stages of init.
    // If we are in updater mode, only one stage of init,
    INIT_LOGI("DISABLE_INIT_TWO_STAGES not defined");
    if (InUpdaterMode() == 0) {
        StartInitSecondStage();
    }
}

void SystemLoadSelinux(void)
{
#ifdef WITH_SELINUX
    // load selinux policy and context
    if (LoadPolicy() < 0) {
        INIT_LOGE("main, load_policy failed.");
    } else {
        INIT_LOGI("main, load_policy success.");
    }

    setcon("u:r:init:s0");
    (void)RestoreconRecurse("/dev");
#endif // WITH_SELINUX
}

static void BootStateChange(const char *content)
{
    INIT_LOGI("boot start %s finish.", content);
    if (strcmp("init", content) == 0) {
        StartAllServices(START_MODE_BOOT);
        return;
    }
    if (strcmp("post-init", content) == 0) {
        StartAllServices(START_MODE_NARMAL);
        // Destroy all hooks
        HookMgrDestroy(NULL);
        return;
    }
}

#if defined(OHOS_SERVICE_DUMP)
static int SystemDump(int id, const char *name, int argc, const char **argv)
{
    INIT_ERROR_CHECK(argv != NULL && argc >= 1, return 0, "Invalid install parameter");
    INIT_LOGI("Dump system info %s", argv[0]);
    SystemDumpParameters(1);
    SystemDumpTriggers(1);
    return 0;
}
#endif

static void IsEnableSandbox(void)
{
    const char *name = "const.sandbox";
    char value[MAX_BUFFER_LEN] = {0};
    unsigned int len = MAX_BUFFER_LEN;
    if (SystemReadParam(name, value, &len) != 0) {
        INIT_LOGE("Failed read param.");
        g_enableSandbox = false;
    }
    if (strcmp(value, "enable") == 0) {
        INIT_LOGI("Enable sandbox.");
        g_enableSandbox = true;
    } else {
        INIT_LOGI("Disable sandbox.");
        g_enableSandbox = false;
    }
}

static void InitLoadParamFiles(void)
{
    if (InUpdaterMode() != 0) {
        LoadDefaultParams("/etc/param/ohos_const", LOAD_PARAM_NORMAL);
        LoadDefaultParams("/etc/param", LOAD_PARAM_ONLY_ADD);
        return;
    }

    // Load const params, these can't be override!
    LoadDefaultParams("/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    CfgFiles *files = GetCfgFiles("etc/param");
    for (int i = MAX_CFG_POLICY_DIRS_CNT - 1; files && i >= 0; i--) {
        if (files->paths[i]) {
            LoadDefaultParams(files->paths[i], LOAD_PARAM_ONLY_ADD);
        }
    }
    FreeCfgFiles(files);
}

typedef struct HOOK_TIMING_STAT {
    struct timespec startTime;
    struct timespec endTime;
} HOOK_TIMING_STAT;

static void InitPreHook(const HOOK_INFO *hookInfo)
{
    HOOK_TIMING_STAT *stat = (HOOK_TIMING_STAT *)hookInfo->cookie;
    clock_gettime(CLOCK_MONOTONIC, &(stat->startTime));
}

static void InitPostHook(const HOOK_INFO *hookInfo)
{
    long long diff;
    const long long baseTime = 1000;
    HOOK_TIMING_STAT *stat = (HOOK_TIMING_STAT *)hookInfo->cookie;
    clock_gettime(CLOCK_MONOTONIC, &(stat->endTime));

    diff = (long long)((stat->endTime.tv_sec - stat->startTime.tv_sec) / baseTime);
    if (stat->endTime.tv_nsec > stat->startTime.tv_nsec) {
        diff += (stat->endTime.tv_nsec - stat->startTime.tv_nsec) * baseTime;
    } else {
        diff -= (stat->endTime.tv_nsec - stat->startTime.tv_nsec) * baseTime;
    }

    INIT_LOGV("Executing hook [%d:%d:%p] cost [%lld]ms, return %d.",
        hookInfo->stage, hookInfo->prio, hookInfo->hook, diff, hookInfo->retVal);
}

void SystemConfig(void)
{
    HOOK_TIMING_STAT timingStat;
    HOOK_EXEC_ARGS args;

    args.flags = 0;
    args.cookie = (void *)&timingStat;
    args.preHook = InitPreHook;
    args.postHook = InitPostHook;

    HookMgrExecute(NULL, INIT_GLOBAL_INIT, (void *)&args);
    InitServiceSpace();

    HookMgrExecute(NULL, INIT_PRE_PARAM_SERVICE, (void *)&args);
    InitParamService();
    InitParseGroupCfg();
    RegisterBootStateChange(BootStateChange);

    // load SELinux context and policy
    // Do not move position!
    SystemLoadSelinux();
    // parse parameters
    HookMgrExecute(NULL, INIT_PRE_PARAM_LOAD, (void *)&args);
    InitLoadParamFiles();
    // read config
    HookMgrExecute(NULL, INIT_PRE_CFG_LOAD, (void *)&args);
    ReadConfig();
    INIT_LOGI("Parse init config file done.");
    HookMgrExecute(NULL, INIT_POST_CFG_LOAD, (void *)&args);

    // dump config
#if defined(OHOS_SERVICE_DUMP)
    AddCmdExecutor("display", SystemDump);
    (void)AddCompleteJob("param:ohos.servicectrl.display", "ohos.servicectrl.display=*", "display system");
#endif
    IsEnableSandbox();
    // execute init
    PostTrigger(EVENT_TRIGGER_BOOT, "pre-init", strlen("pre-init"));
    PostTrigger(EVENT_TRIGGER_BOOT, "init", strlen("init"));
    PostTrigger(EVENT_TRIGGER_BOOT, "post-init", strlen("post-init"));
}

void SystemRun(void)
{
    StartParamService();
}

void SetServiceEnterSandbox(const char *execPath, unsigned int attribute)
{
    if (g_enableSandbox == false) {
        return;
    }
    if ((attribute & SERVICE_ATTR_SANDBOX) != SERVICE_ATTR_SANDBOX) {
        return;
    }
    INIT_ERROR_CHECK(execPath != NULL, return, "Service path is null.");
    if (strncmp(execPath, "/system/bin/", strlen("/system/bin/")) == 0) {
        if (strcmp(execPath, "/system/bin/appspawn") == 0) {
            INIT_LOGI("Appspawn skip enter sandbox.");
        } else if (strcmp(execPath, "/system/bin/hilogd") == 0) {
            INIT_LOGI("Hilogd skip enter sandbox.");
        } else {
            INIT_INFO_CHECK(EnterSandbox("system") == 0, return,
                "Service %s skip enter sandbox system.", execPath);
        }
    } else if (strncmp(execPath, "/vendor/bin/", strlen("/vendor/bin/")) == 0) {
        // chipset sandbox will be implemented later.
        INIT_INFO_CHECK(EnterSandbox("chipset") == 0, return,
            "Service %s skip enter sandbox system.", execPath);
    } else {
        INIT_LOGI("Service %s does not enter sandbox", execPath);
    }
    return;
}
