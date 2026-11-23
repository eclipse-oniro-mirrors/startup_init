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
#include "iteratenamevaluepairs_fuzzer.h"

namespace OHOS {
    static void TestNameValuePairIterator(const NAME_VALUE_PAIR *nv, void *context)
    {
        if (nv == nullptr || context == nullptr) {
            return;
        }
        int *pairCount = static_cast<int *>(context);
        if ((nv->name != nullptr) && (nv->nameEnd >= nv->name) &&
            (nv->value != nullptr) && (nv->valueEnd >= nv->value)) {
            (*pairCount)++;
        }
    }

    bool FuzzIterateNameValuePairs(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        int pairCount = 0;
        (void)IterateNameValuePairs(str.c_str(), TestNameValuePairIterator, &pairCount);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzIterateNameValuePairs(data, size);
    return 0;
}
