/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "init_utils.h"
#include <string>
#include "splitstring_fuzzer.h"

#define MAX_STR_NUMS 20

namespace OHOS {
    bool FuzzSplitString(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        char *opt[MAX_STR_NUMS] = {NULL};
        char *srcPtr = strdup(str.c_str());
        int ret = SplitString(srcPtr, " ", opt, MAX_STR_NUMS);
        if (ret != 0) {
            free(srcPtr);
            return false;
        };
        free(srcPtr);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzSplitString(data, size);
    return 0;
}