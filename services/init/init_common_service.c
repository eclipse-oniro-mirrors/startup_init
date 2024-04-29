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

#include <dlfcn.h>
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
#include "init_param.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_service_socket.h"
#include "init_utils.h"
#include "fd_holder_internal.h"
#include "loop_event.h"
#include "securec.h"
#include "service_control.h"
#include "init_group_manager.h"

#ifndef OHOS_LITE
#include "hookmgr.h"
#include "bootstage.h"
#endif

#ifdef WITH_SECCOMP
#define APPSPAWN_NAME ("appspawn")
#define NWEBSPAWN_NAME ("nwebspawn")
#endif

#ifndef TIOCSCTTY
#define TIOCSCTTY 0x540E
#endif

#define DEF_CRASH_TIME 240000 // default crash time is 240000 ms

static int SetAllAmbientCapability(void)
{
    for (int i = 0; i <= CAP_LAST_CAP; ++i) {
        if (SetAmbientCapability(i) != 0) {
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

static int SetSystemSeccompPolicy(const Service *service)
{
#ifdef WITH_SECCOMP
    if (strncmp(APPSPAWN_NAME, service->name, strlen(APPSPAWN_NAME))
        && strncmp(NWEBSPAWN_NAME, service->name, strlen(NWEBSPAWN_NAME))) {
        char cmdContent[MAX_CMD_CONTENT_LEN + 1] = {0};

        int rc = snprintf_s(cmdContent, MAX_CMD_CONTENT_LEN + 1, strlen(service->name) + 1,
                            "%s ", service->name);
        if (rc == -1) {
            return SERVICE_FAILURE;
        }

        rc = strcat_s(cmdContent, MAX_CMD_CONTENT_LEN + 1, service->pathArgs.argv[0]);
        if (rc != 0) {
            return SERVICE_FAILURE;
        }

        PluginExecCmdByName("SetSeccompPolicy", cmdContent);
    }
#endif
    return SERVICE_SUCCESS;
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

static int ServiceRestartHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    SERVICE_RESTART_CTX *serviceContext = (SERVICE_RESTART_CTX *)executionContext;
    ServiceRestartHook realHook = (ServiceRestartHook)hookInfo->hookCookie;

    realHook(serviceContext);
    return 0;
}

int InitServiceRestartHook(ServiceRestartHook hook, int hookState)
{
    HOOK_INFO info;

    info.stage = hookState;
    info.prio = 0;
    info.hook = ServiceRestartHookWrapper;
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

static int ServiceSetGid(const Service *service)
{
    if (service->servPerm.gIDCnt == 0) {
        // use uid as gid
        INIT_ERROR_CHECK(setgid(service->servPerm.uID) == 0, return SERVICE_FAILURE,
            "Service error %d %s, failed to set gid.", errno, service->name);
    }
    if (service->servPerm.gIDCnt > 0) {
        INIT_ERROR_CHECK(setgid(service->servPerm.gIDArray[0]) == 0, return SERVICE_FAILURE,
            "Service error %d %s, failed to set gid.", errno, service->name);
    }
    if (service->servPerm.gIDCnt > 1) {
        INIT_ERROR_CHECK(setgroups(service->servPerm.gIDCnt - 1, (const gid_t *)&service->servPerm.gIDArray[1]) == 0,
            return SERVICE_FAILURE,
            "Service error %d %s, failed to set gid.", errno, service->name);
    }

    return SERVICE_SUCCESS;
}

static int SetPerms(const Service *service)
{
    INIT_ERROR_CHECK(KeepCapability() == 0,
        return INIT_EKEEPCAP,
        "Service error %d %s, failed to set keep capability.", errno, service->name);

    INIT_ERROR_CHECK(ServiceSetGid(service) == SERVICE_SUCCESS,
        return INIT_EGIDSET,
        "Service error %d %s, failed to set gid.", errno, service->name);

    // set seccomp policy before setuid
    INIT_ERROR_CHECK(SetSystemSeccompPolicy(service) == SERVICE_SUCCESS,
        return INIT_ESECCOMP,
        "Service error %d %s, failed to set system seccomp policy.", errno, service->name);

    if (service->servPerm.uID != 0) {
        INIT_ERROR_CHECK(setuid(service->servPerm.uID) == 0,
            return INIT_EUIDSET,
            "Service error %d %s, failed to set uid.", errno, service->name);
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
            int ret = SetAllAmbientCapability();
            INIT_ERROR_CHECK(ret == 0,
                return INIT_ECAP,
                "Service error %d %s, failed to set ambient capability.", errno, service->name);
            return 0;
        }
        INIT_ERROR_CHECK(SetAmbientCapability(service->servPerm.caps[i]) == 0,
            return INIT_ECAP,
            "Service error %d %s, failed to set ambient capability.", errno, service->name);
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
    pid_t childPid = getpid();
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
            INIT_CHECK_ONLY_ELOG((int)fprintf(fd, "%d", childPid) > 0,
                "Failed to write %s pid:%d", service->writePidArgs.argv[i], childPid);
            (void)fclose(fd);
        } else {
            INIT_LOGE("Failed to open realPath: %s  %s errno:%d.", realPath, service->writePidArgs.argv[i], errno);
        }
        if (realPath != NULL) {
            free(realPath);
        }
        INIT_LOGV("ServiceStart writepid filename=%s, childPid=%d, ok", service->writePidArgs.argv[i], childPid);
    }
    return SERVICE_SUCCESS;
}

void CloseServiceFds(Service *service, bool needFree)
{
    INIT_ERROR_CHECK(service != NULL, return, "Service null");
    INIT_LOGI("Closing service \' %s \' fds", service->name);
    // fdCount > 0, There is no reason fds is NULL
    if (service->fdCount != 0) {
        size_t fdCount = service->fdCount;
        int *fds = service->fds;
        for (size_t i = 0; i < fdCount; i++) {
            INIT_LOGV("Closing fd: %d", fds[i]);
            if (fds[i] != -1) {
                close(fds[i]);
                fds[i] = -1;
            }
        }
    }
    service->fdCount = 0;
    if (needFree && service->fds != NULL) {
        free(service->fds);
        service->fds = NULL;
    }
}

static int PublishHoldFds(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return INIT_EPARAMETER, "Publish hold fds with invalid service");
    if (service->fdCount == 0 || service->fds == NULL) {
        return 0;
    }
    char fdBuffer[MAX_FD_HOLDER_BUFFER] = {};
    char envName[MAX_BUFFER_LEN] = {};
    int ret = snprintf_s(envName, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, ENV_FD_HOLD_PREFIX"%s", service->name);
    INIT_ERROR_CHECK(ret >= 0, return INIT_EFORMAT,
        "Service error %d %s, failed to format string for publish", ret, service->name);

    size_t pos = 0;
    for (size_t i = 0; i < service->fdCount; i++) {
        int fd = dup(service->fds[i]);
        if (fd < 0) {
            INIT_LOGW("Service warning %d %s, failed to dup fd for publish", errno, service->name);
            continue;
        }
        ret = snprintf_s((char *)fdBuffer + pos, sizeof(fdBuffer) - pos, sizeof(fdBuffer) - 1, "%d ", fd);
        INIT_ERROR_CHECK(ret >= 0, return INIT_EFORMAT,
            "Service error %d %s, failed to format fd for publish", ret, service->name);
        pos += ret;
    }
    fdBuffer[pos - 1] = '\0'; // Remove last ' '
    INIT_LOGI("Service %s publish fd [%s]", service->name, fdBuffer);
    return setenv(envName, fdBuffer, 1);
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

static void SetServiceEnv(Service *service)
{
    INIT_CHECK(service->env != NULL, return);
    for (int i = 0; i < service->envCnt; i++) {
        if (strlen(service->env[i].name) > 0 && strlen(service->env[i].value) > 0) {
            int ret = setenv(service->env[i].name, service->env[i].value, 1);
            INIT_CHECK_ONLY_ELOG(ret != 0, "set service:%s env:%s failed!", service->name, service->env[i].name);
        }
    }
}

static int InitServiceProperties(Service *service, const ServiceArgs *pathArgs)
{
    INIT_ERROR_CHECK(service != NULL, return -1, "Invalid parameter.");
    int ret = SetServiceEnterSandbox(service, pathArgs->argv[0]);
    if (ret != 0) {
        INIT_LOGW("Service warning %d %s, failed to enter sandbox.", ret, service->name);
        service->lastErrno = INIT_ESANDBOX;
    }
    ret = SetAccessToken(service);
    if (ret != 0) {
        INIT_LOGW("Service warning %d %s, failed to set access token.", ret, service->name);
        service->lastErrno = INIT_EACCESSTOKEN;
    }

    SetServiceEnv(service);

    // deal start job
    if (service->serviceJobs.jobsName[JOB_ON_START] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_ON_START]);
    }
    ClearEnvironment(service);
    if (!IsOnDemandService(service)) {
        INIT_ERROR_CHECK(CreateSocketForService(service) == 0, service->lastErrno = INIT_ESOCKET;
            return INIT_ESOCKET, "Service error %d %s, failed to create service socket.", errno, service->name);
    }
    SetSocketEnvForService(service);
    INIT_ERROR_CHECK(CreateServiceFile(service) == 0, service->lastErrno = INIT_EFILE;
        return INIT_EFILE, "Service error %d %s, failed to create service file.", errno, service->name);

    if ((service->attribute & SERVICE_ATTR_CONSOLE)) {
        INIT_ERROR_CHECK(OpenConsole() == 0, service->lastErrno = INIT_ECONSOLE;
            return INIT_ECONSOLE, "Service error %d %s, failed to open console.", errno, service->name);
    }
    if ((service->attribute & SERVICE_ATTR_KMSG)) {
        OpenKmsg();
    }

    INIT_ERROR_CHECK(PublishHoldFds(service) == 0, service->lastErrno = INIT_EHOLDER;
        return INIT_EHOLDER, "Service error %d %s, failed to publish fd", errno, service->name);

    INIT_CHECK_ONLY_ELOG(BindCpuCore(service) == SERVICE_SUCCESS,
        "Service warning %d %s, failed to publish fd", errno, service->name);

    // permissions
    ret = SetPerms(service);
    INIT_ERROR_CHECK(ret == SERVICE_SUCCESS, service->lastErrno = ret;
        return ret, "Service error %d %s, failed to set permissions.", ret, service->name);

    // write pid
    INIT_ERROR_CHECK(WritePid(service) == SERVICE_SUCCESS, service->lastErrno = INIT_EWRITEPID;
        return INIT_EWRITEPID, "Service error %d %s, failed to write pid.", errno, service->name);

    PluginExecCmdByName("setServiceContent", service->name);
    return 0;
}

