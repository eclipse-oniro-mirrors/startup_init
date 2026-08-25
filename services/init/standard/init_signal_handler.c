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
#include <signal.h>
#include <sys/wait.h>

#include "control_fd.h"
#include "init.h"
#include "init_adapter.h"
#include "init_log.h"
#include "init_param.h"
#include "init_context.h"
#include "init_service_manager.h"
#include "loop_event.h"
#include "crash_handler.h"
#include "init_hisysevent.h"
#include "init_service.h"
#ifdef SUPPORT_SA_MULTI_USER
#include "init_service_by_userid.h"
#endif

static SignalHandle g_sigHandle = NULL;


#ifdef INIT_FEATURE_SUPPORT_SASPAWN
static void SetSaSpawnFailedTag(Service *service)
{
    if (service == NULL) {
        return;
    }
    ServiceResetSupportSaSpawn(service);
}
#endif

#ifdef SUPPORT_SA_MULTI_USER
static bool ReapByUserChild(pid_t pid, int procStat, const struct signalfd_siginfo *siginfo)
{
    ServiceByUserId *instance = GetServiceByUserPid(pid);
    if (instance == NULL) {
        return false;
    }
    CmdServiceProcessDelClient(pid);
    StopSubInit(pid);
    ReportServiceByUserExit(instance, pid, procStat);
    INIT_LOGW("By-user service SIGCHLD received, pid:%d uid:%d status:%d.",
        pid, siginfo->ssi_uid, procStat);
    CheckWaitPid(pid);
    (void)ReapServiceByUserId(pid, procStat);
    return true;
}
#endif

INIT_STATIC void ReportChildExitStatus(Service *service, const char *serviceName, pid_t pid,
    int procStat, bool isSaspawn)
{
    if (WIFSIGNALED(procStat)) {
        INIT_LOGW("Child process %s(pid %d) exit with signal : %d", serviceName, pid, WTERMSIG(procStat));
        ReportChildProcessExit(serviceName, pid, WTERMSIG(procStat),
            isSaspawn ? SERVICES_EXIT_INFO_IS_SASPAWN : SERVICES_EXIT_INFO_NOT_SASPAWN);
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
        SetSaSpawnFailedTag(service);
#endif
        return;
    }
    if (!WIFEXITED(procStat)) {
        return;
    }
    INIT_LOGW("Child process %s(pid %d) exit with code : %d", serviceName, pid, WEXITSTATUS(procStat));
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
    if (WEXITSTATUS(procStat) == INIT_SASPAWN) {
        SetSaSpawnFailedTag(service);
    }
#endif
    if (service != NULL) {
        service->lastErrno = WEXITSTATUS(procStat);
    }
    if (isSaspawn) {
        ReportChildProcessExit(serviceName, pid, WEXITSTATUS(procStat), SERVICES_EXIT_INFO_IS_SASPAWN);
    }
}

static pid_t HandleSigChild(const struct signalfd_siginfo *siginfo)
{
    int procStat = 0;
    bool isSaspawn = false;
    pid_t sigPID = waitpid(-1, &procStat, WNOHANG);
    if (sigPID <= 0) {
        return sigPID;
    }
#ifdef SUPPORT_SA_MULTI_USER
    if (ReapByUserChild(sigPID, procStat, siginfo)) {
        return sigPID;
    }
#endif
    Service* service = GetServiceByPid(sigPID);
    const char *serviceName = (service == NULL) ? "Unknown" : service->name;
    (void)ProcessServiceDied(service);

#ifdef INIT_FEATURE_SUPPORT_SASPAWN
    if (service != NULL) {
        isSaspawn = ((service->attribute & SERVICE_ATTR_SASPAWN) == SERVICE_ATTR_SASPAWN);
        INIT_LOGI("Is the service supported Saspawn %d", isSaspawn);
    }
#endif

    ReportChildExitStatus(service, serviceName, sigPID, procStat, isSaspawn);
    CmdServiceProcessDelClient(sigPID);
    StopSubInit(sigPID);
    INIT_LOGW("Service warning %s, SIGCHLD received, pid:%d uid:%d status:%d.",
        serviceName, sigPID, siginfo->ssi_uid, procStat);
    CheckWaitPid(sigPID);
    ServiceReap(service);
    return sigPID;
}

INIT_STATIC void ProcessSignal(const struct signalfd_siginfo *siginfo)
{
    switch (siginfo->ssi_signo) {
        case SIGCHLD: {
            while (HandleSigChild(siginfo) > 0) {
                ;
            }
            break;
        }
        case SIGTERM: {
            INIT_LOGI("SigHandler, SIGTERM received.");
            SystemWriteParam("startup.device.ctl", "stop");
            // exec reboot use toybox reboot cmd
            ExecReboot("reboot");
            break;
        }
        default:
            INIT_LOGI("SigHandler, unsupported signal %d.", siginfo->ssi_signo);
            break;
    }
}

void SignalInit(void)
{
    if (LE_CreateSignalTask(LE_GetDefaultLoop(), &g_sigHandle, ProcessSignal) == 0) {
        if (LE_AddSignal(LE_GetDefaultLoop(), g_sigHandle, SIGCHLD) != 0) {
            INIT_LOGW("start SIGCHLD handler failed");
        }
        if (LE_AddSignal(LE_GetDefaultLoop(), g_sigHandle, SIGTERM) != 0) {
            INIT_LOGW("start SIGTERM handler failed");
        }
    }
    InstallLocalSignalHandler();
}
