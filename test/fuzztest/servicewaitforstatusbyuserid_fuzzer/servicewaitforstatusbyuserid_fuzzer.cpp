/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "servicewaitforstatusbyuserid_fuzzer.h"

#include <string>

#include "service_control.h"

namespace OHOS {
constexpr int32_t FUZZ_USER_ID = 100;

bool FuzzServiceWaitForStatusByUserId(const uint8_t *data, size_t size)
{
    std::string serviceName(reinterpret_cast<const char *>(data), size);
    return ServiceWaitForStatusByUserId(serviceName.c_str(), FUZZ_USER_ID, SERVICE_STARTED, 1) == 0;
}
} // namespace OHOS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::FuzzServiceWaitForStatusByUserId(data, size);
    return 0;
}
