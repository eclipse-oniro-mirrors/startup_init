/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MUSL__
#include <stropts.h>
#endif
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "init.h"
#include "init_adapter.h"
#include "init_cmds.h"
#include "init_cmdexecutor.h"
#include "init_log.h"
#include "init_cmdexecutor.h"
#include "init_jobs_internal.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_service_socket.h"
#include "init_utils.h"
#include "fd_holder_internal.h"
#include "loop_event.h"
#include "securec.h"
#include "service_control.h"

#ifndef OHOS_LITE
#include "hookmgr.h"
#include "bootstage.h"
#endif

#ifdef WITH_SECCOMP
#define APPSPAWN_NAME ("appspawn")
#define NWEBSPAWN_NAME ("nwebspawn")
#define SA_MAIN_PATH ("/system/bin/sa_main")
#endif

#ifndef TIOCSCTTY
#define TIOCSCTTY 0x540E
#endif

static int SetAllAmbientCapability(void)
{
    for (int i = 0; i <= CAP_LAST_CAP; ++i) {
        if (SetAmbientCapability(i) != 0) {
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

static void SetSystemSeccompPolicy(const Service *service)
{
#ifdef WITH_SECCOMP
    if (strncmp(APPSPAWN_NAME, service->name, strlen(APPSPAWN_NAME)) \
        && strncmp(NWEBSPAWN_NAME, service->name, strlen(NWEBSPAWN_NAME))
        && !strncmp(SA_MAIN_PATH, service->pathArgs.argv[0], strlen(SA_MAIN_PATH))) {
        PluginExecCmdByName("SetSeccompPolicy", "start");
    }
#endif
}

#ifndef OHOS_LITE
/**
 * service Hooking
 */
static int ServiceHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    SERVICE_INFO_CTX *serviceContext = (SERVICE_INFO_CTX *)executionContext;
    ServiceHook realHook = (ServiceHook)hookInfo->hookCookie;

    realHook(serviceContext);
    return 0;
};

int InitAddServiceHook(ServiceHook hook, int hookState)
{
    HOOK_INFO info;

    info.stage = hookState;
    info.prio = 0;
    info.hook = ServiceHookWrapper;
    info.hookCookie = (void *)hook;

    return HookMgrAddEx(GetBootStageHookMgr(), &info);
}

/**
 * service hooking execute
 */
static void ServiceHookExecute(const char *serviceName, const char *info, int stage)
{
    SERVICE_INFO_CTX context;

    context.serviceName = serviceName;
    context.reserved = info;

    (void)HookMgrExecute(GetBootStageHookMgr(), stage, (void *)(&context), NULL);
}
#endif

static int SetPerms(const Service *service)
{
    INIT_CHECK_RETURN_VALUE(KeepCapability() == 0, SERVICE_FAILURE);

    if (service->servPerm.gIDCnt == 0) {
        // use uid as gid
        INIT_ERROR_CHECK(setgid(service->servPerm.uID) == 0, return SERVICE_FAILURE,
            "SetPerms, setgid for %s failed. %d", service->name, errno);
    }
    if (service->servPerm.gIDCnt > 0) {
        INIT_ERROR_CHECK(setgid(service->servPerm.gIDArray[0]) == 0, return SERVICE_FAILURE,
            "SetPerms, setgid for %s failed. %d", service->name, errno);
    }
    if (service->servPerm.gIDCnt > 1) {
        INIT_ERROR_CHECK(setgroups(service->servPerm.gIDCnt - 1, (const gid_t *)&service->servPerm.gIDArray[1]) == 0,
            return SERVICE_FAILURE,
            "SetPerms, setgroups failed. errno = %d, gIDCnt=%d", errno, service->servPerm.gIDCnt);
    }
    if (service->servPerm.uID != 0) {
        if (setuid(service->servPerm.uID) != 0) {
            INIT_LOGE("setuid of service: %s failed, uid = %d", service->name, service->servPerm.uID);
            return SERVICE_FAILURE;
        }
    }

    struct __user_cap_header_struct capHeader;
    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;
    struct __user_cap_data_struct capData[CAP_NUM] = {};
    for (unsigned int i = 0; i < service->servPerm.capsCnt; ++i) {
        if (service->servPerm.caps[i] == FULL_CAP) {
            for (int j = 0; j < CAP_NUM; ++j) {
                capData[j].effective = FULL_CAP;
                capData[j].permitted = FULL_CAP;
                capData[j].inheritable = FULL_CAP;
            }
            break;
        }
        capData[CAP_TO_INDEX(service->servPerm.caps[i])].effective |= CAP_TO_MASK(service->servPerm.caps[i]);
        capData[CAP_TO_INDEX(service->servPerm.caps[i])].permitted |= CAP_TO_MASK(service->servPerm.caps[i]);
        capData[CAP_TO_INDEX(service->servPerm.caps[i])].inheritable |= CAP_TO_MASK(service->servPerm.caps[i]);
    }

    INIT_ERROR_CHECK(capset(&capHeader, capData) == 0, return SERVICE_FAILURE,
        "capset failed for service: %s, error: %d", service->name, errno);
    for (unsigned int i = 0; i < service->servPerm.capsCnt; ++i) {
        if (service->servPerm.caps[i] == FULL_CAP) {
            return SetAllAmbientCapability();
        }
        INIT_ERROR_CHECK(SetAmbientCapability(service->servPerm.caps[i]) == 0, return SERVICE_FAILURE,
            "SetAmbientCapability failed for service: %s", service->name);
    }
#ifndef OHOS_LITE
    /*
     * service set Perms hooks
     */
    ServiceHookExecute(service->name, NULL, INIT_SERVICE_SET_PERMS);
#endif
    return SERVICE_SUCCESS;
}

static int WritePid(const Service *service)
{
    const int maxPidStrLen = 50;
    char pidString[maxPidStrLen];
    pid_t childPid = getpid();
    int len = snprintf_s(pidString, maxPidStrLen, maxPidStrLen - 1, "%d", childPid);
    INIT_ERROR_CHECK(len > 0, return SERVICE_FAILURE, "Failed to format pid for service %s", service->name);
    for (int i = 0; i < service->writePidArgs.count; i++) {
        if (service->writePidArgs.argv[i] == NULL) {
            continue;
        }
        FILE *fd = NULL;
        char *realPath = GetRealPath(service->writePidArgs.argv[i]);
        if (realPath != NULL) {
            fd = fopen(realPath, "wb");
        } else {
            fd = fopen(service->writePidArgs.argv[i], "wb");
        }
        if (fd != NULL) {
            INIT_CHECK_ONLY_ELOG((int)fwrite(pidString, 1, len, fd) == len,
                "Failed to write %s pid:%s", service->writePidArgs.argv[i], pidString);
            (void)fclose(fd);
        } else {
            INIT_LOGE("Failed to open %s.", service->writePidArgs.argv[i]);
        }
        if (realPath != NULL) {
            free(realPath);
        }
        INIT_LOGV("ServiceStart writepid filename=%s, childPid=%s, ok", service->writePidArgs.argv[i], pidString);
    }
    return SERVICE_SUCCESS;
}

void CloseServiceFds(Service *service, bool needFree)
{
    if (service == NULL) {
        return;
    }

    INIT_LOGI("Closing service \' %s \' fds", service->name);
    // fdCount > 0, There is no reason fds is NULL
    if (service->fdCount != 0) {
        size_t fdCount = service->fdCount;
        int *fds = service->fds;
        for (size_t i = 0; i < fdCount; i++) {
            INIT_LOGV("Closing fd: %d", fds[i]);
            close(fds[i]);
            fds[i] = -1;
        }
    }
    service->fdCount = 0;
    if (needFree && service->fds != NULL) {
        free(service->fds);
        service->fds = NULL;
    }
}

static void PublishHoldFds(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return, "Publish hold fds with invalid service");
    char fdBuffer[MAX_FD_HOLDER_BUFFER] = {};
    if (service->fdCount > 0 && service->fds != NULL) {
        size_t pos = 0;
        for (size_t i = 0; i < service->fdCount; i++) {
            int fd = dup(service->fds[i]);
            if (fd < 0) {
                INIT_LOGE("Duplicate file descriptors of Service \' %s \' failed. err = %d", service->name, errno);
                continue;
            }
            INIT_ERROR_CHECK(!(snprintf_s((char *)fdBuffer + pos, sizeof(fdBuffer) - pos, sizeof(fdBuffer) - 1,
                "%d ", fd) < 0), return, "snprintf_s failed err=%d", errno);
            pos = strlen(fdBuffer);
        }
        fdBuffer[pos - 1] = '\0'; // Remove last ' '
        INIT_LOGI("fd buffer: [%s]", fdBuffer);
        char envName[MAX_BUFFER_LEN] = {};
        INIT_ERROR_CHECK(!(snprintf_s(envName, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, ENV_FD_HOLD_PREFIX"%s",
            service->name) < 0), return, "snprintf_s failed err=%d", errno);
        INIT_CHECK_ONLY_ELOG(!(setenv(envName, fdBuffer, 1) < 0), "Failed to set env %s", envName);
        INIT_LOGI("File descriptors of Service \' %s \' published", service->name);
    }
}

static int BindCpuCore(Service *service)
{
    if (service == NULL || service->cpuSet == NULL) {
        return SERVICE_SUCCESS;
    }
    if (CPU_COUNT(service->cpuSet) == 0) {
        return SERVICE_SUCCESS;
    }
#ifndef __LITEOS_A__
    int pid = getpid();
    INIT_ERROR_CHECK(sched_setaffinity(pid, sizeof(cpu_set_t), service->cpuSet) == 0,
        return SERVICE_FAILURE, "%s set affinity between process(pid=%d) with CPU's core failed", service->name, pid);
    INIT_LOGI("%s set affinity between process(pid=%d) with CPU's core successfully", service->name, pid);
#endif
    return SERVICE_SUCCESS;
}

static void ClearEnvironment(Service *service)
{
    if (strcmp(service->name, "appspawn") != 0 && strcmp(service->name, "nwebspawn") != 0) {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGTERM);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
    }
    return;
}

