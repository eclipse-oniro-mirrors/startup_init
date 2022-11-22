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

#ifndef STARTUP_INIT_PARAM_HOOK
#define STARTUP_INIT_PARAM_HOOK
#include <stdio.h>
#include <stdint.h>

#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define SERVICE_CTL_CMD_INDEX 2
typedef struct {
    char *name; // system parameter partial name
    char *replace; // replace content if filed name match system parameter
    char *cmd; // command name
} ParamCmdInfo;

const ParamCmdInfo *GetServiceStartCtrl(size_t *size);
const ParamCmdInfo *GetServiceCtl(size_t *size);
const ParamCmdInfo *GetStartupPowerCtl(size_t *size);
const ParamCmdInfo *GetOtherSpecial(size_t *size);

typedef struct {
    struct ListNode node;
    uint32_t dataId;
    uint8_t data[0];
} ServiceExtData;

ServiceExtData *AddServiceExtData(const char *serviceName, uint32_t id, void *data, uint32_t dataLen);
void DelServiceExtData(const char *serviceName, uint32_t id);
ServiceExtData *GetServiceExtData(const char *serviceName, uint32_t id);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif