/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef SYSTEM_PARAMETER_FFI_H
#define SYSTEM_PARAMETER_FFI_H

#include <cstdint>
#include "ffi_remote_data.h"
#include "cj_common_ffi.h"

extern "C" {
    FFI_EXPORT RetDataCString FfiOHOSSysTemParameterGet(const char* key, const char* def);
    FFI_EXPORT int32_t FfiOHOSSysTemParameterSet(const char* key, const char* value);
}

#endif // SYSTEM_PARAMETER_FFI_H