void EnterServiceSandbox(Service *service)
{
    INIT_ERROR_CHECK(InitServiceProperties(service, &service->pathArgs) == 0, return, "Failed init service property");
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

static void AddUpdateList(ServiceArgs *args, char *updateList)
{
    char** argvOrig = args->argv;
    const int paramCount = 2; // -u updatelist
    args->argv = (char **)malloc((args->count + paramCount) * sizeof(char *));
    INIT_ERROR_CHECK(args->argv != NULL, return, "Failed to malloc for argv");
    int i = 0;
    for (; i < args->count + 1; ++i) {
        if (i == args->count - 1) {
            args->argv[i] = "-u";
            args->argv[++i] = updateList;
            break;
        } else {
            args->argv[i] = argvOrig[i];
        }
    }
    args->argv[++i] = NULL;
    args->count += paramCount;
    free(argvOrig);
}

static void CheckModuleUpdate(ServiceArgs *args)
{
    INIT_LOGI("CheckModuleUpdate start");
    void *handle = dlopen("libmodule_update.z.so", RTLD_NOW);
    INIT_ERROR_CHECK(handle != NULL, return, "dlopen module update lib failed with error:%s", dlerror());
    typedef char* (*ExtFunc)(int, char **);
    ExtFunc func = (ExtFunc)dlsym(handle, "CheckModuleUpdate");
    if (func == NULL) {
        INIT_LOGE("dlsym get func failed with error:%s", dlerror());
    } else {
        char *updateList = func(args->count, args->argv);
        INIT_LOGI("update list: %s", updateList);
        if (updateList != NULL) {
            AddUpdateList(args, updateList);
        } else {
            INIT_LOGW("no update list");
        }
    }
    INIT_LOGI("CheckModuleUpdate end");
}

#ifdef IS_DEBUG_VERSION
static bool ServiceNeedDebug(char *name)
{
    char nameValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int nameLen = PARAM_VALUE_LEN_MAX;
    // specify process debugging: param set llvm.debug.service.name "service name"
    if (SystemReadParam("llvm.debug.service.name", nameValue, &nameLen) == 0) {
        if (strcmp(nameValue, name) == 0) {
            return true;
        }
    }

    char debugValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int debugLen = PARAM_VALUE_LEN_MAX;
    // multi process debugging: param set llvm.debug.service.all 1
    if (SystemReadParam("llvm.debug.service.all", debugValue, &debugLen) == 0) {
        if (strcmp(debugValue, "1") == 0) {
            return true;
        }
    }
    return false;
}

static bool IsDebuggableVersion(void)
{
    char secureValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int secureLen = PARAM_VALUE_LEN_MAX;
    char debugValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int debugLen = PARAM_VALUE_LEN_MAX;
    // the image is debuggable only when secureValue is 0 and debugValue is 1
    if (SystemReadParam("const.secure", secureValue, &secureLen) == 0 &&
        SystemReadParam("const.debuggable", debugValue, &debugLen) == 0) {
        if (strcmp(secureValue, "0") == 0 &&
            strcmp(debugValue, "1") == 0) {
            return true;
        }
    }
    return false;
}

static int32_t CheckTraceStatus(void)
{
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd == -1) {
        INIT_LOGE("lldb: open /proc/self/status error: %{public}d", errno);
        return (-errno);
    }

    char data[1024] = { 0 };  // 1024 data len
    ssize_t dataNum = read(fd, data, sizeof(data));
    if (close(fd) < 0) {
        INIT_LOGE("lldb: close fd error: %{public}d", errno);
        return (-errno);
    }

    if (dataNum <= 0) {
        INIT_LOGE("lldb: fail to read data");
        return -1;
    }

    const char* tracerPid = "TracerPid:\t";
    data[1023] = '\0'; // 1023 data last position
    char *traceStr = strstr(data, tracerPid);
    if (traceStr == NULL) {
        INIT_LOGE("lldb: fail to find %{public}s", tracerPid);
        return -1;
    }
    char *separator = strchr(traceStr, '\n');
    if (separator == NULL) {
        INIT_LOGE("lldb: fail to find line break");
        return -1;
    }

    int len = separator - traceStr - strlen(tracerPid);
    char pid = *(traceStr + strlen(tracerPid));
    if (len > 1 || pid != '0') {
        return 0;
    }
    return -1;
}

