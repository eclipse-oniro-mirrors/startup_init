/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef INIT_SERVICE_BY_USERID_H
#define INIT_SERVICE_BY_USERID_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef SUPPORT_SA_MULTI_USER
typedef struct ServiceByUserId ServiceByUserId;

void StartServiceByUserId(const char *encodedValue);
void StopServiceByUserId(const char *encodedValue);
ServiceByUserId *GetServiceByUserPid(pid_t pid);
void ReportServiceByUserExit(ServiceByUserId *instance, pid_t pid, int procStat);
int ReapServiceByUserId(pid_t pid, int procStat);
void StopAllServiceByUserIdInstances(void);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
