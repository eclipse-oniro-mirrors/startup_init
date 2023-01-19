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
#include "init_adapter.h"
#include "init_log.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "loop_event.h"

SignalHandle g_sigHandle = NULL;

static void ProcessSignal(const struct signalfd_siginfo *siginfo)
{
    switch (siginfo->ssi_signo) {
        case SIGCHLD: {
            pid_t sigPID;
            int procStat = 0;
            while (1) {
                sigPID = waitpid(-1, &procStat, WNOHANG);
                if (sigPID <= 0) {
                    break;
                }
                Service* service = GetServiceByPid(sigPID);
                // check child process exit status
                if (WIFSIGNALED(procStat)) {
                    INIT_LOGE("Child process %s(pid %d) exit with signal : %d",
                        service == NULL ? "Unknown" : service->name, sigPID, WTERMSIG(procStat));
                } else if (WIFEXITED(procStat)) {
                    INIT_LOGE("Child process %s(pid %d) exit with code : %d",
                        service == NULL ? "Unknown" : service->name, sigPID, WEXITSTATUS(procStat));
                } else {
                    INIT_LOGE("Child process %s(pid %d) exit with invalid status : %d",
                        service == NULL ? "Unknown" : service->name, sigPID, procStat);
                }
                CmdServiceProcessDelClient(sigPID);
                INIT_LOGI("SigHandler, SIGCHLD received, Service:%s pid:%d uid:%d status:%d.",
                    service == NULL ? "Unknown" : service->name,
                    sigPID, siginfo->ssi_uid, procStat);
                CheckWaitPid(sigPID);
                ServiceReap(service);
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
    if (LE_CreateSignalTask(LE_GetDefaultLoop(), &g_sigHandle, ProcessSignal) != 0) {
        INIT_LOGW("initialize signal handler failed");
        return;
    }
    if (LE_AddSignal(LE_GetDefaultLoop(), g_sigHandle, SIGCHLD) != 0) {
        INIT_LOGW("start SIGCHLD handler failed");
    }
    if (LE_AddSignal(LE_GetDefaultLoop(), g_sigHandle, SIGTERM) != 0) {
        INIT_LOGW("start SIGTERM handler failed");
    }
}
