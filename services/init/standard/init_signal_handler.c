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
#include <stdio.h>
#include <sys/wait.h>

#include "init_adapter.h"
#include "init_log.h"
#include "init_service_manager.h"
#include "uv.h"

uv_signal_t g_sigchldHandler;
uv_signal_t g_sigtermHandler;

static void SigHandler(int sig)
{
    switch (sig) {
        case SIGCHLD: {
            pid_t sigPID;
            int procStat = 0;
            while (1) {
                sigPID = waitpid(-1, &procStat, WNOHANG);
                if (sigPID <= 0) {
                    break;
                }
                // check child process exit status
                if (WIFSIGNALED(procStat)) {
                    INIT_LOGE("Child process %d exit with signal: %d", sigPID, WTERMSIG(procStat));
                }
                if (WIFEXITED(procStat)) {
                    INIT_LOGE("Child process %d exit with code : %d", sigPID, WEXITSTATUS(procStat));
                }
                INIT_LOGI("SigHandler, SIGCHLD received, sigPID = %d.", sigPID);
                CheckWaitPid(sigPID);
                ServiceReap(GetServiceByPid(sigPID));
            }
            break;
        }
        case SIGTERM: {
            INIT_LOGI("SigHandler, SIGTERM received.");
            StopAllServices(0);
            break;
        }
        default:
            INIT_LOGI("SigHandler, unsupported signal %d.", sig);
            break;
    }
}

static void UVSignalHandler(uv_signal_t *handle, int signum)
{
    SigHandler(signum);
}

void SignalInit(void)
{
    int ret = uv_signal_init(uv_default_loop(), &g_sigchldHandler);
    int ret1 = uv_signal_init(uv_default_loop(), &g_sigtermHandler);
    if (ret != 0 && ret1 != 0) {
        INIT_LOGW("initialize signal handler failed");
        return;
    }

    if (uv_signal_start(&g_sigchldHandler, UVSignalHandler, SIGCHLD) != 0) {
        INIT_LOGW("start SIGCHLD handler failed");
    }
    if (uv_signal_start(&g_sigtermHandler, UVSignalHandler, SIGTERM) != 0) {
        INIT_LOGW("start SIGTERM handler failed");
    }
}