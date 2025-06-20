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

#include "addparamentry_fuzzer.h"
#include <string>
#include <memory>
#include "param_trie.h"
#include "param_manager.h"
#include "securec.h"
constexpr int DATA_SIZE = 2;
constexpr int INDEX_MAX = 1000;
constexpr int TYPE_MAX = 5;

namespace OHOS {
    bool FuzzAddParamEntry(const uint8_t* data, size_t size)
    {
        if (size < sizeof(uint32_t)) {
            return false;
        }

        size_t splitPos = size / DATA_SIZE;
        if (splitPos == 0) {
            splitPos = 1;
        }

        std::unique_ptr<char[]> name(new char[splitPos + 1]);
        if (memcpy_s(name.get(), splitPos + 1, data, splitPos) != 0) {
            return false;
        }
        name[splitPos] = '\0';

        size_t valueSize = size - splitPos;
        std::unique_ptr<char[]> value(new char[valueSize + 1]);
        if (memcpy_s(value.get(), valueSize + 1, data + splitPos, valueSize) != 0) {
            return false;
        }
        value[valueSize] = '\0';

        uint32_t index = 0;
        if (memcpy_s(&index, sizeof(uint32_t), data, sizeof(uint32_t)) != 0) {
            index = 0;
        }
        index %= INDEX_MAX;

        uint8_t type = 0;
        if (size > 0) {
            type = data[0] % TYPE_MAX;
        }

        return (AddParamEntry(index, type, name.get(), value.get()) == 0);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OHOS::FuzzAddParamEntry(data, size);
    return 0;
}