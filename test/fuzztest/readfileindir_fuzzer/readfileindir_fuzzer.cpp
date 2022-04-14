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

#include "readfileindir_fuzzer.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "init.h"
#include "init_utils.h"

static int FakeParser(const char *testFile, void *context)
{
    UNUSED(context);
    std::ifstream fileStream;
    fileStream.open(testFile);
    std::vector<std::string> fileText;
    std::string textLine;
    while (getline(fileStream, textLine)) {
        fileText.push_back(textLine);
    }
    fileStream.close();
    std::vector<std::string>().swap(fileText);
    return 0;
}

namespace OHOS {
    bool FuzzReadFileInDir(const uint8_t* data, size_t size)
    {
        bool result = false;
        FILE *pFile = nullptr;
        pFile = fopen("ReadFileInDir.test", "w+");
        if (pFile == nullptr) {
            std::cout << "[fuzz] open file ReadFileInDir.test failed";
            return false;
        }
        if (fwrite(data, 1, size, pFile) != size) {
            std::cout << "[fuzz] write data to ReadFileInDir.test failed";
            (void)fclose(pFile);
            return false;
        }
        (void)fclose(pFile);
        if (!ReadFileInDir("/data", ".test", FakeParser, NULL)) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzReadFileInDir(data, size);
    return 0;
}