static int InitServiceProperties(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return -1, "Invalid parameter.");
    SetServiceEnterSandbox(service->pathArgs.argv[0], service->attribute);
    INIT_CHECK_ONLY_ELOG(SetAccessToken(service) == SERVICE_SUCCESS,
        "Set service %s access token failed", service->name);
    // deal start job
    if (service->serviceJobs.jobsName[JOB_ON_START] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_ON_START]);
    }
    ClearEnvironment(service);
    if (!IsOnDemandService(service)) {
        INIT_ERROR_CHECK(CreateServiceSocket(service) >= 0, return -1,
            "service %s exit! create socket failed!", service->name);
    }

    CreateServiceFile(service->fileCfg);
    if ((service->attribute & SERVICE_ATTR_CONSOLE)) {
        OpenConsole();
    }

    PublishHoldFds(service);
    INIT_CHECK_ONLY_ELOG(BindCpuCore(service) == SERVICE_SUCCESS,
        "binding core number failed for service %s", service->name);

    SetSystemSeccompPolicy(service);

    // permissions
    INIT_ERROR_CHECK(SetPerms(service) == SERVICE_SUCCESS, return -1,
        "service %s exit! set perms failed! err %d.", service->name, errno);

    // write pid
    INIT_ERROR_CHECK(WritePid(service) == SERVICE_SUCCESS, return -1,
        "service %s exit! write pid failed!", service->name);
    PluginExecCmdByName("setServiceContent", service->name);
    return 0;
}

