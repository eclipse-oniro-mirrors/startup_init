/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "control_fd.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "init_log.h"

static void ProcessSandboxControlFd(uint16_t type, const char *serviceCmd)
{
    if ((type != ACTION_SANDBOX) || (serviceCmd == NULL)) {
        INIT_LOGE("Invaild parameter");
        return;
    }
    Service *service  = GetServiceByName(serviceCmd);
    if (service == NULL) {
        INIT_LOGE("Failed get service %s", serviceCmd);
        return;
    }
    EnterServiceSandbox(service);
    return;
}

static void ProcessDumpServiceControlFd(uint16_t type, const char *serviceCmd)
{
    if ((type != ACTION_DUMP) || (serviceCmd == NULL)) {
        return;
    }
    Service *service  = GetServiceByName(serviceCmd);

    if (service == NULL) {
#ifdef OHOS_SERVICE_DUMP
        DumpAllServices();
    } else {
        DumpOneService(service);
#endif
    }
    return;
}

static void ProcessParamShellControlFd(uint16_t type, const char *serviceCmd)
{
    if ((type != ACTION_PARAM_SHELL) || (serviceCmd == NULL)) {
        return;
    }
    (void)setuid(2000); // 2000 shell group
    (void)setgid(2000); // 2000 shell group
    char *args[] = {(char *)serviceCmd, NULL};
    int ret = execv(args[0], args);
    if (ret < 0) {
        INIT_LOGE("error on exec %d \n", errno);
        exit(-1);
    }
    exit(0);
}

void ProcessControlFd(uint16_t type, const char *serviceCmd, const void *context)
{
    if ((type >= ACTION_MAX) || (serviceCmd == NULL)) {
        return;
    }
    switch (type) {
        case ACTION_SANDBOX :
            ProcessSandboxControlFd(type, serviceCmd);
            break;
        case ACTION_DUMP :
            ProcessDumpServiceControlFd(type, serviceCmd);
            break;
        case ACTION_PARAM_SHELL :
            ProcessParamShellControlFd(type, serviceCmd);
            break;
        default :
            INIT_LOGW("Unknown control fd type.");
            break;
    }
}

void InitControlFd(void)
{
    CmdServiceInit(INIT_CONTROL_FD_SOCKET_PATH, ProcessControlFd);
    return;
}
