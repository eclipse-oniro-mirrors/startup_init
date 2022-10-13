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
#ifndef PARAM_TEST_STUB
#define PARAM_TEST_STUB
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "param_manager.h"
#include "param_security.h"
#include "param_utils.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef PARAM_SUPPORT_SELINUX
typedef struct ParameterNode {
    const char *paraName;
    const char *paraContext;
} ParameterNode;

typedef struct ParamContextsList {
    struct ParameterNode info;
    struct ParamContextsList *next;
} ParamContextsList;

typedef struct SrcInfo {
    int sockFd;
    struct ucred uc;
} SrcInfo;
#endif

void PrepareInitUnitTestEnv(void);
void TestSetSelinuxOps(void);
void SetTestPermissionResult(int result);
void TestSetParamCheckResult(const char *prefix, uint16_t mode, int result);
int TestCheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
int TestFreeLocalSecurityLabel(ParamSecurityLabel *srcLabel);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // PARAM_TEST_STUB_