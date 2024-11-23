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

#include "receivefds_fuzzer.h"
#include <string>
#include "securec.h"
#include "fd_holder_internal.h"

namespace OHOS {
    const uint8_t* BASE_DATA = nullptr;
    size_t g_baseSize = 0;
    size_t g_basePos;

    template <class T> T GetData()
    {
        T object{};
        size_t objectSize = sizeof(object);
        if ((BASE_DATA == nullptr) || (objectSize > g_baseSize - g_basePos)) {
            return object;
        }
        errno_t ret = memcpy_s(&object, objectSize, BASE_DATA + g_basePos, objectSize);
        if (ret != EOK) {
            return {};
        }
        g_basePos += objectSize;
        return object;
    }

    bool FuzzReceiveFds(const uint8_t* data, size_t size)
    {
        bool result = false;
        BASE_DATA = data;
        g_baseSize = size;
        g_basePos = 0;
        if (size > sizeof(struct iovec)) {
            struct iovec iovec = GetData<struct iovec>();
            if (!ReceiveFds(1, iovec, nullptr, false, nullptr)) {
                result = true;
            }
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzReceiveFds(data, size);
    return 0;
}
