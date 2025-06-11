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

#include "initworkspace_fuzzer.h"
#include <string>
#include <memory>
#include "param_trie.h"
#include "param_manager.h"
#include "securec.h"
constexpr int DATA_SIZE = 2;

namespace OHOS {
    bool FuzzInitWorkSpace(const uint8_t* data, size_t size)
    {
        constexpr size_t minSize = sizeof(uint32_t) * DATA_SIZE;
        if (size < minSize) {
            return false;
        }

        uint32_t spaceSize = 0;
        if (memcpy_s(&spaceSize, sizeof(uint32_t), data, sizeof(uint32_t)) != 0) {
            return false;
        }
        data += sizeof(uint32_t);
        size -= sizeof(uint32_t);

        int onlyRead = 0;
        if (memcpy_s(&onlyRead, sizeof(int), data, sizeof(int)) != 0) {
            return false;
        }
        WorkSpace *workSpace = GetWorkSpace(0);

        return (InitWorkSpace(workSpace, onlyRead, spaceSize) == 0);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OHOS::FuzzInitWorkSpace(data, size);
    return 0;
}