static int32_t WaitForDebugger(void)
{
    uint32_t count = 0;
    while (CheckTraceStatus() != 0) {
        usleep(1000 * 100); // sleep 1000 * 100 microsecond
        count++;
        // remind users to connect to the debugger every 60 * 10 times
        if (count % (10 * 60) == 0) {
            INIT_LOGI("lldb: wait for debugger, please attach the process");
            count = 0;
        }
    }
    return 0;
}
#endif

static void CloseOnDemandServiceFdForOtherService(Service *service)
{
    ServiceSocket *tmpSockNode = GetOnDemandSocketList();
    while (tmpSockNode != NULL) {
        if (tmpSockNode->service != service) {
            ServiceSocket *tmpCloseSocket = tmpSockNode;
            // If the service is not the OnDemand service itself, will close the OnDemand service socket fd
            while (tmpCloseSocket != NULL) {
                close(tmpCloseSocket->sockFd);
                tmpCloseSocket = tmpCloseSocket->next;
            }
        }
        tmpSockNode = tmpSockNode->nextNode;
    }
}

static void RunChildProcess(Service *service, ServiceArgs *pathArgs)
{
    // set selinux label by context
    if (service->context.type != INIT_CONTEXT_MAIN && SetSubInitContext(&service->context, service->name) != 0) {
        service->lastErrno = INIT_ECONTENT;
    }

    CloseOnDemandServiceFdForOtherService(service);

    if (service->attribute & SERVICE_ATTR_MODULE_UPDATE) {
        CheckModuleUpdate(pathArgs);
    }
#ifdef IS_DEBUG_VERSION
    // only the image is debuggable and need debug, then wait for debugger
    if (ServiceNeedDebug(service->name) && IsDebuggableVersion()) {
        WaitForDebugger();
    }
#endif
    // fail must exit sub process
    int ret = InitServiceProperties(service, pathArgs);
    INIT_ERROR_CHECK(ret == 0,
        _exit(service->lastErrno), "Service error %d %s, failed to set properties", ret, service->name);

    (void)ServiceExec(service, pathArgs);
    _exit(service->lastErrno);
}

