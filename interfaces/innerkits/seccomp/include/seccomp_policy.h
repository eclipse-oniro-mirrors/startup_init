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

#ifndef SECCOMP_POLICY_H
#define SECCOMP_POLICY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define SYSTEM_NAME "system"
#define APPSPAWN_NAME "appspawn"
#define NWEBSPAWN_NAME "nwebspawn"
#define APP_NAME "app"

typedef enum {
    SYSTEM_SA,       // system service process
    SYSTEM_OTHERS,   // HDF process and daemon process
    APP,
    INDIVIDUAL       // process which need enable individual policy
} SeccompFilterType;

bool SetSeccompPolicyWithName(SeccompFilterType type, const char *filterName);

bool IsEnableSeccomp(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // SECCOMP_POLICY_H
