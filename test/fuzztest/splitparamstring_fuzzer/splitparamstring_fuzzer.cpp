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

#include "param_utils.h"
#include "splitparamstring_fuzzer.h"

namespace OHOS {
    static int TestSplitResult(const uint32_t *context, const char *name, const char *value)
    {
        uint32_t *counter = static_cast<uint32_t *>(const_cast<uint32_t *>(context));
        if ((counter != nullptr) && (name != nullptr) && (value != nullptr)) {
            (*counter)++;
        }
        return 0;
    }

    bool FuzzSplitParamString(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        std::string buf = str;
        if (buf.empty()) {
            return true;
        }
        static const char *exclude[] = {"persist.test.exclude", nullptr};
        uint32_t context = 0;
        // with exclude filter: covers filter-hit and filter-pass branches
        (void)SplitParamString(&buf[0], exclude, 1, TestSplitResult, &context);
        // without filter: covers normal split, quote-strip and space-trim branches
        (void)SplitParamString(&buf[0], nullptr, 0, TestSplitResult, &context);
        // null guard
        (void)SplitParamString(nullptr, exclude, 1, TestSplitResult, &context);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzSplitParamString(data, size);
    return 0;
}
