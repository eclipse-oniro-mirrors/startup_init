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

#include "getparametercpp_fuzzer.h"
#include<string>
#include "parameters.h"

namespace OHOS {
    bool FuzzGetParameter(const uint8_t* data, size_t size)
    {
        bool result = false;
        const std::string def = "";
        std::string key(reinterpret_cast<const char*>(data), size);
        if (system::GetParameter(key,  def) != def) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetParameter(data, size);
    return 0;
}
