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
#include "ohstrarraygetindex_fuzzer.h"

namespace OHOS {
    bool FuzzOHStrArrayGetIndex(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        static const char *array[] = {"init", "ueventd", "param", nullptr};
        static const char *extend[] = {"watcher", nullptr};
        // fuzz-controlled target: covers no-match and both ignoreCase values
        (void)OH_StrArrayGetIndex(array, str.c_str(), 0);
        (void)OH_StrArrayGetIndex(array, str.c_str(), 1);
        // fixed probes: covers exact match, extend match and extend-null branches
        (void)OH_StrArrayGetIndex(array, "param", 0);
        (void)OH_ExtendableStrArrayGetIndex(array, "param", 1, nullptr);
        (void)OH_ExtendableStrArrayGetIndex(array, "watcher", 0, extend);
        (void)OH_ExtendableStrArrayGetIndex(array, "watcher", 1, extend);
        (void)OH_ExtendableStrArrayGetIndex(array, "watcher", 0, nullptr);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzOHStrArrayGetIndex(data, size);
    return 0;
}
