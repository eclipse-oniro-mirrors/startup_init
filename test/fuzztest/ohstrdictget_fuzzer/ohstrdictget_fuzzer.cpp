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
#include "ohstrdictget_fuzzer.h"

namespace OHOS {
    struct StrDictEntry {
        const char *key;
        const char *value;
    };

    bool FuzzOHStrDictGet(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        const char *target = str.c_str();
        static const StrDictEntry dict[] = {
            {"init", "value1"},
            {"param", "value2"},
            {nullptr, nullptr},
        };
        static const StrDictEntry extendDict[] = {
            {"watcher", "value3"},
            {nullptr, nullptr},
        };
        int dictSize = static_cast<int>(sizeof(StrDictEntry));
        // wrapper without extend: covers stride walk, match, ignoreCase and no-match branches
        (void)OH_StrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), dictSize, target, 0);
        (void)OH_StrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), dictSize, target, 1);
        // guard branches: null dict, invalid stride
        (void)OH_StrDictGet(nullptr, dictSize, target, 0);
        (void)OH_StrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), -1, target, 0);
        (void)OH_StrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), 1, target, 0);
        // extendable: covers extend-match, extend-no-match and extend-null branches
        (void)OH_ExtendableStrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), dictSize,
            target, 0, reinterpret_cast<void **>(const_cast<StrDictEntry *>(extendDict)));
        (void)OH_ExtendableStrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), dictSize,
            target, 1, reinterpret_cast<void **>(const_cast<StrDictEntry *>(extendDict)));
        (void)OH_ExtendableStrDictGet(reinterpret_cast<void **>(const_cast<StrDictEntry *>(dict)), dictSize,
            target, 0, nullptr);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzOHStrDictGet(data, size);
    return 0;
}
