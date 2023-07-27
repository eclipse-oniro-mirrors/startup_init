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

#include "hookmgrtraversal_fuzzer.h"
#include <string>
#include "hookmgr.h"

static void HookTraversal(const HOOK_INFO *hookInfo, void *traversalCookie)
{
    printf("hook traversal test!\n");
}

namespace OHOS {
    bool FuzzHookMgrTraversal(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        HOOK_MGR *hookMgr = HookMgrCreate(str.c_str());
        HookMgrTraversal(hookMgr, nullptr, HookTraversal);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzHookMgrTraversal(data, size);
    return 0;
}