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

#include "hookmgradd_fuzzer.h"
#include <string>
#include "hookmgr.h"

static int TestHook(const HOOK_INFO *hookInfo, void *executionContext)
{
    printf("Test HookMgrAdd\n");
    return 0;
}

namespace OHOS {
    bool FuzzHookMgrAdd(const uint8_t* data, size_t size)
    {
        bool result = false;
        int offset = 0;
        int stage = *data;
        offset += sizeof(int);
        int prio = *(data + offset);

        if (!HookMgrAdd(NULL, stage, prio, TestHook)) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzHookMgrAdd(data, size);
    return 0;
}