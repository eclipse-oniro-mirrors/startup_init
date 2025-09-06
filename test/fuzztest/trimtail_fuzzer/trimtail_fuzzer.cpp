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

#include <string>
#include "init_utils.h"
#include "securec.h"
#include "trimtail_fuzzer.h"
static const int MIN_CHARECTER = 32;
static const int MAX_CHARECTER = 126;

namespace OHOS {
    std::vector<char> CreateSafeString(const uint8_t* data, size_t size)
    {
        if (size == 0) {
            return {'\0'};
        }

        std::vector<char> safeStr(size + 1);
        for (size_t i = 0; i < size; ++i) {
            safeStr[i] = (data[i] >= MIN_CHARECTER && data[i] <= MAX_CHARECTER) ? data[i] : 'a';
        }
        safeStr[size] = '\0';
        return safeStr;
    }

    bool FuzzTrimTail(const uint8_t* data, size_t size)
    {
        if (size == 0) {
            return false;
        }

        auto safeStr = CreateSafeString(data, size);
        char* str = safeStr.data();
        char trimChar = (size > 0 && data[0] >= MIN_CHARECTER && data[0] <= MAX_CHARECTER) ? data[0] : 'a';
        TrimTail(str, trimChar);

        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzTrimTail(data, size);
    return 0;
}
