/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BEGET_UTILS_API_H
#define BEGET_UTILS_API_H

#include <inttypes.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define STARTUP_SERVICE_CTL "startup.service.ctl"

typedef enum {
    SERVICE_IDLE = 0, // service add
    SERVICE_STARTING, // service start
    SERVICE_STARTED, // 2 service ok running
    SERVICE_READY, // 3 service ok running
    SERVICE_STOPPING,
    SERVICE_STOPPED, // 5
    SERVICE_ERROR,
    SERVICE_SUSPENDED,
    SERVICE_FREEZED,
    SERVICE_DISABLED,
    SERVICE_CRITICAL
} ServiceStatus;

enum ServiceAction {
    START = 0,
    STOP = 1,
    RESTART = 2,
};

int ServiceControlWithExtra(const char *serviceName, int action, const char *extArgv[], int extArgc);
int ServiceControl(const char *serviceName, int action);
int ServiceWaitForStatus(const char *serviceName, ServiceStatus status, int waitTimeout);
int ServiceSetReady(const char *serviceName);
int StartServiceByTimer(const char *serviceName, uint64_t timeout);
int StopServiceTimer(const char *serviceName);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