int ServiceStart(Service *service, ServiceArgs *pathArgs)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "ServiceStart failed! null ptr.");
    INIT_INFO_CHECK(service->pid <= 0, return SERVICE_SUCCESS, "ServiceStart already started:%s", service->name);
    INIT_ERROR_CHECK(pathArgs != NULL && pathArgs->count > 0,
        return SERVICE_FAILURE, "ServiceStart pathArgs is NULL:%s", service->name);
    struct timespec startingTime;
    clock_gettime(CLOCK_REALTIME, &startingTime);
    INIT_LOGI("ServiceStart starting:%s", service->name);
    if (service->attribute & SERVICE_ATTR_INVALID) {
        INIT_LOGE("ServiceStart invalid:%s", service->name);
        return SERVICE_FAILURE;
    }
    struct stat pathStat = { 0 };
    service->attribute &= (~(SERVICE_ATTR_NEED_RESTART | SERVICE_ATTR_NEED_STOP));
    if (stat(pathArgs->argv[0], &pathStat) != 0) {
        service->attribute |= SERVICE_ATTR_INVALID;
        service->lastErrno = INIT_EPATH;
        INIT_LOGE("ServiceStart pathArgs invalid, please check %s,%s", service->name, service->pathArgs.argv[0]);
        return SERVICE_FAILURE;
    }
