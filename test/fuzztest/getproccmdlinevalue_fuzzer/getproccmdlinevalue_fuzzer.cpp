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
#include "getproccmdlinevalue_fuzzer.h"

namespace OHOS {
    bool FuzzGetProcCmdlineValue(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        // value uses fixed char[256] buffer and passes sizeof(value) as length
        char value[256] = {0};
        // fixed name against fuzz buffer: covers match, miss and nested '=' paths
        (void)GetProcCmdlineValue("root", str.c_str(), value, sizeof(value));
        (void)GetProcCmdlineValue("test.name", str.c_str(), value, sizeof(value));
        // fuzz name against itself: covers name == buffer corner cases
        (void)GetProcCmdlineValue(str.c_str(), str.c_str(), value, sizeof(value));
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetProcCmdlineValue(data, size);
    return 0;
}
