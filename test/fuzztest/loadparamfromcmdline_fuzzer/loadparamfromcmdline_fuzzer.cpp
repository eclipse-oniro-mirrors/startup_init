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

#include "init_utils.h"
#include "param_manager.h"
#include <string>
#include "loadparamfromcmdline_fuzzer.h"

namespace OHOS {
    void CreateFuzzTestFile(const char *fileName, const char *data)
    {
        CheckAndCreateDir(fileName);
        FILE *tmpFile = fopen(fileName, "wr");
        if (tmpFile != nullptr) {
            fprintf(tmpFile, "%s", data);
            (void)fflush(tmpFile);
            fclose(tmpFile);
        }
    }

    bool FuzzLoadParamFromCmdLine(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        CreateFuzzTestFile(BOOT_CMD_LINE, str.c_str());
        LoadParamFromCmdLine();
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzLoadParamFromCmdLine(data, size);
    return 0;
}