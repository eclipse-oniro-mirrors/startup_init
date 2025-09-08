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
#include "convertmicrosecondtosecond_fuzzer.h"
#include "securec.h"

extern "C" {
float ConvertMicrosecondToSecond(int x);
}
namespace OHOS {
    bool FuzzConvertMicrosecondToSecond(const uint8_t* data, size_t size)
    {
        if (data == nullptr || size < sizeof(int)) {
            return false;
        }

        int value = 0;
        errno_t err = memcpy_s(&value, sizeof(value), data, sizeof(value));
        if (err != 0) {
            return false;
        }
        
        float ret = ConvertMicrosecondToSecond(value);
        return ret != 0;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzConvertMicrosecondToSecond(data, size);
    return 0;
}