void EnterServiceSandbox(Service *service)
{
    INIT_ERROR_CHECK(InitServiceProperties(service) == 0, return, "Failed init service property");
    if (service->importance != 0) {
        if (setpriority(PRIO_PROCESS, 0, service->importance) != 0) {
            INIT_LOGE("setpriority failed for %s, importance = %d, err=%d",
                service->name, service->importance, errno);
                _exit(0x7f); // 0x7f: user specified
        }
    }
#ifndef STARTUP_INIT_TEST
    char *argv[] = { (char *)"/bin/sh", NULL };
    INIT_CHECK_ONLY_ELOG(execv(argv[0], argv) == 0,
        "service %s execv sh failed! err %d.", service->name, errno);
    _exit(PROCESS_EXIT_CODE);
#endif
}

int ServiceStart(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "start service failed! null ptr.");
    INIT_ERROR_CHECK(service->pid <= 0, return SERVICE_SUCCESS, "Service %s already started", service->name);
    INIT_ERROR_CHECK(service->pathArgs.count > 0,
        return SERVICE_FAILURE, "start service %s pathArgs is NULL.", service->name);

    if (service->attribute & SERVICE_ATTR_INVALID) {
        INIT_LOGE("start service %s invalid.", service->name);
        return SERVICE_FAILURE;
    }
    struct stat pathStat = { 0 };
    service->attribute &= (~(SERVICE_ATTR_NEED_RESTART | SERVICE_ATTR_NEED_STOP));
    if (stat(service->pathArgs.argv[0], &pathStat) != 0) {
        service->attribute |= SERVICE_ATTR_INVALID;
        INIT_LOGE("start service %s invalid, please check %s.", service->name, service->pathArgs.argv[0]);
        return SERVICE_FAILURE;
    }
