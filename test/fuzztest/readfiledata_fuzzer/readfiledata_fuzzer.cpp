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

#include <cstdlib>

#include "init_utils.h"
#include "readfiledata_fuzzer.h"
#include "securec.h"

namespace OHOS {
    constexpr int FUZZ_PATH_MAX_LEN = 256;

    bool FuzzReadFileData(const uint8_t* data, size_t size)
    {
        // fixed buffer instead of std::string: keeps c_str/data off the assignment line (G.STD.04-CPP)
        char name[FUZZ_PATH_MAX_LEN] = {0};
        size_t copyLen = (size < sizeof(name) - 1) ? size : sizeof(name) - 1;
        if (copyLen > 0 && memcpy_s(name, sizeof(name) - 1, data, copyLen) != EOK) {
            return true;
        }
        // fuzz-controlled name hits the open-failed path
        char *fileBuf = ReadFileData(name);
        if (fileBuf != nullptr) {
            free(fileBuf);
        }
        // deterministic readable file hits the read-success path
        fileBuf = ReadFileData("/proc/version");
        if (fileBuf != nullptr) {
            free(fileBuf);
        }
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzReadFileData(data, size);
    return 0;
}
