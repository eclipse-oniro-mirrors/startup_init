/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef INIT_HISYSEVENT_H
#define INIT_HISYSEVENT_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

void ReportStartupInitReport(int64_t count);
void ReportServiceStart(char *serviceName, int64_t pid);
void ReportStartupReboot(const char *argv);
void ReportChildProcessExit(const char *serviceName, int pid, int err);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* INIT_HISYSEVENT_H */