#ifndef OHOS_LITE
    /*
     * before service fork hooks
     */
    ServiceHookExecute(service->name, NULL, INIT_SERVICE_FORK_BEFORE);
#endif
    int pid = fork();
    if (pid == 0) {
        // fail must exit sub process
        INIT_ERROR_CHECK(InitServiceProperties(service) == 0,
            _exit(PROCESS_EXIT_CODE), "Failed init service property");
        ServiceExec(service);
        _exit(PROCESS_EXIT_CODE);
    } else if (pid < 0) {
        INIT_LOGE("start service %s fork failed!", service->name);
        return SERVICE_FAILURE;
    }
    INIT_LOGI("Service %s(pid %d) started", service->name, pid);
    service->pid = pid;
    NotifyServiceChange(service, SERVICE_STARTED);
#ifndef OHOS_LITE
    /*
     * after service fork hooks
     */
    ServiceHookExecute(service->name, (const char *)&pid, INIT_SERVICE_FORK_AFTER);
#endif
    return SERVICE_SUCCESS;
}

int ServiceStop(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "stop service failed! null ptr.");
    if (service->serviceJobs.jobsName[JOB_ON_STOP] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_ON_STOP]);
    }
    service->attribute &= ~SERVICE_ATTR_NEED_RESTART;
    service->attribute |= SERVICE_ATTR_NEED_STOP;
    if (service->pid <= 0) {
        return SERVICE_SUCCESS;
    }
    CloseServiceSocket(service);
    CloseServiceFile(service->fileCfg);
    // Service stop means service is killed by init or command(i.e stop_service) or system is rebooting
    // There is no reason still to hold fds
    if (service->fdCount != 0) {
        CloseServiceFds(service, true);
    }

    if (IsServiceWithTimerEnabled(service)) {
        ServiceStopTimer(service);
    }
    INIT_ERROR_CHECK(kill(service->pid, GetKillServiceSig(service->name)) == 0, return SERVICE_FAILURE,
        "stop service %s pid %d failed! err %d.", service->name, service->pid, errno);
    NotifyServiceChange(service, SERVICE_STOPPING);
    INIT_LOGI("stop service %s, pid %d.", service->name, service->pid);
    service->pid = -1;
    NotifyServiceChange(service, SERVICE_STOPPED);
    return SERVICE_SUCCESS;
}

static bool CalculateCrashTime(Service *service, int crashTimeLimit, int crashCountLimit)
{
    INIT_ERROR_CHECK(service != NULL && crashTimeLimit > 0 && crashCountLimit > 0,
        return false, "input params error.");
    time_t curTime = time(NULL);
    if (service->crashCnt == 0) {
        service->firstCrashTime = curTime;
        ++service->crashCnt;
        if (service->crashCnt == crashCountLimit) {
            return false;
        }
    } else if (difftime(curTime, service->firstCrashTime) > crashTimeLimit) {
        service->firstCrashTime = curTime;
        service->crashCnt = 1;
    } else {
        ++service->crashCnt;
        if (service->crashCnt >= crashCountLimit) {
            return false;
        }
    }
    return true;
}