#ifndef OHOS_LITE
    // before service fork hooks
    ServiceHookExecute(service->name, NULL, INIT_SERVICE_FORK_BEFORE);
#endif
    // pre-start job
    if (service->serviceJobs.jobsName[JOB_PRE_START] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_PRE_START]);
    }
    struct timespec prefork;
    clock_gettime(CLOCK_REALTIME, &prefork);
    int pid = fork();
    if (pid == 0) {
        RunChildProcess(service, pathArgs);
    } else if (pid < 0) {
        INIT_LOGE("ServiceStart error failed to fork %d, %s", errno, service->name);
        service->lastErrno = INIT_EFORK;
        return SERVICE_FAILURE;
    }
    struct timespec startedTime;
    clock_gettime(CLOCK_REALTIME, &startedTime);
    INIT_LOGI("ServiceStart started info %s(pid %d uid %d)", service->name, pid, service->servPerm.uID);
    INIT_LOGI("starttime:%ld-%ld,preforktime:%ld-%ld,startedtime:%ld-%ld",
              startingTime.tv_sec, startingTime.tv_nsec, prefork.tv_sec,
              prefork.tv_nsec, startedTime.tv_sec, startedTime.tv_nsec);
    service->pid = pid;
    NotifyServiceChange(service, SERVICE_STARTED);
#ifndef OHOS_LITE
    // after service fork hooks
    ServiceHookExecute(service->name, (const char *)&pid, INIT_SERVICE_FORK_AFTER);
#endif
    return SERVICE_SUCCESS;
}

