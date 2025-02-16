/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "aclgetdisksn_fuzzer.h"
#include <string>
#include "parameter.h"

namespace OHOS {
    bool FuzzAclGetDiskSN(const uint8_t* data, size_t size)
    {
        bool result = false;
        char diskSN[65] = {0};
        int len = sizeof(diskSN);
        if (AclGetDiskSN(diskSN, len) == 0) {
            result = true;
        };
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzAclGetDiskSN(data, size);
    return 0;
}
