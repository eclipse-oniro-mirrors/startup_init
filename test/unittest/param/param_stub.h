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
#ifndef PARAM_TEST_STUB_
#define PARAM_TEST_STUB_
#include "param_libuvadp.h"
#include "param_manager.h"
#include "param_security.h"
#include "param_service.h"
#include "param_utils.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
extern int RunParamCommand(int argc, char *argv[]);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

void TestClient(int index);
int TestEncodeSecurityLabel(const ParamSecurityLabel *srcLabel, char *buffer, uint32_t *bufferSize);
int TestDecodeSecurityLabel(ParamSecurityLabel **srcLabel, const char *buffer, uint32_t bufferSize);
int TestCheckParamPermission(const ParamSecurityLabel *srcLabel, const ParamAuditData *auditData, uint32_t mode);
int TestFreeLocalSecurityLabel(ParamSecurityLabel *srcLabel);

#endif // PARAM_TEST_STUB_