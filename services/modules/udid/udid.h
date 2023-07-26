/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef PLUGIN_UDID_H
#define PLUGIN_UDID_H

#include "beget_ext.h"

#ifdef STARTUP_INIT_TEST
#define INIT_UDID_LOCAL_API
#else
#define INIT_UDID_LOCAL_API INIT_LOCAL_API
#endif

INIT_UDID_LOCAL_API int GetUdidFromParam(char *udid, uint32_t size);
INIT_UDID_LOCAL_API int CalcDevUdid(char *udid, uint32_t size);

#endif /* PLUGIN_UDID_H */
