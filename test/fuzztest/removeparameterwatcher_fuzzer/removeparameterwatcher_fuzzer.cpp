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

#include "removeparameterwatcher_fuzzer.h"
#include <string>
#include "parameter.h"
#include "fuzz_utils.h"

static void HandleParamChange(const char *key, const char *value, void *context)
{
    if (key == nullptr || value == nullptr) {
        return;
    }
    printf("Receive parameter change %s %s \n", key, value);
}

namespace OHOS {
    bool FuzzRemoveParameterWatcher(const uint8_t* data, size_t size)
    {
        bool result = false;
        std::string str(reinterpret_cast<const char*>(data), size);
        CloseStdout();
        if (!RemoveParameterWatcher(str.c_str(), HandleParamChange, NULL)) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzRemoveParameterWatcher(data, size);
    return 0;
}