int ServiceStop(Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "stop service failed! null ptr.");
    NotifyServiceChange(service, SERVICE_STOPPING);
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
    INIT_LOGI("stop service %s, pid %d.", service->name, service->pid);
    service->pid = -1;
    NotifyServiceChange(service, SERVICE_STOPPED);
    return SERVICE_SUCCESS;
}

static bool CalculateCrashTime(Service *service, int crashTimeLimit, int crashCountLimit)
{
    INIT_ERROR_CHECK(service != NULL && crashTimeLimit > 0 && crashCountLimit > 0,
        return false, "input params error.");
    struct timespec curTime = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &curTime);
    struct timespec crashTime = {service->firstCrashTime, 0};
    if (service->crashCnt == 0) {
        service->firstCrashTime = curTime.tv_sec;
        ++service->crashCnt;
        if (service->crashCnt == crashCountLimit) {
            return false;
        }
    } else if (IntervalTime(&crashTime, &curTime) > crashTimeLimit) {
        service->firstCrashTime = curTime.tv_sec;
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
        DoCmdByIndex(service->restartArg->cmds[i].cmdIndex, service->restartArg->cmds[i].cmdContent, NULL);
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
        AddSocketWatcher(&tmpSock->watcher, service, tmpSock->sockFd);
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

static void ServiceReapHookExecute(Service *service)
{
#ifndef OHOS_LITE
    HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_REAP, (void*)service, NULL);
#endif
}

void ServiceReap(Service *service)
{
    INIT_CHECK(service != NULL, return);
    INIT_LOGI("ServiceReap info %s pid %d.", service->name, service->pid);
    NotifyServiceChange(service, SERVICE_STOPPED);
    int tmp = service->pid;
    service->pid = -1;

    if (service->attribute & SERVICE_ATTR_INVALID) {
        INIT_LOGE("ServiceReap error invalid service %s", service->name);
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

    if (service->attribute & SERVICE_ATTR_PERIOD) { //period
        ServiceStartTimer(service, service->period);
        return;
    }

    // service no need to restart if it is an ondemand service.
    if (IsOnDemandService(service)) {
        CheckOndemandService(service);
        return;
    }

    if (service->attribute & SERVICE_ATTR_CRITICAL) { // critical
        if (!CalculateCrashTime(service, service->crashTime, service->crashCount)) {
            INIT_LOGE("ServiceReap error critical service crashed %s %d", service->name, service->crashCount);
            service->pid = tmp;
            ServiceReapHookExecute(service);
            service->pid = -1;
            ExecReboot("panic");
        }
    } else if (!(service->attribute & SERVICE_ATTR_NEED_RESTART)) {
        if (!CalculateCrashTime(service, service->crashTime, service->crashCount)) {
            INIT_LOGI("ServiceReap start failed! will reStart %d second later", service->name, service->crashTime);
            service->crashCnt = 0;
            ServiceStartTimer(service, DEF_CRASH_TIME);
            return;
        }
    }

    int ret = ExecRestartCmd(service);
    INIT_CHECK_ONLY_ELOG(ret == SERVICE_SUCCESS, "ServiceReap Failed to exec restartArg for %s", service->name);

    if (service->serviceJobs.jobsName[JOB_ON_RESTART] != NULL) {
        DoJobNow(service->serviceJobs.jobsName[JOB_ON_RESTART]);
    }
    ret = ServiceStart(service, &service->pathArgs);
    INIT_CHECK_ONLY_ELOG(ret == SERVICE_SUCCESS, "ServiceReap ServiceStart failed %s", service->name);
    service->attribute &= (~SERVICE_ATTR_NEED_RESTART);
}

int UpdaterServiceFds(Service *service, int *fds, size_t fdCount)
{
    if (service == NULL || fds == NULL) {
        INIT_LOGE("Invalid service info or fds");
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
        INIT_ERROR_CHECK(fdCount <= MAX_HOLD_FDS, return -1, "Invalid fdCount %d", fdCount);
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
    int ret = ServiceStart(service, &service->pathArgs);
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
