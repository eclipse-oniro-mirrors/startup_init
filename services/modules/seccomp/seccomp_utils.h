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

#ifndef BASE_STARTUP_SECCOMP_UTILS_H
#define BASE_STARTUP_SECCOMP_UTILS_H
#include <stddef.h>
#include <stdint.h>

#include "beget_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


#ifndef SECCOMP_DOMAIN
#define SECCOMP_DOMAIN (BASE_DOMAIN + 0xe)
#endif
#define SECCOMP_LABEL "SECCOMP"
#define SECCOMP_LOGI(fmt, ...) STARTUP_LOGI(SECCOMP_DOMAIN, SECCOMP_LABEL, fmt, ##__VA_ARGS__)
#define SECCOMP_LOGE(fmt, ...) STARTUP_LOGE(SECCOMP_DOMAIN, SECCOMP_LABEL, fmt, ##__VA_ARGS__)
#define SECCOMP_LOGV(fmt, ...) STARTUP_LOGV(SECCOMP_DOMAIN, SECCOMP_LABEL, fmt, ##__VA_ARGS__)

#ifdef INIT_AGENT
#define SECCOMP_DUMP printf
#else
#define SECCOMP_DUMP SECCOMP_LOGI
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif