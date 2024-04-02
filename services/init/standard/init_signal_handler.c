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

static SignalHandle g_sigHandle = NULL;

static pid_t HandleSigChild(const struct signalfd_siginfo *siginfo)
{
    int procStat = 0;
    pid_t sigPID = waitpid(-1, &procStat, WNOHANG);
    if (sigPID <= 0) {
        return sigPID;
    }
    Service* service = GetServiceByPid(sigPID);
    const char *serviceName = (service == NULL) ? "Unknown" : service->name;

    // check child process exit status
    if (WIFSIGNALED(procStat)) {
        INIT_LOGW("Child process %s(pid %d) exit with signal : %d", serviceName, sigPID, WTERMSIG(procStat));
    } else if (WIFEXITED(procStat)) {
        INIT_LOGW("Child process %s(pid %d) exit with code : %d", serviceName, sigPID, WEXITSTATUS(procStat));
        if (service != NULL) {
            service->lastErrno = WEXITSTATUS(procStat);
        }
    }
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
