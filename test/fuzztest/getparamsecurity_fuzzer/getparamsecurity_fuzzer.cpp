/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "param_manager.h"
#include "param_security.h"
#include "getparamsecurity_fuzzer.h"

namespace OHOS {
    bool FuzzGetParamSecurity(const uint8_t* data, size_t size)
    {
        // read-only accessors, no fuzz-controlled state involved
        (void)data;
        (void)size;
        (void)GetParamSecurityOps(PARAM_SECURITY_DAC);
        (void)GetParamSecurityOps(PARAM_SECURITY_MAX);
        (void)GetParamSecurityOps(-1);
        (void)GetParamSecurityLabel();
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetParamSecurity(data, size);
    return 0;
}
