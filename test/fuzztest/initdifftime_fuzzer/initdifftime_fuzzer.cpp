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

#include <string>

#include "init_utils.h"
#include "initdifftime_fuzzer.h"
#include "securec.h"

namespace OHOS {
    bool FuzzInitDiffTime(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        // INIT_TIMING_STAT is a pure value struct, arbitrary input bytes are safe
        INIT_TIMING_STAT stat = {};
        size_t copyLen = (str.size() < sizeof(stat)) ? str.size() : sizeof(stat);
        if (copyLen > 0) {
            memcpy_s(&stat, sizeof(stat), str.data(), copyLen);
        }
        (void)InitDiffTime(&stat);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzInitDiffTime(data, size);
    return 0;
}
