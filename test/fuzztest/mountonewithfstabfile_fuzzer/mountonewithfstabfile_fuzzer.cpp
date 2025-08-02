/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "mountonewithfstabfile_fuzzer.h"
#include "securec.h"
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include "fs_manager.h"
#include <string>

namespace OHOS {
    bool FuzzMountOneWithFstabFile(const uint8_t* data, size_t size)
    {
        char fstabPath[] = "/tmp/fuzz_fstab_1";
        int fd = mkstemp(fstabPath);
        if (fd < 0) {
            return false;
        }

        std::string str(reinterpret_cast<const char*>(data), size);

        int ret = MountOneWithFstabFile(fstabPath, str.c_str(), false);
        unlink(fstabPath);
        if (ret == 0) {
            return true;
        }
        return false;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OHOS::FuzzMountOneWithFstabFile(data, size);
    return 0;
}