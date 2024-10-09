/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "syscheckparamexist_fuzzer.h"
#include <string>
#include "param_manager.h"

#define NAME_LEN_MAX 96

namespace OHOS {
    bool FuzzSysCheckParamExist(const uint8_t* data, size_t size)
    {
        if (size >= NAME_LEN_MAX) {
            return false;
        }
        
        std::string name(reinterpret_cast<const char*>(data), size);
        int ret = SysCheckParamExist(name.c_str());
        if (ret == 0) {
            return false;
        };
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzSysCheckParamExist(data, size);
    return 0;
}