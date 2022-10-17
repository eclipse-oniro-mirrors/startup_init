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

#include "systemtraversalparameter_fuzzer.h"

#include <string>
#include "fuzz_utils.h"
#include "init.h"
#include "securec.h"
#include "init_param.h"

static void FakeShowParam(ParamHandle handle, void *cookie)
{
    Cookie *nameAndValue = reinterpret_cast<Cookie*>(cookie);
    char *name = nameAndValue->data;
    unsigned int nameLen = nameAndValue->size;
    char value[PARAM_CONST_VALUE_LEN_MAX] = {0};
    SystemGetParameterName(handle, name, nameLen);
    uint32_t size = PARAM_CONST_VALUE_LEN_MAX;
    SystemGetParameterValue(handle, value, &size);
    printf("\t%s = %s \n", name, value);
}

namespace OHOS {
    bool FuzzSystemTraversalParameter(const uint8_t* data, size_t size)
    {
        bool result = false;
        CloseStdout();
        Cookie instance = {0, nullptr};
        Cookie *cookie = &instance;
        if (size == 0) {
            return false;
        }
        if (size > PARAM_CONST_VALUE_LEN_MAX + PARAM_NAME_LEN_MAX) {
            return true;
        }
        cookie->data = static_cast<char *>(malloc(size));
        if (cookie->data == nullptr) {
            return true;
        }
        cookie->size = size;
        std::string str(reinterpret_cast<const char*>(data), size - 1);
        int ret = memcpy_s(cookie->data, size, str.c_str(), size);
        if (ret != EOK) {
            return false;
        }
        if (!SystemTraversalParameter(nullptr, FakeShowParam, reinterpret_cast<void*>(cookie))) {
            result = true;
        }
        free(cookie->data);
        cookie->data = nullptr;
        cookie->size = 0;
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzSystemTraversalParameter(data, size);
    return 0;
}
