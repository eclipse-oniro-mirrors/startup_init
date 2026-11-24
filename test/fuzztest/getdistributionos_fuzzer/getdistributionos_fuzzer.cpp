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

#include "parameter.h"
#include "getdistributionos_fuzzer.h"

namespace OHOS {
    bool FuzzGetDistributionOS(const uint8_t* data, size_t size)
    {
        // read-only distribution-info getters, no fuzz-controlled state involved
        (void)data;
        (void)size;
        (void)GetDistributionOSName();
        (void)GetDistributionOSVersion();
        (void)GetDistributionOSApiVersion();
        (void)GetDistributionOSApiName();
        (void)GetDistributionOSReleaseType();
        (void)GetPerformanceClass();
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetDistributionOS(data, size);
    return 0;
}
