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

#include "loadfscryptpolicy_fuzzer.h"
#include "fs_manager/fs_manager.h"

namespace OHOS {
    bool FuzzLoadFscryptPolicy(const uint8_t* data, size_t size)
    {
        bool result = false;
        char buff[1024] = {0};
        size_t len = sizeof(buff);
        if (!LoadFscryptPolicy(buff, len)) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzLoadFscryptPolicy(data, size);
    return 0;
}