static int ExecRestartCmd(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "Exec service failed! null ptr.");
    if (service->restartArg == NULL) {
        return SERVICE_SUCCESS;
    }
    for (int i = 0; i < service->restartArg->cmdNum; i++) {
        INIT_LOGI("ExecRestartCmd cmdLine->cmdContent %s ", service->restartArg->cmds[i].cmdContent);
        DoCmdByIndex(service->restartArg->cmds[i].cmdIndex, service->restartArg->cmds[i].cmdContent);
    }
    free(service->restartArg);
    service->restartArg = NULL;
    return SERVICE_SUCCESS;
}

static void CheckServiceSocket(Service *service)
{
    if (service->socketCfg == NULL) {
        return;
    }
    ServiceSocket *tmpSock = service->socketCfg;
    while (tmpSock != NULL) {
        if (tmpSock->sockFd <= 0) {
            INIT_LOGE("Invalid socket %s for service", service->name);
            tmpSock = tmpSock->next;
        }
        SocketAddWatcher(&tmpSock->watcher, service, tmpSock->sockFd);
        tmpSock = tmpSock->next;
    }
    return;
}

static void CheckOndemandService(Service *service)
{
    CheckServiceSocket(service);
    if (strcmp(service->name, "console") == 0) {
        if (WatchConsoleDevice(service) < 0) {
            INIT_LOGE("Failed to watch console service after it exit, mark console service invalid");
            service->attribute |= SERVICE_ATTR_INVALID;
        }
    }
}

void ServiceReap(Service *service)
{
    INIT_CHECK(service != NULL, return);
    INIT_LOGI("Reap service %s, pid %d.", service->name, service->pid);
    service->pid = -1;
    NotifyServiceChange(service, SERVICE_STOPPED);

    if (service->attribute & SERVICE_ATTR_INVALID) {
        INIT_LOGE("Reap service %s invalid.", service->name);
        return;
    }

    // If the service set timer
    // which means the timer handler will start the service
    // Init should not start it automatically.
    INIT_CHECK(IsServiceWithTimerEnabled(service) == 0, return);

    if (!IsOnDemandService(service)) {
        CloseServiceSocket(service);
    }
    CloseServiceFile(service->fileCfg);
    // stopped by system-init itself, no need to restart even if it is not one-shot service
    if (service->attribute & SERVICE_ATTR_NEED_STOP) {
        service->attribute &= (~SERVICE_ATTR_NEED_STOP);
        service->crashCnt = 0;
        return;
    }

    // for one-shot service
    if (service->attribute & SERVICE_ATTR_ONCE) {
        // no need to restart
        if (!(service->attribute & SERVICE_ATTR_NEED_RESTART)) {
            service->attribute &= (~SERVICE_ATTR_NEED_STOP);
            return;
        }
        // the service could be restart even if it is one-shot service
    }

    if (service->attribute & SERVICE_ATTR_CRITICAL) { // critical
        if (!CalculateCrashTime(service, service->crashTime, service->crashCount)) {
            INIT_LOGE("Critical service \" %s \" crashed %d times, rebooting system",
                service->name, service->crashCount);
            ExecReboot("panic");
        }
    } else if (!(service->attribute & SERVICE_ATTR_NEED_RESTART)) {
        if (!CalculateCrashTime(service, service->crashTime, service->crashCount)) {
            INIT_LOGE("Service name=%s, crash %d times, no more start.", service->name, service->crashCount);
            return;
        }
    }
    // service no need to restart if it is an ondemand service.
    if (IsOnDemandService(service)) {
        CheckOndemandService(service);
        return;
    }

    int ret = ExecRestartCmd(service);
    INIT_CHECK_ONLY_ELOG(ret == SERVICE_SUCCESS, "Failed to exec restartArg for %s", service->name);

    if (service->serviceJobs.jobsName[JOB_ON_RESTART] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_ON_RESTART]);
    }
    ret = ServiceStart(service);
    INIT_CHECK_ONLY_ELOG(ret == SERVICE_SUCCESS, "reap service %s start failed!", service->name);
    service->attribute &= (~SERVICE_ATTR_NEED_RESTART);
}

