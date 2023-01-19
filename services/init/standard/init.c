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
#include <stdarg.h>
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
#include "key_control.h"
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
#include "bootstage.h"

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
        INIT_LOGE("Failed to copy fd hoder socket path");
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
            INIT_LOGE("Mount required partitions failed; please check fstab file");
            // Execute sh for debugging
            execv("/bin/sh", NULL);
            abort();
        }
    }

    // It will panic if close stdio before execv("/bin/sh", NULL)
    CloseStdio();

    // Set up a session keyring that all processes will have access to.
    KeyCtrlGetKeyringId(KEY_SPEC_SESSION_KEYRING, 1);

#ifndef DISABLE_INIT_TWO_STAGES
    INIT_LOGI("Start init second stage.");
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
    INIT_LOGI("Start init first stage.");
    // Only ohos normal system support
    // two stages of init.
    // If we are in updater mode, only one stage of init.
    if (InUpdaterMode() == 0) {
        StartInitSecondStage();
    }
}

#define INIT_BOOTSTAGE_HOOK_NAME "bootstage"
static HOOK_MGR *bootStageHookMgr = NULL;

HOOK_MGR *GetBootStageHookMgr()
{
    if (bootStageHookMgr != NULL) {
        return bootStageHookMgr;
    }

    /*
     * Create bootstage hook manager for booting only.
     * When boot completed, this manager will be destroyed.
     */
    bootStageHookMgr = HookMgrCreate(INIT_BOOTSTAGE_HOOK_NAME);
    return bootStageHookMgr;
}

INIT_TIMING_STAT g_bootJob = {{0}, {0}};

static void RecordInitBootEvent(const char *initBootEvent)
{
    const char *bootEventArgv[] = {"init", initBootEvent};
    PluginExecCmd("bootevent", ARRAY_LENGTH(bootEventArgv), bootEventArgv);
    return;
}

static void BootStateChange(int start, const char *content)
{
    if (start == 0) {
        clock_gettime(CLOCK_MONOTONIC, &(g_bootJob.startTime));
        RecordInitBootEvent(content);
        INIT_LOGI("boot job %s start.", content);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &(g_bootJob.endTime));
        RecordInitBootEvent(content);
        long long diff = InitDiffTime(&g_bootJob);
        INIT_LOGI("boot job %s finish diff %lld us.", content, diff);
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

static void InitPreHook(const HOOK_INFO *hookInfo, void *executionContext)
{
    INIT_TIMING_STAT *stat = (INIT_TIMING_STAT *)executionContext;
    clock_gettime(CLOCK_MONOTONIC, &(stat->startTime));
}

static void InitPostHook(const HOOK_INFO *hookInfo, void *executionContext, int executionRetVal)
{
    INIT_TIMING_STAT *stat = (INIT_TIMING_STAT *)executionContext;
    clock_gettime(CLOCK_MONOTONIC, &(stat->endTime));
    long long diff = InitDiffTime(stat);
    INIT_LOGI("Executing hook [%d:%d] cost [%lld]us, return %d.",
        hookInfo->stage, hookInfo->prio, diff, executionRetVal);
}

static void InitSysAdj(void)
{
    const char* path = "/proc/self/oom_score_adj";
    const char* content = "-1000";
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        return;
    }
    if (write(fd, content, strlen(content)) < 0) {
        close(fd);
        return;
    }
    close(fd);
    return;
}

static void TriggerServices(int startMode)
{
    int index = 0;
    int jobNum = 0;
    char jobName[64] = {0}; // 64 job name
    char cmd[64] = {0};  // 64 job name
    const int maxServiceInJob = 4; // 4 service in job
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        Service *service = node->data.service;
        if (service == NULL || service->startMode != startMode) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        if (IsOnDemandService(service)) {
            if (CreateServiceSocket(service) != 0) {
                INIT_LOGE("service %s exit! create socket failed!", service->name);
            }
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        if (sprintf_s(cmd, sizeof(cmd), "start %s", service->name) <= 0) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        if (index == 0) {
            if (sprintf_s(jobName, sizeof(jobName), "boot-service:service-%d-%03d", startMode, jobNum) <= 0) {
                node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
                continue;
            }
            jobNum++;
        }
        index++;
        AddCompleteJob(jobName, NULL, cmd);
        INIT_LOGV("Add %s to job %s", service->name, jobName);
        if (index == maxServiceInJob) {
            PostTrigger(EVENT_TRIGGER_BOOT, jobName, strlen(jobName));
            index = 0;
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
    if (index > 0) {
        PostTrigger(EVENT_TRIGGER_BOOT, jobName, strlen(jobName));
    }
}

void ParseInitCfgByPriority(void)
{
    CfgFiles *files = GetCfgFiles("etc/init");
    for (int i = 0; files && i < MAX_CFG_POLICY_DIRS_CNT; i++) {
        if (files->paths[i]) {
            if (ReadFileInDir(files->paths[i], ".cfg", ParseInitCfg, NULL) < 0) {
                break;
            }
        }
    }
    FreeCfgFiles(files);
}

void SystemConfig(void)
{
    INIT_TIMING_STAT timingStat;
    
    InitSysAdj();
    HOOK_EXEC_OPTIONS options;

    options.flags = 0;
    options.preHook = InitPreHook;
    options.postHook = InitPostHook;
    InitServiceSpace();
    HookMgrExecute(GetBootStageHookMgr(), INIT_GLOBAL_INIT, (void *)&timingStat, (void *)&options);
    RecordInitBootEvent("init.prepare");

    HookMgrExecute(GetBootStageHookMgr(), INIT_PRE_PARAM_SERVICE, (void *)&timingStat, (void *)&options);
    InitParamService();
    InitParseGroupCfg();
    RegisterBootStateChange(BootStateChange);

    INIT_LOGI("boot init finish.");
    // load SELinux context and policy
    // Do not move position!
    PluginExecCmdByName("loadSelinuxPolicy", "");
    RecordInitBootEvent("init.prepare");

    RecordInitBootEvent("init.ParseCfg");
    LoadSpecialParam();

    // parse parameters
    HookMgrExecute(GetBootStageHookMgr(), INIT_PRE_PARAM_LOAD, (void *)&timingStat, (void *)&options);
    InitLoadParamFiles();
    // read config
    HookMgrExecute(GetBootStageHookMgr(), INIT_PRE_CFG_LOAD, (void *)&timingStat, (void *)&options);
    ReadConfig();
    RecordInitBootEvent("init.ParseCfg");
    INIT_LOGI("boot parse config file done.");
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_CFG_LOAD, (void *)&timingStat, (void *)&options);

    IsEnableSandbox();
    // execute init
    PostTrigger(EVENT_TRIGGER_BOOT, "pre-init", strlen("pre-init"));
    PostTrigger(EVENT_TRIGGER_BOOT, "init", strlen("init"));
    TriggerServices(START_MODE_BOOT);
    PostTrigger(EVENT_TRIGGER_BOOT, "post-init", strlen("post-init"));
    TriggerServices(START_MODE_NORMAL);
    clock_gettime(CLOCK_MONOTONIC, &(g_bootJob.startTime));
}

void SystemRun(void)
{
    StartParamService();
}
