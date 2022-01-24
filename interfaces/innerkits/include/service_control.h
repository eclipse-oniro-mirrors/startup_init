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

enum ServiceAction {
    START = 0,
    STOP = 1,
    RESTART = 2,
};

int ServiceControlWithExtra(const char *serviceName, int action, const char *extArgv[], int extArgc);
int ServiceControl(const char *serviceName, int action);
// Service status can set "running", "stopping", "stopped", "reseting". waitTimeout(s).
int ServiceWaitForStatus(const char *serviceName, const char *status, int waitTimeout);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
