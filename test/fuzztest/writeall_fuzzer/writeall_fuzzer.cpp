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

#include <string>
#include "init_utils.h"
#include "writeall_fuzzer.h"

namespace OHOS {
    bool FuzzGetParameterFromCmdLine(const uint8_t* data, size_t size)
    {
        CheckAndCreateDir("/data/init_fuzztest/FuzzTestData");
        int fd = open("/data/init_fuzztest/FuzzTestData", O_CREAT | O_RDWR, 0777);
        if (fd < 0) {
            return false;
        }
        std::string str(reinterpret_cast<const char*>(data), size);
        size_t ret = WriteAll(fd, str.c_str(), size);
        if (ret == 0) {
            close(fd);
            return false;
        };
        close(fd);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetParameterFromCmdLine(data, size);
    return 0;
}