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
#include "splitstringext_fuzzer.h"

namespace OHOS {
    constexpr int MAX_ITEM_COUNT = 64;

    bool FuzzSplitStringExt(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        if (str.empty()) {
            return true;
        }
        int returnCount = 0;
        // MAX_ITEM_COUNT: internal default 50 clamp and dynamic expand paths are reachable
        char **items = SplitStringExt(&str[0], " ", &returnCount, MAX_ITEM_COUNT);
        if (items != nullptr) {
            FreeStringVector(items, returnCount);
        }
        // maxItemCount 0: covers the parameter-guard early-return path
        (void)SplitStringExt(&str[0], " ", &returnCount, 0);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzSplitStringExt(data, size);
    return 0;
}
