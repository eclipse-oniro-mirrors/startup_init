/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef _MODULE_REBOOT_ADP_H
#define _MODULE_REBOOT_ADP_H
#include <stdio.h>

int GetRebootReasonFromMisc(char *reason, size_t size);
int UpdateMiscMessage(const char *valueData, const char *cmd, const char *cmdExt, const char *boot);

#endif /* _MODULE_REBOOT_ADP_H */