int UpdaterServiceFds(Service *service, int *fds, size_t fdCount)
{
    if (service == NULL) {
        INIT_LOGE("Invalid service info");
        return -1;
    }

    if (fdCount == 0) {
        INIT_LOGE("Update service fds with fdCount is 0, ignore.");
        return 0;
    }

    // if service->fds is NULL, allocate new memory to hold the fds
    // else if service->fds is not NULL, we will try to override it.
    // There are two cases:
    // 1) service->fdCount != fdCount:
    //  It is not easy to re-use the memory of service->fds, so we have to free the memory first
    //  then re-allocate memory to store new fds
    // 2) service->fdCount == fdCount
    //  A situation we happy to meet, just override it.

    int ret = 0;
    if (service->fdCount == fdCount) {
        // case 2
        CloseServiceFds(service, false);
        if (memcpy_s(service->fds, sizeof(int) * (fdCount + 1), fds, sizeof(int) * fdCount) != 0) {
            INIT_LOGE("Failed to copy fds to service");
            // Something wrong happened, maybe service->fds is broken, clear it.
            free(service->fds);
            service->fds = NULL;
            service->fdCount = 0;
            ret = -1;
        } else {
            service->fdCount = fdCount;
        }
    } else {
        if (service->fdCount > 0) {
            // case 1
            CloseServiceFds(service, true);
        }
        service->fds = calloc(fdCount + 1, sizeof(int));
        if (service->fds == NULL) {
            INIT_LOGE("Service \' %s \' failed to allocate memory for fds", service->name);
            ret = -1;
        } else {
            if (memcpy_s(service->fds, sizeof(int) * (fdCount + 1), fds, sizeof(int) * fdCount) != 0) {
                INIT_LOGE("Failed to copy fds to service");
                // Something wrong happened, maybe service->fds is broken, clear it.
                free(service->fds);
                service->fds = NULL;
                service->fdCount = 0;
                return -1;
            } else {
                service->fdCount = fdCount;
            }
        }
    }
    INIT_LOGI("Hold fd for service \' %s \' done", service->name);
    return ret;
}

void ServiceStopTimer(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return, "Stop timer with invalid service");
    if (IsServiceWithTimerEnabled(service)) {
        // Stop timer first
        if (service->timer) {
            LE_StopTimer(LE_GetDefaultLoop(), service->timer);
        }
        service->timer = NULL;
        DisableServiceTimer(service);
    }
}

static void ServiceTimerStartProcess(const TimerHandle handler, void *context)
{
    UNUSED(handler);
    Service *service = (Service *)context;

    if (service == NULL) {
        INIT_LOGE("Service timer process with invalid service");
        return;
    }

    // OK, service is ready to start.
    // Before start the service, stop service timer.
    // make sure it will not enter timer handler next time.
    ServiceStopTimer(service);
    int ret = ServiceStart(service);
    if (ret != SERVICE_SUCCESS) {
        INIT_LOGE("Start service \' %s \' in timer failed", service->name);
    }
}

void ServiceStartTimer(Service *service, uint64_t timeout)
{
    bool oldTimerClean = false;
    INIT_ERROR_CHECK(service != NULL, return, "Start timer with invalid service");
    // If the service already set a timer.
    // And a new request coming. close it and create a new one.
    if (IsServiceWithTimerEnabled(service)) {
        ServiceStopTimer(service);
        oldTimerClean = true;
    }
    LE_STATUS status = LE_CreateTimer(LE_GetDefaultLoop(), &service->timer, ServiceTimerStartProcess,
        (void *)service);
    if (status != LE_SUCCESS) {
        INIT_LOGE("Create service timer for service \' %s \' failed, status = %d", service->name, status);
        if (oldTimerClean) {
            INIT_LOGE("previous timer is cleared");
        }
        return;
    }
    status = LE_StartTimer(LE_GetDefaultLoop(), service->timer, timeout, 1);
    INIT_ERROR_CHECK(status == LE_SUCCESS, return,
        "Start service timer for service \' %s \' failed, status = %d", service->name, status);
    EnableServiceTimer(service);
}
