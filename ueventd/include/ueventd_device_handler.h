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

#ifndef BASE_STARTUP_INITLITE_UEVENTD_DEVICE_HANDLER_H
#define BASE_STARTUP_INITLITE_UEVENTD_DEVICE_HANDLER_H
#include "ueventd.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

void HandleBlockDeviceEvent(const struct Uevent *uevent);
void HandleOtherDeviceEvent(const struct Uevent *uevent);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_INITLITE_UEVENTD_DEVICE_HANDLER_H
