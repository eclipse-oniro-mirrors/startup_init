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

#include "getdevicenodepermissions_fuzzer.h"
#include <string>
#include "ueventd_read_cfg.h"

namespace OHOS {
    bool FuzzGetDeviceNodePermissions(const uint8_t* data, size_t size)
    {
        if ((data == nullptr) || (size < sizeof(char) + sizeof(uid_t) + sizeof(gid_t) + sizeof(mode_t))) {
            return false;
        }
        bool result = false;
        unsigned int offset = 0;
        const char *devNode = reinterpret_cast<const char*>(data + offset);
        offset += sizeof(char);
        uid_t *uid = reinterpret_cast<uid_t*>(const_cast<uint8_t *>(data + offset));
        offset += sizeof(uid_t);
        gid_t *gid = reinterpret_cast<gid_t*>(const_cast<uint8_t *>(data + offset));
        offset += sizeof(gid_t);
        mode_t *mode = reinterpret_cast<mode_t*>(const_cast<uint8_t *>(data + offset));
        if (GetDeviceNodePermissions(devNode, uid, gid, mode) != 0) {
            result = true;
        };
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetDeviceNodePermissions(data, size);
    return 0;
}