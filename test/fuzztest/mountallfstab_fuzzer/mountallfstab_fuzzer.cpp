/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mountallfstab_fuzzer.h"

#include <iostream>
#include "fs_manager/fs_manager.h"

namespace OHOS {
    bool FuzzMountAllWithFstabFile(const uint8_t* data, size_t size)
    {
        bool result = false;
        FILE *pFile = nullptr;
        pFile = fopen("fstab.test", "w+");
        if (pFile == nullptr) {
            std::cout << "[fuzz] open file fstab.test failed";
            return false;
        }
        if (fwrite(data, 1, size, pFile) != size) {
            std::cout << "[fuzz] write data to fstab.test failed";
            (void)fclose(pFile);
            return false;
        }
        (void)fclose(pFile);
        CloseStdout();
        if (!MountAllWithFstabFile("fstab.test", false)) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzMountAllWithFstabFile(data, size);
    return 0;
}
