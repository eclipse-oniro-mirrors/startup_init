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

#include "cmdserviceinit_fuzzer.h"
#include <string>
#include "fuzz_utils.h"
#include "control_fd.h"

static void Func(uint16_t type, const char *serviceCmd, const void *context)
{
    printf("type is:%d, serviceCmd is:%s\n", type, serviceCmd);
}

namespace OHOS {
    bool FuzzCmdServiceInit(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        CloseStdout();
        CmdServiceInit(str.c_str(), Func, LE_GetDefaultLoop());
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzCmdServiceInit(data, size);
    return 0;
}