/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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

#include "init_adapter.h"
#include "init_service_manager.h"

void ReapService(Service *service)
{
    if (service == NULL) {
        return;
    }
    if (service->attribute & SERVICE_ATTR_IMPORTANT) {
        // important process exit, need to reboot system
        service->pid = -1;
        StopAllServices(0, NULL, 0, NULL);
        RebootSystem();
    }
    ServiceReap(service);
}

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
                CheckWaitPid(sigPID);
                ReapService(GetServiceByPid(sigPID));
            }
            break;
        }
        case SIGTERM: {
            StopAllServices(0, NULL, 0, NULL);
            break;
        }
        default:
            break;
    }
}

void SignalInit(void)
{
    struct sigaction act;
    act.sa_handler = SigHandler;
    act.sa_flags = SA_RESTART;
    (void)sigfillset(&act.sa_mask);

    sigaction(SIGCHLD, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}